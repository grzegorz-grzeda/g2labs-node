/* SPDX-License-Identifier: MIT */
/* System adapter: rebooting the Node, and the core's warnings in the log. */
#include "adapters.h"

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(core, LOG_LEVEL_INF);

void system_adapter_reboot(void *ctx)
{
	ARG_UNUSED(ctx);

	LOG_INF("rebooting");
	LOG_PANIC();
	sys_reboot(SYS_REBOOT_COLD);
}

void system_adapter_log_warning(void *ctx, const char *message)
{
	ARG_UNUSED(ctx);

	LOG_WRN("%s", message);
}
