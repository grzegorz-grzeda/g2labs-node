/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>

#include <zephyr/shell/shell.h>

#include <node/core.h>

#include "adapters/adapters.h"
#include "app.h"

static int cmd_node_version(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct node_core *core = app_core_lock();

	shell_print(sh, "%s", node_core_fw_version(core));
	app_core_unlock();
	return 0;
}

/* Persist the Known Networks after a change and tell the installer when it applies. */
static int save_and_report(const struct shell *sh, const struct node_core *core, const char *done)
{
	int err = settings_adapter_save_known_networks(core);

	if (err) {
		shell_error(sh, "%s, but saving to flash failed (%d); it is lost on reboot", done,
			    err);
		return err;
	}
	shell_print(sh, "%s", done);
	return 0;
}

static int cmd_network_add(const struct shell *sh, size_t argc, char **argv)
{
	const char *ssid = argv[1];
	const char *password = argc > 2 ? argv[2] : "";
	char done[64 + NODE_SSID_MAX_LEN];
	struct node_core *core = app_core_lock();
	int err = node_core_add_known_network(core, ssid, password);

	if (err == 0) {
		snprintf(done, sizeof(done), "Known Network \"%s\" saved", ssid);
		err = save_and_report(sh, core, done);
		if (node_core_current_network(core) != NULL) {
			shell_print(sh, "The Node stays on its current network while it is healthy");
		}
	} else if (err == -ENOSPC) {
		shell_error(sh, "Already %d Known Networks, the limit; remove one first",
			    NODE_KNOWN_NETWORKS_MAX);
	} else if (err == -EINVAL) {
		shell_error(sh, "SSID must be 1-%d bytes; password empty (open network), "
				"%d-%d characters, or a %d hex digit key",
			    NODE_SSID_MAX_LEN, NODE_PASSWORD_MIN_LEN, NODE_PASSWORD_MAX_LEN - 1,
			    NODE_PASSWORD_MAX_LEN);
	} else {
		shell_error(sh, "Adding Known Network failed (%d)", err);
	}
	app_core_unlock();
	return err;
}

static int cmd_network_remove(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);

	const char *ssid = argv[1];
	char done[64 + NODE_SSID_MAX_LEN];
	struct node_core *core = app_core_lock();
	const char *current = node_core_current_network(core);
	bool on_it = current != NULL && strcmp(current, ssid) == 0;
	int err = node_core_remove_known_network(core, ssid);

	if (err == 0) {
		snprintf(done, sizeof(done), "Known Network \"%s\" removed", ssid);
		err = save_and_report(sh, core, done);
		if (on_it) {
			shell_print(sh, "The Node stays on it until the link drops or it reboots");
		}
	} else {
		shell_error(sh, "No Known Network \"%s\"", ssid);
	}
	app_core_unlock();
	return err;
}

static int cmd_network_list(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct node_core *core = app_core_lock();
	size_t count = node_core_known_network_count(core);

	/* Passwords are never shown, only whether a network has one. */
	for (size_t i = 0; i < count; i++) {
		const struct node_known_network *kn = node_core_known_network(core, i);

		shell_print(sh, "%zu: %s (%s)", i + 1, kn->ssid,
			    kn->password[0] != '\0' ? "password set" : "open");
	}
	shell_print(sh, "%zu of %d Known Networks", count, NODE_KNOWN_NETWORKS_MAX);
	app_core_unlock();
	return 0;
}

static int cmd_network_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	char current[NODE_SSID_MAX_LEN + 1] = "";
	struct node_core *core = app_core_lock();
	const char *ssid = node_core_current_network(core);

	if (ssid != NULL) {
		strcpy(current, ssid);
	}
	app_core_unlock();

	if (current[0] == '\0') {
		shell_print(sh, "Offline");
		return 0;
	}

	int rssi;
	char ip[NET_IPV4_ADDR_LEN];
	int err = wifi_adapter_link_info(&rssi, ip, sizeof(ip));

	shell_print(sh, "Network: %s", current);
	if (err) {
		shell_print(sh, "RSSI and IP address unavailable (%d)", err);
	} else {
		shell_print(sh, "RSSI: %d dBm", rssi);
		shell_print(sh, "IP address: %s", ip);
	}
	return 0;
}

static int cmd_node_id(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	char mdns_name[WIFI_ADAPTER_MDNS_NAME_SIZE];
	struct node_core *core = app_core_lock();
	const char *name = node_core_friendly_name(core);

	shell_print(sh, "Node ID: %s", node_core_id(core));
	shell_print(sh, "Friendly name: %s", name[0] != '\0' ? name : "(none)");
	app_core_unlock();
	if (wifi_adapter_mdns_name(mdns_name, sizeof(mdns_name)) == 0) {
		shell_print(sh, "mDNS name: %s", mdns_name);
	}
	return 0;
}

