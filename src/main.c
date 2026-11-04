/* SPDX-License-Identifier: MIT */
#include <zephyr/app_version.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <node/core.h>

#include "adapters/adapters.h"
#include "app.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define EVENT_QUEUE_LEN 64

static const struct node_ports ports = {
	.ctx = NULL,
	.mqtt_publish = mqtt_adapter_publish,
	.wifi_scan = wifi_adapter_scan,
	.wifi_connect = wifi_adapter_connect,
	.image_confirm = image_adapter_confirm,
};

static struct node_core core;
static K_MUTEX_DEFINE(core_lock);
K_MSGQ_DEFINE(event_queue, sizeof(struct node_event), EVENT_QUEUE_LEN, 4);

struct node_core *app_core_lock(void)
{
	k_mutex_lock(&core_lock, K_FOREVER);
	return &core;
}

void app_core_unlock(void)
{
	k_mutex_unlock(&core_lock);
}

void app_post_event(const struct node_event *event)
{
	/*
	 * Scan results may be dropped when a scan floods the queue; every other
	 * event changes the core's state, so wait for room.
	 */
	k_timeout_t wait =
		event->type == NODE_EVENT_WIFI_NETWORK_HEARD ? K_NO_WAIT : K_FOREVER;

	if (k_msgq_put(&event_queue, event, wait) != 0) {
		LOG_DBG("event queue full, dropped scan result");
	}
}

int main(void)
{
	const struct node_info info = {.fw_version = APP_VERSION_EXTENDED_STRING};
	struct node_event event;

	wifi_adapter_init();

	app_core_lock();
	node_core_init(&core, &ports, &info);
	settings_adapter_load_known_networks(&core);
	node_core_start(&core);
	app_core_unlock();

	LOG_INF("g2labs-node %s started", info.fw_version);

	while (true) {
		k_msgq_get(&event_queue, &event, K_FOREVER);
		app_core_lock();
		node_core_handle_event(&core, &event);
		app_core_unlock();
	}
	return 0;
}
