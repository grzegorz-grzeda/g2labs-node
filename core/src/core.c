/* SPDX-License-Identifier: MIT */
#include <node/core.h>

#include "json.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static void format_id(char *id, const uint8_t mac[NODE_MAC_LEN])
{
	static const char hex[] = "0123456789abcdef";

	for (size_t i = 0; i < NODE_MAC_LEN; i++) {
		id[2 * i] = hex[mac[i] >> 4];
		id[2 * i + 1] = hex[mac[i] & 0x0f];
	}
	id[NODE_ID_LEN] = '\0';
}

void node_core_init(struct node_core *core, const struct node_ports *ports,
		    const struct node_info *info)
{
	memset(core, 0, sizeof(*core));
	core->ports = ports;
	core->info = *info;
	core->wifi_state = NODE_WIFI_IDLE;
	core->broker_state = NODE_BROKER_IDLE;
	core->broker.port = NODE_BROKER_DEFAULT_PORT;
	format_id(core->id, info->mac);
}

/* The Broker has just become unreachable: start counting towards failover. */
static void start_failover_period(struct node_core *core)
{
	uint32_t period = core->info.broker_failover_ms != 0 ? core->info.broker_failover_ms
							      : NODE_BROKER_FAILOVER_TIMEOUT_MS;

	core->broker_outage = true;
	core->broker_failover_deadline_ms = core->now_ms + period;
}

/* g2labs/<node-id>/<leaf>: every topic of the Node's own. */
#define TOPIC_MAX (sizeof("g2labs//availability") + NODE_ID_LEN)

static void node_topic(const struct node_core *core, char *topic, const char *leaf)
{
	snprintf(topic, TOPIC_MAX, "g2labs/%s/%s", core->id, leaf);
}

static void publish(struct node_core *core, const char *leaf, const char *payload, bool retained)
{
	char topic[TOPIC_MAX];

	node_topic(core, topic, leaf);
	(void)core->ports->mqtt_publish(core->ports->ctx, topic, payload, strlen(payload),
					retained);
}

static void broker_connect(struct node_core *core)
{
	const struct node_broker *b = &core->broker;
	char will_topic[TOPIC_MAX];

	node_topic(core, will_topic, "availability");

	const struct node_mqtt_connect params = {
		.host = b->host,
		.port = b->port,
		.client_id = core->id,
		.username = b->username[0] != '\0' ? b->username : NULL,
		.password = b->password[0] != '\0' ? b->password : NULL,
		.will_topic = will_topic,
		.will_payload = "offline",
		.will_retained = true,
	};

	(void)core->ports->mqtt_connect(core->ports->ctx, &params);
	core->broker_state = NODE_BROKER_CONNECTING;
	core->broker_timeout_ms = core->now_ms + NODE_BROKER_CONNECT_TIMEOUT_MS;
}

/* Close or abandon the Broker connection; with `retry`, connect again later. */
static void broker_disconnect(struct node_core *core, bool retry)
{
	if (core->broker_state == NODE_BROKER_CONNECTED) {
		/* A clean disconnect doesn't fire the last will. */
		publish(core, "availability", "offline", true);
	}
	(void)core->ports->mqtt_disconnect(core->ports->ctx);
	core->broker_state = retry ? NODE_BROKER_RETRY_WAIT : NODE_BROKER_IDLE;
	core->broker_timeout_ms = core->now_ms + NODE_BROKER_RETRY_INTERVAL_MS;
}

/* Room for the status JSON with every string at its longest, fully escaped. */
#define STATUS_MAX 512

