/* SPDX-License-Identifier: MIT */
#include <node/core.h>

void node_core_start(struct node_core *core, const struct node_ports *ports,
		     const struct node_info *info)
{
	core->ports = ports;
	core->info = *info;
}

const char *node_core_fw_version(const struct node_core *core)
{
	return core->info.fw_version;
}
