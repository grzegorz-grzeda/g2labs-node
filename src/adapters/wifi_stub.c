/* SPDX-License-Identifier: MIT */
/* Placeholder Wi-Fi adapter until Known Networks are joined. */
#include "adapters.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wifi_adapter, LOG_LEVEL_INF);

int wifi_adapter_scan(void *ctx)
{
	ARG_UNUSED(ctx);

	LOG_WRN("Wi-Fi not available, scan ignored");
	return -ENOTSUP;
}

int wifi_adapter_connect(void *ctx, const char *ssid, const char *password)
{
	ARG_UNUSED(ctx);
	ARG_UNUSED(password);

	LOG_WRN("Wi-Fi not available, connect to %s ignored", ssid);
	return -ENOTSUP;
}