static void publish_status(struct node_core *core)
{
	char payload[STATUS_MAX];
	struct json_writer w = {.buf = payload, .size = sizeof(payload)};
	int rssi;

	json_putc(&w, '{');
	json_key(&w, "node_id");
	json_string(&w, core->id);
	if (core->friendly_name[0] != '\0') {
		json_key(&w, "name");
		json_string(&w, core->friendly_name);
	}
	json_key(&w, "fw_version");
	json_string(&w, core->info.fw_version);
	json_key(&w, "uptime_s");
	json_unsigned(&w, (unsigned long)(core->now_ms / 1000));
	json_key(&w, "network");
	json_string(&w, core->wifi_ssid);
	json_key(&w, "rssi");
	if (core->ports->wifi_rssi(core->ports->ctx, &rssi) == 0) {
		json_int(&w, rssi);
	} else {
		json_raw(&w, "null");
	}
	json_key(&w, "free_heap");
	json_unsigned(&w, (unsigned long)core->ports->free_memory(core->ports->ctx));
	json_putc(&w, '}');

	if (!w.overflow) {
		publish(core, "status", payload, false);
	}
	core->broker_timeout_ms = core->now_ms + NODE_STATUS_INTERVAL_MS;
}

/*
 * The command name from a command payload, {"command": "<name>"} plus any
 * members the Node doesn't know. `name` holds NODE_COMMAND_MAX_LEN + 1.
 */
static bool parse_command(const char *payload, size_t len, char *name)
{
	struct json_reader r = {.p = payload, .end = payload + len};
	char key[NODE_COMMAND_MAX_LEN + 1];
	bool found = false;

	if (!json_expect(&r, '{')) {
		return false;
	}
	if (!json_expect(&r, '}')) {
		do {
			if (!json_read_string(&r, key) || !json_expect(&r, ':')) {
				return false;
			}
			if (strcmp(key, "command") == 0) {
				json_skip_space(&r);
				if (found || !json_read_string(&r, name)) {
					return false;
				}
				found = true;
			} else if (!json_skip_value(&r, 1)) {
				return false;
			}
		} while (json_expect(&r, ','));
		if (!json_expect(&r, '}')) {
			return false;
		}
	}
	json_skip_space(&r);
	return found && r.p == r.end;
}

/* Room for a warning naming an unknown command; a long name is cut short. */
#define WARNING_MAX 160

static void warn(struct node_core *core, const char *message)
{
	core->ports->log_warning(core->ports->ctx, message);
}

static void warn_unknown_command(struct node_core *core, const char *name)
{
	char message[WARNING_MAX];
	struct json_writer w = {.buf = message, .size = sizeof(message)};

	json_raw(&w, "ignored unknown command ");
	json_string(&w, name);
	warn(core, message);
}

static void on_command(struct node_core *core, const struct node_event *event)
{
	char name[NODE_COMMAND_MAX_LEN + 1];

	if (core->broker_state != NODE_BROKER_CONNECTED) {
		/* Queued before the connection went: not meant for now. */
		warn(core, "ignored a command from a closed Broker connection");
		return;
	}
	if (event->command.retained) {
		/* A retained Reboot would reboot the Node on every connect. */
		warn(core, "ignored a retained command");
		return;
	}
	/* len over the maximum would be an adapter bug; don't read past payload. */
	if (event->command.truncated || event->command.len > NODE_COMMAND_MAX_LEN ||
	    !parse_command(event->command.payload, event->command.len, name)) {
		warn(core, "ignored a malformed command");
		return;
	}
	if (strcmp(name, "publish_status") == 0) {
		publish_status(core);
	} else if (strcmp(name, "reboot") == 0) {
		/* Say offline and leave cleanly; the reboot waits for it to get out. */
		core->reboot_pending = true;
		core->reboot_at_ms = core->now_ms + NODE_REBOOT_DELAY_MS;
		broker_disconnect(core, false);
	} else {
		warn_unknown_command(core, name);
	}
}

static void subscribe_commands(struct node_core *core)
{
	char topic[TOPIC_MAX];

	node_topic(core, topic, "command");
	if (core->ports->mqtt_subscribe_commands(core->ports->ctx, topic) != 0) {
		/* The next connection tries again. */
		warn(core, "can't subscribe to commands on this Broker connection");
	}
}

