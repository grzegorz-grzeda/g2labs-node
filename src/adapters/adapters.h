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

#define IMAGE_ADAPTER_SLOTS       2
#define IMAGE_ADAPTER_VERSION_LEN 24 /* "255.255.65535.4294967295" */
#define IMAGE_ADAPTER_HASH_LEN    32 /* SHA-256 */

/* An image in one of the two MCUboot slots, as SMP's image list reports it. */
struct image_adapter_slot {
	int slot;
	char version[IMAGE_ADAPTER_VERSION_LEN + 1];
	uint8_t hash[IMAGE_ADAPTER_HASH_LEN];
	bool active;    /* running now */
	bool confirmed; /* MCUboot keeps it */
	bool pending;   /* MCUboot swaps to it on the next reboot */
	bool permanent; /* pending, and confirmed once swapped */
};

/* Fill slots with the images present; returns how many (0 to IMAGE_ADAPTER_SLOTS). */
size_t image_adapter_list(struct image_adapter_slot slots[IMAGE_ADAPTER_SLOTS]);

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
/* The global IPv4 address of the Wi-Fi interface; -ENOENT without one. */
int wifi_adapter_ipv4(void *ctx, uint8_t addr[NODE_IPV4_LEN]);
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
/* Whether a settings key names a password ("…/password"), never to be read back. */
bool settings_adapter_is_password_key(const char *name);

/*
 * Guard settings management over SMP-UDP: refuse password reads, deletes
 * under node/ and settings load.
 */
void smp_adapter_init(void);

/*
 * Start the hardware watchdog, which resets the Node unless fed within about
 * timeout_ms (see CONFIG_NODE_WATCHDOG_TIMEOUT_S). Failures are logged.
 */
void watchdog_adapter_init(uint32_t timeout_ms);
void watchdog_adapter_feed(void);

#endif /* NODE_APP_ADAPTERS_H */
