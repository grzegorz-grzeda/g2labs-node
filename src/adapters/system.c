/* SPDX-License-Identifier: MIT */
/*
 * System adapter: rebooting the Node, why it last reset, and the core's
 * warnings in the log.
 */
#include "adapters.h"

#include <zephyr/drivers/hwinfo.h>
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

const char *system_adapter_reset_cause(void)
{
	/* The causes the ESP32 hwinfo driver reports, one at a time. */
	static const struct {
		uint32_t bit;
		const char *name;
	} causes[] = {
		{RESET_POR, "power-on"},
		{RESET_PIN, "reset pin"},
		{RESET_SOFTWARE, "software"},
		{RESET_WATCHDOG, "watchdog"},
		{RESET_CPU_LOCKUP, "panic"},
		{RESET_BROWNOUT, "brown-out"},
		{RESET_LOW_POWER_WAKE, "deep sleep wake"},
	};
	uint32_t cause;

	if (hwinfo_get_reset_cause(&cause) != 0) {
		return "unavailable";
	}
	for (size_t i = 0; i < ARRAY_SIZE(causes); i++) {
		if (cause & causes[i].bit) {
			return causes[i].name;
		}
	}
	return "unknown";
}
