/* SPDX-License-Identifier: MIT */
#include <zephyr/shell/shell.h>

#include <node/core.h>

#include "app.h"

static int cmd_node_version(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "%s", node_core_fw_version(app_core()));
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(node_cmds,
	SHELL_CMD_ARG(version, NULL, "Print the firmware version", cmd_node_version, 1, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(node, &node_cmds, "Node commands", NULL);