static int set_friendly_name(const struct shell *sh, const char *name)
{
	struct node_core *core = app_core_lock();
	int err = node_core_set_friendly_name(core, name);

	if (err) {
		shell_error(sh, "The friendly name must be at most %d bytes",
			    NODE_FRIENDLY_NAME_MAX_LEN);
	} else if ((err = settings_adapter_save_friendly_name(core)) != 0) {
		shell_error(sh, "Friendly name set, but saving to flash failed (%d); "
				"it is lost on reboot", err);
	} else {
		shell_print(sh, "Friendly name %s; status reports it from the next message",
			    name[0] != '\0' ? "saved" : "cleared");
	}
	app_core_unlock();
	return err;
}

static int cmd_name_set(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);

	return set_friendly_name(sh, argv[1]);
}

static int cmd_name_clear(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return set_friendly_name(sh, "");
}

/* Apply changed Broker settings, persist them and say when they take effect. */
static int apply_broker(const struct shell *sh, struct node_core *core,
			const struct node_broker *broker)
{
	int err = node_core_set_broker(core, broker);

	if (err) {
		shell_error(sh, "A password needs a username; set one with: node broker login "
				"<username> <password>");
		return err;
	}
	err = settings_adapter_save_broker(core);
	if (err) {
		shell_error(sh, "Broker settings applied, but saving to flash failed (%d); "
				"they are lost on reboot", err);
		return err;
	}
	shell_print(sh, "Broker settings saved");
	if (broker->host[0] == '\0') {
		shell_print(sh, "No Broker: the Node stays off MQTT");
	} else if (node_core_current_network(core) != NULL) {
		shell_print(sh, "Reconnecting to the Broker with the new settings");
	} else {
		shell_print(sh, "They apply once the Node is on a Known Network");
	}
	return 0;
}

static int cmd_broker_set(const struct shell *sh, size_t argc, char **argv)
{
	unsigned long port = NODE_BROKER_DEFAULT_PORT;

	if (argc > 2) {
		char *end;

		port = strtoul(argv[2], &end, 10);
		if (*end != '\0' || port == 0 || port > UINT16_MAX) {
			shell_error(sh, "Port must be 1-%u", UINT16_MAX);
			return -EINVAL;
		}
	}
	if (strlen(argv[1]) > NODE_BROKER_HOST_MAX_LEN) {
		shell_error(sh, "Host must be at most %d bytes", NODE_BROKER_HOST_MAX_LEN);
		return -EINVAL;
	}

	struct node_core *core = app_core_lock();
	struct node_broker broker = *node_core_broker(core);

	strcpy(broker.host, argv[1]);
	broker.port = (uint16_t)port;
	int err = apply_broker(sh, core, &broker);

	app_core_unlock();
	return err;
}

static int cmd_broker_login(const struct shell *sh, size_t argc, char **argv)
{
	const char *username = argv[1];
	const char *password = argc > 2 ? argv[2] : "";

	if (strlen(username) > NODE_BROKER_USERNAME_MAX_LEN ||
	    strlen(password) > NODE_BROKER_PASSWORD_MAX_LEN) {
		shell_error(sh, "Username must be at most %d bytes, password at most %d",
			    NODE_BROKER_USERNAME_MAX_LEN, NODE_BROKER_PASSWORD_MAX_LEN);
		return -EINVAL;
	}

	struct node_core *core = app_core_lock();
	struct node_broker broker = *node_core_broker(core);

	strcpy(broker.username, username);
	strcpy(broker.password, password);
	int err = apply_broker(sh, core, &broker);

	app_core_unlock();
	return err;
}

static int cmd_broker_logout(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct node_core *core = app_core_lock();
	struct node_broker broker = *node_core_broker(core);

	broker.username[0] = '\0';
	broker.password[0] = '\0';
	int err = apply_broker(sh, core, &broker);

	app_core_unlock();
	return err;
}

static int cmd_broker_clear(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct node_core *core = app_core_lock();
	const struct node_broker broker = {.port = NODE_BROKER_DEFAULT_PORT};
	int err = apply_broker(sh, core, &broker);

	app_core_unlock();
	return err;
}

static int cmd_broker_show(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct node_core *core = app_core_lock();
	const struct node_broker *b = node_core_broker(core);

	if (b->host[0] == '\0') {
		shell_print(sh, "No Broker set");
	} else {
		shell_print(sh, "Broker: %s:%u", b->host, b->port);
		/* The password is never shown, only whether there is one. */
		if (b->username[0] != '\0') {
			shell_print(sh, "Username: %s (%s)", b->username,
				    b->password[0] != '\0' ? "password set" : "no password");
		} else {
			shell_print(sh, "Anonymous");
		}
		shell_print(sh, "%s", node_core_broker_connected(core) ? "Connected"
								      : "Not connected");
	}
	app_core_unlock();
	return 0;
}

