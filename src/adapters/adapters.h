/* SPDX-License-Identifier: MIT */
/*
 * Platform adapters: the only code that talks to Zephyr Wi-Fi, Bluetooth,
 * MQTT, MCUboot and settings APIs. Port functions match struct node_ports.
 */
#ifndef NODE_APP_ADAPTERS_H
#define NODE_APP_ADAPTERS_H

#include <stdbool.h>
#include <stddef.h>

#include <node/core.h>

int image_adapter_confirm(void *ctx);

int mqtt_adapter_publish(void *ctx, const char *topic, const void *payload, size_t len,
			 bool retained);

/* Register for Zephyr Wi-Fi and IPv4 events; they reach the core as events. */
void wifi_adapter_init(void);
int wifi_adapter_scan(void *ctx);
int wifi_adapter_connect(void *ctx, const char *ssid, const char *password);
/* RSSI and IPv4 address of the current link, for diagnostics. */
int wifi_adapter_link_info(int *rssi, char *ip, size_t ip_len);

/* Known Networks in flash (settings subsystem). */
int settings_adapter_load_known_networks(struct node_core *core);
int settings_adapter_save_known_networks(const struct node_core *core);

#endif /* NODE_APP_ADAPTERS_H */
