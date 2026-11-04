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
#include <stdint.h>

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

	/* Wi-Fi adapter: leave the current network, or abandon a join. */
	int (*wifi_disconnect)(void *ctx);

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

/*
 * Wi-Fi timeouts, in milliseconds.
 *
 * RECONNECT: how long a join (or a rejoin after a link drop) may take to
 * associate before the Node gives that network up and rescans.
 */
#define NODE_WIFI_RECONNECT_TIMEOUT_MS 20000
/* DHCP: how long, once associated, the Node waits for an IP address. */
#define NODE_WIFI_DHCP_TIMEOUT_MS      15000
/* RETRY: the pause before trying again to join a network that refused. */
#define NODE_WIFI_RETRY_INTERVAL_MS    2000
/* RESCAN: how often the Node scans while no Known Network is reachable. */
#define NODE_WIFI_RESCAN_INTERVAL_MS   30000
/* SCAN: how long a scan may run before the Node stops waiting for it. */
#define NODE_WIFI_SCAN_TIMEOUT_MS      10000

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
	/* Associated with the network the core asked for; no IP address yet. */
	NODE_EVENT_WIFI_ASSOCIATED,
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
	/* Asked to join a network, not associated yet. */
	NODE_WIFI_JOINING,
	/* Associated, waiting for DHCP to give an IP address. */
	NODE_WIFI_AWAITING_IP,
	/* The join failed; trying the same network again shortly. */
	NODE_WIFI_JOIN_RETRY_WAIT,
	NODE_WIFI_CONNECTED,
};

struct node_core {
	const struct node_ports *ports;
	struct node_info info;
	bool started;
	enum node_wifi_state wifi_state;
	/* The Known Network being joined or joined. */
	char wifi_ssid[NODE_SSID_MAX_LEN + 1];
	/* The latest time given to node_core_tick(). */
	int64_t now_ms;
	/*
	 * When the current Wi-Fi state times out: while trying a network, when
	 * it is given up; while scanning, when to stop waiting for the scan;
	 * while idle, when the next scan is due.
	 */
	int64_t wifi_timeout_ms;
	/* While waiting to retry a join: when to try again. */
	int64_t wifi_retry_ms;
	/*
	 * A Known Network the Node just gave up on: the next scan joins it only
	 * if no other Known Network is heard. Empty when there is none.
	 */
	char wifi_avoid_ssid[NODE_SSID_MAX_LEN + 1];
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

/*
 * Tell the core the time, in milliseconds on a monotonic clock. Call it at
 * least once a second, and once before node_core_start(); the core measures
 * its Wi-Fi timeouts from the latest time it was given.
 */
void node_core_tick(struct node_core *core, int64_t now_ms);

/* Feed a platform event into the core. */
void node_core_handle_event(struct node_core *core, const struct node_event *event);

/* The firmware version this Node is running. */
const char *node_core_fw_version(const struct node_core *core);

#ifdef __cplusplus
}
#endif

#endif /* NODE_CORE_H */
