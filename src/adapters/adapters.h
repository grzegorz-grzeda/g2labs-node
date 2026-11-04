/* SPDX-License-Identifier: MIT */
/*
 * Platform adapters: the only code that talks to Zephyr Wi-Fi, Bluetooth,
 * MQTT and MCUboot APIs. Each function matches a port in struct node_ports.
 */
#ifndef ADAPTERS_H
#define ADAPTERS_H

#include <stdbool.h>
#include <stddef.h>

int image_adapter_confirm(void *ctx);

int mqtt_adapter_publish(void *ctx, const char *topic, const void *payload, size_t len,
			 bool retained);

int wifi_adapter_scan(void *ctx);
int wifi_adapter_connect(void *ctx, const char *ssid, const char *password);

#endif /* ADAPTERS_H */