/* A duration as "3d 2h 13m 05s", leaving out leading zero units. */
static void format_duration(char *buf, size_t len, int64_t ms)
{
	int64_t s = ms / 1000;
	int days = (int)(s / 86400);
	int hours = (int)(s / 3600 % 24);
	int minutes = (int)(s / 60 % 60);
	int seconds = (int)(s % 60);

	if (days > 0) {
		snprintf(buf, len, "%dd %dh %02dm %02ds", days, hours, minutes, seconds);
	} else if (hours > 0) {
		snprintf(buf, len, "%dh %02dm %02ds", hours, minutes, seconds);
	} else if (minutes > 0) {
		snprintf(buf, len, "%dm %02ds", minutes, seconds);
	} else {
		snprintf(buf, len, "%ds", seconds);
	}
}

static void print_wifi(const struct shell *sh, enum node_wifi_state wifi, const char *ssid)
{
	uint8_t mac[NODE_MAC_LEN];
	char mac_str[3 * NODE_MAC_LEN] = "unknown";

	if (wifi_adapter_mac(mac) == 0) {
		snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0],
			 mac[1], mac[2], mac[3], mac[4], mac[5]);
	}

	switch (wifi) {
	case NODE_WIFI_IDLE:
		shell_print(sh, "Wi-Fi: offline  MAC %s", mac_str);
		return;
	case NODE_WIFI_SCANNING:
		shell_print(sh, "Wi-Fi: scanning  MAC %s", mac_str);
		return;
	case NODE_WIFI_JOINING:
		shell_print(sh, "Wi-Fi: joining %s  MAC %s", ssid, mac_str);
		return;
	case NODE_WIFI_AWAITING_IP:
		shell_print(sh, "Wi-Fi: on %s, waiting for an IP address  MAC %s", ssid, mac_str);
		return;
	case NODE_WIFI_JOIN_RETRY_WAIT:
		shell_print(sh, "Wi-Fi: join of %s failed, retrying  MAC %s", ssid, mac_str);
		return;
	case NODE_WIFI_CONNECTED:
		break;
	}

	int rssi;
	char ip[NET_IPV4_ADDR_LEN];

	if (wifi_adapter_link_info(&rssi, ip, sizeof(ip)) == 0) {
		shell_print(sh, "Wi-Fi: on %s  RSSI %d dBm  IP %s  MAC %s", ssid, rssi, ip,
			    mac_str);
	} else {
		shell_print(sh, "Wi-Fi: on %s  RSSI and IP unavailable  MAC %s", ssid, mac_str);
	}
}

static void print_broker(const struct shell *sh, const struct node_broker *b,
			 const struct node_state *st)
{
	char took[32];
	char left[32];

	if (b->host[0] == '\0') {
		shell_print(sh, "Broker: none set");
		return;
	}
	switch (st->broker) {
	case NODE_BROKER_IDLE:
		shell_print(sh, "Broker: %s:%u, not connecting", b->host, b->port);
		break;
	case NODE_BROKER_CONNECTED:
		format_duration(took, sizeof(took), st->broker_connected_ms);
		shell_print(sh, "Broker: %s:%u connected %s", b->host, b->port, took);
		break;
	case NODE_BROKER_CONNECTING:
	case NODE_BROKER_RETRY_WAIT:
		if (st->broker == NODE_BROKER_CONNECTING && st->broker_unreachable_ms < 1000) {
			shell_print(sh, "Broker: %s:%u connecting", b->host, b->port);
			break;
		}
		format_duration(took, sizeof(took), st->broker_unreachable_ms);
		if (st->failover_in_ms >= 0) {
			format_duration(left, sizeof(left), st->failover_in_ms);
			shell_print(sh, "Broker: %s:%u unreachable %s, next Known Network in %s",
				    b->host, b->port, took, left);
		} else {
			shell_print(sh, "Broker: %s:%u unreachable %s", b->host, b->port, took);
		}
		break;
	}
}