static void on_broker_connected(struct node_core *core)
{
	if (core->broker_state == NODE_BROKER_IDLE) {
		return;
	}
	core->broker_state = NODE_BROKER_CONNECTED;
	core->broker_outage = false;
	publish(core, "availability", "online", true);
	subscribe_commands(core);
	publish_status(core);
}

static void on_broker_disconnected(struct node_core *core)
{
	if (core->broker_state == NODE_BROKER_CONNECTED) {
		start_failover_period(core);
	}
	if (core->broker_state == NODE_BROKER_CONNECTING ||
	    core->broker_state == NODE_BROKER_CONNECTED) {
		core->broker_state = NODE_BROKER_RETRY_WAIT;
		core->broker_timeout_ms = core->now_ms + NODE_BROKER_RETRY_INTERVAL_MS;
	}
}

/* Connect to the Broker when there is one and Wi-Fi is up. */
static void broker_update(struct node_core *core)
{
	bool wanted = core->started && !core->reboot_pending &&
		      core->wifi_state == NODE_WIFI_CONNECTED && core->broker.host[0] != '\0';

	if (wanted && core->broker_state == NODE_BROKER_IDLE) {
		if (!core->broker_outage) {
			start_failover_period(core);
		}
		broker_connect(core);
	} else if (!wanted && core->broker_state != NODE_BROKER_IDLE) {
		broker_disconnect(core, false);
	}
}

/* strlen, but stops counting at max + 1 (strnlen isn't ISO C). */
static size_t bounded_strlen(const char *s, size_t max)
{
	size_t len = 0;

	while (len <= max && s[len] != '\0') {
		len++;
	}
	return len;
}

