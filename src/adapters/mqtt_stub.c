/* SPDX-License-Identifier: MIT */
/* Placeholder MQTT adapter until the Broker connection exists. */
#include "adapters.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mqtt_adapter, LOG_LEVEL_INF);

int mqtt_adapter_publish(void *ctx, const char *topic, const void *payload, size_t len,
			 bool retained)
{
	ARG_UNUSED(ctx);
	ARG_UNUSED(payload);
	ARG_UNUSED(retained);

	LOG_WRN("MQTT not available, dropping %zu bytes to %s", len, topic);
	return -ENOTSUP;
}
