/* SPDX-License-Identifier: MIT */
#include <zephyr/app_version.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <node/core.h>

#include "adapters/adapters.h"
#include "app.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define EVENT_QUEUE_LEN 64
/* The core is told the time at least this often, for its timeouts. */
#define TICK_PERIOD     K_SECONDS(1)

static const struct node_ports ports = {
	.ctx = NULL,
	.mqtt_connect = mqtt_adapter_connect,
	.mqtt_disconnect = mqtt_adapter_disconnect,
	.mqtt_publish = mqtt_adapter_publish,
	.wifi_rssi = wifi_adapter_rssi,
	.free_memory = memory_adapter_free,
	.wifi_scan = wifi_adapter_scan,
	.wifi_connect = wifi_adapter_connect,
	.wifi_disconnect = wifi_adapter_disconnect,
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
	struct node_info info = {.fw_version = APP_VERSION_EXTENDED_STRING};
	struct node_event event;

	if (wifi_adapter_mac(info.mac) != 0) {
		LOG_ERR("no Wi-Fi MAC address; the Node ID will be all zeros");
	}
	wifi_adapter_init();

	app_core_lock();
	node_core_init(&core, &ports, &info);
	settings_adapter_load(&core);
	node_core_tick(&core, k_uptime_get());
	node_core_start(&core);
	app_core_unlock();

	LOG_INF("g2labs-node %s started, Node ID %s", info.fw_version, node_core_id(&core));

	while (true) {
		bool have_event = k_msgq_get(&event_queue, &event, TICK_PERIOD) == 0;

		app_core_lock();
		node_core_tick(&core, k_uptime_get());
		if (have_event) {
			node_core_handle_event(&core, &event);
		}
		app_core_unlock();
	}
	return 0;
}
