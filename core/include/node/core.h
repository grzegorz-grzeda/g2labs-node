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
#include <limits.h>
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

/*
 * Wi-Fi limits: SSIDs are 1-32 bytes; passwords are empty (open network),
 * an 8-63 character passphrase, or a 64 hex digit key.
 */
#define NODE_SSID_MAX_LEN         32
#define NODE_PASSWORD_MIN_LEN     8
#define NODE_PASSWORD_MAX_LEN     64
#define NODE_KNOWN_NETWORKS_MAX   5
#define NODE_RSSI_NOT_HEARD       INT_MIN

/* A Wi-Fi network the Node is allowed to join. */
struct node_known_network {
	char ssid[NODE_SSID_MAX_LEN + 1];
	char password[NODE_PASSWORD_MAX_LEN + 1];
};

/* Platform events fed into the core by adapters. */
enum node_event_type {
	/* One network heard during a Wi-Fi scan. */
	NODE_EVENT_WIFI_NETWORK_HEARD,
	/* The Wi-Fi scan has finished. */
	NODE_EVENT_WIFI_SCAN_DONE,
	/* Joined the network the core asked for, with an IP address. */
	NODE_EVENT_WIFI_CONNECTED,
	/* Not on a network any more, or the join failed. */
	NODE_EVENT_WIFI_DISCONNECTED,
};

struct node_event {
	enum node_event_type type;
	union {
		struct {
			char ssid[NODE_SSID_MAX_LEN + 1];
			int rssi;
		} network_heard;
	};
};

/* Facts about this Node fixed at build or boot time. */
struct node_info {
	const char *fw_version;
};

/* What the Node is doing about Wi-Fi. */
enum node_wifi_state {
	/* Not on a network and not trying to join one. */
	NODE_WIFI_IDLE,
	NODE_WIFI_SCANNING,
	NODE_WIFI_JOINING,
	NODE_WIFI_CONNECTED,
};

struct node_core {
	const struct node_ports *ports;
	struct node_info info;
	bool started;
	enum node_wifi_state wifi_state;
	/* The Known Network being joined or joined. */
	char wifi_ssid[NODE_SSID_MAX_LEN + 1];
	struct node_known_network known_networks[NODE_KNOWN_NETWORKS_MAX];
	size_t known_network_count;
	/*
	 * Strongest RSSI of each Known Network in the current scan, kept in step
	 * with known_networks; NODE_RSSI_NOT_HEARD if it wasn't heard.
	 */
	int heard_rssi[NODE_KNOWN_NETWORKS_MAX];
};

/*
 * Prepare the core without producing any output, so stored settings can be
 * loaded before it starts. `ports` and the strings in `info` must outlive the
 * core.
 */
void node_core_init(struct node_core *core, const struct node_ports *ports,
		    const struct node_info *info);

/* Start acting: from here on the core may produce outputs. */
void node_core_start(struct node_core *core);

/*
 * Add a Known Network, or change the password of an existing one.
 * Returns 0, -EINVAL for an invalid SSID or password, or -ENOSPC when
 * NODE_KNOWN_NETWORKS_MAX are already stored.
 */
int node_core_add_known_network(struct node_core *core, const char *ssid, const char *password);

/* Remove a Known Network. Returns 0, or -ENOENT if it isn't stored. */
int node_core_remove_known_network(struct node_core *core, const char *ssid);

/* The i-th Known Network, in the order added, or NULL past the end. */
const struct node_known_network *node_core_known_network(const struct node_core *core, size_t i);

/* How many Known Networks are stored. */
size_t node_core_known_network_count(const struct node_core *core);

/* SSID of the Known Network the Node is on, or NULL when offline. */
const char *node_core_current_network(const struct node_core *core);

/* Feed a platform event into the core. */
void node_core_handle_event(struct node_core *core, const struct node_event *event);

/* The firmware version this Node is running. */
const char *node_core_fw_version(const struct node_core *core);

#ifdef __cplusplus
}
#endif

#endif /* NODE_CORE_H */
