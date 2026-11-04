/* SPDX-License-Identifier: MIT */
#include <node/core.h>

#include <errno.h>
#include <string.h>

void node_core_init(struct node_core *core, const struct node_ports *ports,
		    const struct node_info *info)
{
	memset(core, 0, sizeof(*core));
	core->ports = ports;
	core->info = *info;
	core->wifi_state = NODE_WIFI_IDLE;
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
	}
}

void node_core_tick(struct node_core *core, int64_t now_ms)
{
	core->now_ms = now_ms;

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

const char *node_core_fw_version(const struct node_core *core)
{
	return core->info.fw_version;
}
