/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <stdio.h>
#include <string.h>

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
	SHELL_CMD_ARG(version, NULL, "Print the firmware version", cmd_node_version, 1, 0),
	SHELL_CMD(network, &network_cmds, "Known Networks", NULL),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(node, &node_cmds, "Node commands", NULL);
