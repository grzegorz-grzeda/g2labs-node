/* SPDX-License-Identifier: MIT */
/*
 * Platform adapters: the only code that talks to Zephyr Wi-Fi, Bluetooth,
 * MQTT, MCUboot and settings APIs. Port functions match struct node_ports.
 */
#ifndef NODE_APP_ADAPTERS_H
#define NODE_APP_ADAPTERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <node/core.h>

int image_adapter_confirm(void *ctx);

int mqtt_adapter_connect(void *ctx, const struct node_mqtt_connect *params);
int mqtt_adapter_disconnect(void *ctx);
int mqtt_adapter_publish(void *ctx, const char *topic, const void *payload, size_t len,
			 bool retained);
int mqtt_adapter_subscribe_commands(void *ctx, const char *topic);

void system_adapter_reboot(void *ctx);
void system_adapter_log_warning(void *ctx, const char *message);

/* Free bytes across all heaps. */
size_t memory_adapter_free(void *ctx);

/* Register for Zephyr Wi-Fi and IPv4 events; they reach the core as events. */
void wifi_adapter_init(void);
int wifi_adapter_scan(void *ctx);
int wifi_adapter_connect(void *ctx, const char *ssid, const char *password);
int wifi_adapter_disconnect(void *ctx);
int wifi_adapter_rssi(void *ctx, int *rssi);
/* The Wi-Fi MAC address, from which the Node ID is derived. */
int wifi_adapter_mac(uint8_t mac[NODE_MAC_LEN]);
/* RSSI and IPv4 address of the current link, for diagnostics. */
int wifi_adapter_link_info(int *rssi, char *ip, size_t ip_len);

/*
 * Settings in flash (settings subsystem): load everything into the core at
 * boot, and save each group after it changes.
 */
int settings_adapter_load(struct node_core *core);
int settings_adapter_save_known_networks(const struct node_core *core);
int settings_adapter_save_broker(const struct node_core *core);
int settings_adapter_save_friendly_name(const struct node_core *core);

#endif /* NODE_APP_ADAPTERS_H */
