/* SPDX-License-Identifier: MIT */
/*
 * The Node core.
 *
 * All of the Node's decision-making lives here. The core never calls Zephyr
 * Wi-Fi, Bluetooth, MQTT or MCUboot APIs: it is driven by inputs (start-up,
 * and later platform events and settings) and acts only through the output
 * ports in struct node_ports, which platform adapters implement on hardware
 * and fakes implement in tests.
 */
#ifndef NODE_CORE_H
#define NODE_CORE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Outputs of the core. Each returns 0 on success or a negative error code. */
struct node_ports {
	/* Opaque adapter context, passed back to every port. */
	void *ctx;

	/* MQTT adapter: publish a message to the Broker. */
	int (*mqtt_publish)(void *ctx, const char *topic, const void *payload, size_t len,
			    bool retained);

	/* Wi-Fi adapter: start a Wi-Fi scan. */
	int (*wifi_scan)(void *ctx);

	/* Wi-Fi adapter: join the given network. */
	int (*wifi_connect)(void *ctx, const char *ssid, const char *password);

	/* Image adapter: mark the running image as confirmed. */
	int (*image_confirm)(void *ctx);
};

/* Facts about this Node fixed at build or boot time. */
struct node_info {
	const char *fw_version;
};

struct node_core {
	const struct node_ports *ports;
	struct node_info info;
};

/* Start the core. `ports` and the strings in `info` must outlive the core. */
void node_core_start(struct node_core *core, const struct node_ports *ports,
		     const struct node_info *info);

/* The firmware version this Node is running. */
const char *node_core_fw_version(const struct node_core *core);

#ifdef __cplusplus
}
#endif

#endif /* NODE_CORE_H */
