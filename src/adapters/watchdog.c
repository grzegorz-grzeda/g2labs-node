/* SPDX-License-Identifier: MIT */
/*
 * Watchdog adapter: the SoC's hardware watchdog (the `watchdog0` alias),
 * which resets the Node when it isn't fed in time. A hung image that was
 * never confirmed is then reverted by MCUboot.
 */
#include "adapters.h"

#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(watchdog_adapter, LOG_LEVEL_INF);

static const struct device *const wdt = DEVICE_DT_GET(DT_ALIAS(watchdog0));
static int channel = -1;

int watchdog_adapter_init(uint32_t timeout_ms)
{
	const struct wdt_timeout_cfg cfg = {
		.window = {.min = 0, .max = timeout_ms},
		.flags = WDT_FLAG_RESET_SOC,
	};
	int err;

	if (!device_is_ready(wdt)) {
		LOG_ERR("watchdog not ready; a hung Node won't reset itself");
		return -ENODEV;
	}
	channel = wdt_install_timeout(wdt, &cfg);
	if (channel < 0) {
		LOG_ERR("watchdog timeout not installed: %d", channel);
		return channel;
	}
	/* Pause while a debugger halts the CPU, where the driver supports it. */
	err = wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
	if (err) {
		LOG_ERR("watchdog not started: %d", err);
		channel = -1;
		return err;
	}
	return 0;
}

void watchdog_adapter_feed(void)
{
	if (channel >= 0) {
		wdt_feed(wdt, channel);
	}
}