static int cmd_node_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	char id[NODE_ID_LEN + 1];
	char name[NODE_FRIENDLY_NAME_MAX_LEN + 1];
	char ssid[NODE_SSID_MAX_LEN + 1];
	char version[32];
	struct node_broker broker;
	struct node_state st;
	struct node_core *core = app_core_lock();

	node_core_state(core, &st);
	snprintf(id, sizeof(id), "%s", node_core_id(core));
	snprintf(name, sizeof(name), "%s", node_core_friendly_name(core));
	snprintf(ssid, sizeof(ssid), "%s", st.ssid);
	snprintf(version, sizeof(version), "%s", node_core_fw_version(core));
	broker = *node_core_broker(core);
	app_core_unlock();
	memset(broker.password, 0, sizeof(broker.password));

	char mdns_name[WIFI_ADAPTER_MDNS_NAME_SIZE] = "unknown";
	char uptime[32];
	struct image_adapter_slot slots[IMAGE_ADAPTER_SLOTS];
	size_t count = image_adapter_list(slots);
	const struct image_adapter_slot *running = NULL;

	(void)wifi_adapter_mdns_name(mdns_name, sizeof(mdns_name));
	format_duration(uptime, sizeof(uptime), k_uptime_get());
	for (size_t i = 0; i < count; i++) {
		if (slots[i].active) {
			running = &slots[i];
		}
	}

	shell_print(sh, "Version: %s (%s)", version, CONFIG_BOARD_TARGET);
	shell_print(sh, "Node ID: %s  name: %s  mDNS: %s", id, name[0] != '\0' ? name : "(none)",
		    mdns_name);
	shell_print(sh, "Uptime: %s  last reset: %s", uptime, system_adapter_reset_cause());
	shell_print(sh, "Free heap: %zu B", memory_adapter_free(NULL));
	print_wifi(sh, st.wifi, ssid);
	print_broker(sh, &broker, &st);
	if (running != NULL) {
		shell_print(sh, "Image: slot %d %s %s", running->slot, running->version,
			    running->confirmed ? "confirmed" : "not confirmed yet");
	} else {
		shell_print(sh, "Image: unknown");
	}
	if (st.reboot_pending) {
		shell_print(sh, "Reboot pending");
	}
	return 0;
}

static int cmd_image_list(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct image_adapter_slot slots[IMAGE_ADAPTER_SLOTS];
	size_t count = image_adapter_list(slots);

	for (size_t i = 0; i < count; i++) {
		const struct image_adapter_slot *s = &slots[i];
		char hash[2 * IMAGE_ADAPTER_HASH_LEN + 1];

		for (size_t j = 0; j < IMAGE_ADAPTER_HASH_LEN; j++) {
			snprintf(&hash[2 * j], 3, "%02x", s->hash[j]);
		}
		shell_print(sh, "Slot %d: %s%s%s%s%s", s->slot, s->version,
			    s->active ? " active" : "", s->confirmed ? " confirmed" : "",
			    s->pending ? " pending" : "", s->permanent ? " permanent" : "");
		shell_print(sh, "  hash %s", hash);
	}
	if (count == 0) {
		shell_print(sh, "No images found");
	}
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(image_cmds,
	SHELL_CMD_ARG(list, NULL, "List the images in both slots and their state", cmd_image_list,
		      1, 0),
	SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(name_cmds,
	SHELL_CMD_ARG(set, NULL, "Set the friendly name: set <name>", cmd_name_set, 2, 0),
	SHELL_CMD_ARG(clear, NULL, "Clear the friendly name", cmd_name_clear, 1, 0),
	SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(broker_cmds,
	SHELL_CMD_ARG(set, NULL, "Set the Broker: set <host> [port, default 1883]",
		      cmd_broker_set, 2, 1),
	SHELL_CMD_ARG(login, NULL, "Log in to the Broker: login <username> [password]",
		      cmd_broker_login, 2, 1),
	SHELL_CMD_ARG(logout, NULL, "Connect to the Broker anonymously", cmd_broker_logout, 1,
		      0),
	SHELL_CMD_ARG(clear, NULL, "Forget the Broker", cmd_broker_clear, 1, 0),
	SHELL_CMD_ARG(show, NULL, "Show the Broker settings (password hidden) and connection",
		      cmd_broker_show, 1, 0),
	SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(network_cmds,
	SHELL_CMD_ARG(add, NULL, "Add a Known Network: add <ssid> [password]",
		      cmd_network_add, 2, 1),
	SHELL_CMD_ARG(remove, NULL, "Remove a Known Network: remove <ssid>",
		      cmd_network_remove, 2, 0),
	SHELL_CMD_ARG(list, NULL, "List Known Networks (passwords hidden)", cmd_network_list, 1,
		      0),
	SHELL_CMD_ARG(status, NULL, "Show the current Known Network, RSSI and IP address",
		      cmd_network_status, 1, 0),
	SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(node_cmds,
	SHELL_CMD_ARG(status, NULL, "Show everything about the Node's state at a glance",
		      cmd_node_status, 1, 0),
	SHELL_CMD_ARG(version, NULL, "Print the firmware version", cmd_node_version, 1, 0),
	SHELL_CMD_ARG(id, NULL, "Print the Node ID, friendly name and mDNS name", cmd_node_id, 1, 0),
	SHELL_CMD(name, &name_cmds, "Friendly name", NULL),
	SHELL_CMD(network, &network_cmds, "Known Networks", NULL),
	SHELL_CMD(broker, &broker_cmds, "Broker connection", NULL),
	SHELL_CMD(image, &image_cmds, "Firmware images", NULL),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(node, &node_cmds, "Node commands", NULL);