static bool is_hex_digit(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool is_valid_password(const char *password)
{
	size_t len = bounded_strlen(password, NODE_PASSWORD_MAX_LEN);

	if (len == 0) {
		return true; /* open network */
	}
	if (len < NODE_PASSWORD_MIN_LEN || len > NODE_PASSWORD_MAX_LEN) {
		return false;
	}
	if (len == NODE_PASSWORD_MAX_LEN) {
		/* 64 characters is only valid as a raw key in hex. */
		for (size_t i = 0; i < len; i++) {
			if (!is_hex_digit(password[i])) {
				return false;
			}
		}
	}
	return true;
}

static bool is_valid_ssid(const char *ssid)
{
	size_t len = bounded_strlen(ssid, NODE_SSID_MAX_LEN);

	return len >= 1 && len <= NODE_SSID_MAX_LEN;
}

static int find_known_network(const struct node_core *core, const char *ssid)
{
	for (size_t i = 0; i < core->known_network_count; i++) {
		if (strcmp(core->known_networks[i].ssid, ssid) == 0) {
			return (int)i;
		}
	}
	return -1;
}

/* Not on a network and not trying one; the next scan is due after `after_ms`. */
static void go_idle(struct node_core *core, int64_t after_ms)
{
	core->wifi_state = NODE_WIFI_IDLE;
	core->wifi_timeout_ms = core->now_ms + after_ms;
}

/* Asked to join and waiting to hear back. */
static bool is_join_in_flight(const struct node_core *core)
{
	return core->wifi_state == NODE_WIFI_JOINING || core->wifi_state == NODE_WIFI_AWAITING_IP;
}

/* The Known Network being joined or joined, or NULL if it was removed. */
static const struct node_known_network *current_known_network(const struct node_core *core)
{
	int i = find_known_network(core, core->wifi_ssid);

	return i >= 0 ? &core->known_networks[i] : NULL;
}

static void scan_if_idle(struct node_core *core)
{
	if (!core->started || core->wifi_state != NODE_WIFI_IDLE ||
	    core->known_network_count == 0) {
		return;
	}

	for (size_t i = 0; i < NODE_KNOWN_NETWORKS_MAX; i++) {
		core->heard_rssi[i] = NODE_RSSI_NOT_HEARD;
	}
	if (core->ports->wifi_scan(core->ports->ctx) != 0) {
		/* Likely still busy leaving a network: try again soon. */
		go_idle(core, NODE_WIFI_RETRY_INTERVAL_MS);
		return;
	}
	core->wifi_state = NODE_WIFI_SCANNING;
	core->wifi_timeout_ms = core->now_ms + NODE_WIFI_SCAN_TIMEOUT_MS;
}

/* Leave the network being tried and look for another. */
static void give_up_network(struct node_core *core)
{
	(void)core->ports->wifi_disconnect(core->ports->ctx);
	strcpy(core->wifi_avoid_ssid, core->wifi_ssid);
	go_idle(core, 0);
	scan_if_idle(core);
}

/* Ask to join the current Known Network, within the join deadline. */
static void request_join(struct node_core *core)
{
	const struct node_known_network *kn = current_known_network(core);

	if (kn == NULL ||
	    core->ports->wifi_connect(core->ports->ctx, kn->ssid, kn->password) != 0) {
		give_up_network(core);
		return;
	}
	core->wifi_state = NODE_WIFI_JOINING;
}

/* Start trying for kn: it has the reconnect timeout to associate. */
static void join(struct node_core *core, const struct node_known_network *kn)
{
	strcpy(core->wifi_ssid, kn->ssid);
	core->wifi_timeout_ms = core->now_ms + NODE_WIFI_RECONNECT_TIMEOUT_MS;
	request_join(core);
}

/* Index of the strongest heard Known Network, skipping `skip`; -1 if none. */
static int strongest_heard(const struct node_core *core, int skip)
{
	int best = -1;

	for (size_t i = 0; i < core->known_network_count; i++) {
		if ((int)i != skip && core->heard_rssi[i] != NODE_RSSI_NOT_HEARD &&
		    (best < 0 || core->heard_rssi[i] > core->heard_rssi[best])) {
			best = (int)i;
		}
	}
	return best;
}

static void join_strongest_heard(struct node_core *core)
{
	int best = strongest_heard(core, find_known_network(core, core->wifi_avoid_ssid));

	if (best < 0) {
		best = strongest_heard(core, -1);
	}

	if (best < 0) {
		go_idle(core, NODE_WIFI_RESCAN_INTERVAL_MS);
		return;
	}

	/* A network chosen by a scan gets a fresh failover period. */
	core->broker_outage = false;
	join(core, &core->known_networks[best]);
}

static void on_network_heard(struct node_core *core, const char *ssid, int rssi)
{
	int i = find_known_network(core, ssid);

	if (i >= 0 && rssi > core->heard_rssi[i]) {
		core->heard_rssi[i] = rssi;
	}
}

void node_core_handle_event(struct node_core *core, const struct node_event *event)
{
	switch (event->type) {
	case NODE_EVENT_WIFI_NETWORK_HEARD:
		if (core->wifi_state == NODE_WIFI_SCANNING) {
			on_network_heard(core, event->network_heard.ssid, event->network_heard.rssi);
		}
		break;
	case NODE_EVENT_WIFI_SCAN_DONE:
		if (core->wifi_state == NODE_WIFI_SCANNING) {
			join_strongest_heard(core);
		}
		break;
	case NODE_EVENT_WIFI_ASSOCIATED:
		if (core->wifi_state == NODE_WIFI_JOINING) {
			core->wifi_state = NODE_WIFI_AWAITING_IP;
			core->wifi_timeout_ms = core->now_ms + NODE_WIFI_DHCP_TIMEOUT_MS;
		}
		break;
	case NODE_EVENT_WIFI_CONNECTED:
		if (is_join_in_flight(core)) {
			core->wifi_state = NODE_WIFI_CONNECTED;
			core->wifi_avoid_ssid[0] = '\0';
		}
		break;
	case NODE_EVENT_WIFI_DISCONNECTED:
		if (core->wifi_state == NODE_WIFI_CONNECTED) {
			/* Ride out a glitch: rejoin the same network first. */
			const struct node_known_network *kn = current_known_network(core);

			if (kn != NULL) {
				join(core, kn);
			} else {
				go_idle(core, 0);
				scan_if_idle(core);
			}
		} else if (is_join_in_flight(core)) {
			core->wifi_state = NODE_WIFI_JOIN_RETRY_WAIT;
			core->wifi_retry_ms = core->now_ms + NODE_WIFI_RETRY_INTERVAL_MS;
		}
		break;
	case NODE_EVENT_BROKER_CONNECTED:
		on_broker_connected(core);
		break;
	case NODE_EVENT_BROKER_DISCONNECTED:
		on_broker_disconnected(core);
		break;
	case NODE_EVENT_COMMAND:
		on_command(core, event);
		break;
	}
	broker_update(core);
}

static void wifi_tick(struct node_core *core, int64_t now_ms)
{
	if (now_ms < core->wifi_timeout_ms) {
		if (core->wifi_state == NODE_WIFI_JOIN_RETRY_WAIT && now_ms >= core->wifi_retry_ms) {
			request_join(core);
		}
		return;
	}

	switch (core->wifi_state) {
	case NODE_WIFI_IDLE:
		scan_if_idle(core);
		break;
	case NODE_WIFI_SCANNING:
		join_strongest_heard(core);
		break;
	case NODE_WIFI_JOINING:
	case NODE_WIFI_AWAITING_IP:
	case NODE_WIFI_JOIN_RETRY_WAIT:
		give_up_network(core);
		break;
	case NODE_WIFI_CONNECTED:
		break;
	}
}

static void broker_tick(struct node_core *core, int64_t now_ms)
{
	if (now_ms < core->broker_timeout_ms) {
		return;
	}

	switch (core->broker_state) {
	case NODE_BROKER_IDLE:
		break;
	case NODE_BROKER_CONNECTING:
		broker_disconnect(core, true);
		break;
	case NODE_BROKER_RETRY_WAIT:
		broker_connect(core);
		break;
	case NODE_BROKER_CONNECTED:
		publish_status(core);
		break;
	}
}

/*
 * A Known Network with no way to the Broker strands the Node: after the
 * failover period, move to another Known Network. With only one, stay and keep
 * retrying the Broker.
 */
static void broker_failover_tick(struct node_core *core, int64_t now_ms)
{
	bool unreachable = core->broker_state == NODE_BROKER_CONNECTING ||
			   core->broker_state == NODE_BROKER_RETRY_WAIT;

	if (!unreachable || core->wifi_state != NODE_WIFI_CONNECTED ||
	    now_ms < core->broker_failover_deadline_ms) {
		return;
	}
	if (core->known_network_count < 2) {
		start_failover_period(core);
		return;
	}
	give_up_network(core);
	/* Drop the Broker attempt now, before broker_tick() retries it. */
	broker_update(core);
}

static void reboot_tick(struct node_core *core, int64_t now_ms)
{
	if (core->reboot_pending && now_ms >= core->reboot_at_ms) {
		core->reboot_at_ms = INT64_MAX;
		core->ports->reboot(core->ports->ctx);
	}
}

void node_core_tick(struct node_core *core, int64_t now_ms)
{
	core->now_ms = now_ms;
	reboot_tick(core, now_ms);
	wifi_tick(core, now_ms);
	broker_failover_tick(core, now_ms);
	broker_tick(core, now_ms);
	broker_update(core);
}

void node_core_start(struct node_core *core)
{
	core->started = true;
	scan_if_idle(core);
}

int node_core_add_known_network(struct node_core *core, const char *ssid, const char *password)
{
	if (!is_valid_ssid(ssid) || !is_valid_password(password)) {
		return -EINVAL;
	}

	int i = find_known_network(core, ssid);

	if (i < 0) {
		if (core->known_network_count >= NODE_KNOWN_NETWORKS_MAX) {
			return -ENOSPC;
		}
		i = (int)core->known_network_count++;
		core->heard_rssi[i] = NODE_RSSI_NOT_HEARD;
	}

	struct node_known_network *kn = &core->known_networks[i];

	memset(kn, 0, sizeof(*kn));
	strcpy(kn->ssid, ssid);
	strcpy(kn->password, password);
	scan_if_idle(core);
	return 0;
}

int node_core_remove_known_network(struct node_core *core, const char *ssid)
{
	int i = find_known_network(core, ssid);

	if (i < 0) {
		return -ENOENT;
	}

	size_t last = core->known_network_count - 1;
	size_t tail = last - (size_t)i;

	memmove(&core->known_networks[i], &core->known_networks[i + 1],
		tail * sizeof(core->known_networks[0]));
	memmove(&core->heard_rssi[i], &core->heard_rssi[i + 1], tail * sizeof(core->heard_rssi[0]));
	memset(&core->known_networks[last], 0, sizeof(core->known_networks[0]));
	core->heard_rssi[last] = NODE_RSSI_NOT_HEARD;
	core->known_network_count--;
	return 0;
}

const struct node_known_network *node_core_known_network(const struct node_core *core, size_t i)
{
	return i < core->known_network_count ? &core->known_networks[i] : NULL;
}

size_t node_core_known_network_count(const struct node_core *core)
{
	return core->known_network_count;
}

const char *node_core_current_network(const struct node_core *core)
{
	return core->wifi_state == NODE_WIFI_CONNECTED ? core->wifi_ssid : NULL;
}

const char *node_core_id(const struct node_core *core)
{
	return core->id;
}

const char *node_core_fw_version(const struct node_core *core)
{
	return core->info.fw_version;
}

static bool is_valid_broker(const struct node_broker *b)
{
	return bounded_strlen(b->host, NODE_BROKER_HOST_MAX_LEN) <= NODE_BROKER_HOST_MAX_LEN &&
	       bounded_strlen(b->username, NODE_BROKER_USERNAME_MAX_LEN) <=
		       NODE_BROKER_USERNAME_MAX_LEN &&
	       bounded_strlen(b->password, NODE_BROKER_PASSWORD_MAX_LEN) <=
		       NODE_BROKER_PASSWORD_MAX_LEN &&
	       b->port != 0 && (b->password[0] == '\0' || b->username[0] != '\0');
}

int node_core_set_broker(struct node_core *core, const struct node_broker *broker)
{
	if (!is_valid_broker(broker)) {
		return -EINVAL;
	}

	struct node_broker *b = &core->broker;
	bool same = strcmp(b->host, broker->host) == 0 && b->port == broker->port &&
		    strcmp(b->username, broker->username) == 0 &&
		    strcmp(b->password, broker->password) == 0;

	if (same) {
		return 0;
	}

	memset(b, 0, sizeof(*b));
	strcpy(b->host, broker->host);
	b->port = broker->port;
	strcpy(b->username, broker->username);
	strcpy(b->password, broker->password);

	if (core->broker_state != NODE_BROKER_IDLE) {
		broker_disconnect(core, false);
	}
	/* A different Broker gets a fresh failover period. */
	core->broker_outage = false;
	broker_update(core);
	return 0;
}

int node_core_set_friendly_name(struct node_core *core, const char *name)
{
	if (bounded_strlen(name, NODE_FRIENDLY_NAME_MAX_LEN) > NODE_FRIENDLY_NAME_MAX_LEN) {
		return -EINVAL;
	}
	memset(core->friendly_name, 0, sizeof(core->friendly_name));
	strcpy(core->friendly_name, name);
	return 0;
}

const char *node_core_friendly_name(const struct node_core *core)
{
	return core->friendly_name;
}

const struct node_broker *node_core_broker(const struct node_core *core)
{
	return &core->broker;
}

bool node_core_broker_connected(const struct node_core *core)
{
	return core->broker_state == NODE_BROKER_CONNECTED;
}
