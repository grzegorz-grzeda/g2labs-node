/* SPDX-License-Identifier: MIT */
#include <zephyr/app_version.h>
#include <zephyr/logging/log.h>

#include <node/core.h>

#include "adapters/adapters.h"
#include "app.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static const struct node_ports ports = {
	.ctx = NULL,
	.mqtt_publish = mqtt_adapter_publish,
	.wifi_scan = wifi_adapter_scan,
	.wifi_connect = wifi_adapter_connect,
	.image_confirm = image_adapter_confirm,
};

static struct node_core core;

const struct node_core *app_core(void)
{
	return &core;
}

int main(void)
{
	const struct node_info info = {.fw_version = APP_VERSION_EXTENDED_STRING};

	node_core_start(&core, &ports, &info);
	LOG_INF("g2labs-node %s started", node_core_fw_version(&core));
	return 0;
}
