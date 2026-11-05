/* SPDX-License-Identifier: MIT */
/*
 * The Node core.
 *
 * All of the Node's decision-making lives here. The core never calls Zephyr
 * Wi-Fi, Bluetooth, MQTT or MCUboot APIs: it is driven by inputs (start-up,
 * the time, platform events and settings) and acts only through the ports in
 * struct node_ports, which platform adapters implement on hardware and fakes
 * implement in tests.
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

/* How to reach the Broker, for the mqtt_connect port. */
struct node_mqtt_connect {
	const char *host;
	uint16_t port;
	const char *client_id;
	/* NULL to connect anonymously. */
	const char *username;
	/* NULL for no password; only set together with a username. */
	const char *password;
	/* The last will the Broker publishes if the Node drops off. */
	const char *will_topic;
	const char *will_payload;
	bool will_retained;
};

/*
 * Outputs of the core, and the few facts it queries from the platform. Each
 * returns 0 on success or a negative error code, unless stated otherwise.
 */
struct node_ports {
	/* Opaque adapter context, passed back to every port. */
	void *ctx;

	/*
	 * MQTT adapter: connect to the Broker, dropping any current connection.
	 * Returns at once; the outcome arrives as NODE_EVENT_BROKER_CONNECTED or
	 * NODE_EVENT_BROKER_DISCONNECTED. The strings in `params` are valid only
	 * during the call.
	 */
	int (*mqtt_connect)(void *ctx, const struct node_mqtt_connect *params);

	/*
	 * MQTT adapter: close the Broker connection, or abandon a connect. No
	 * further events are reported for that connection.
	 */
	int (*mqtt_disconnect)(void *ctx);

	/* MQTT adapter: publish a message to the Broker (QoS 0). */
	int (*mqtt_publish)(void *ctx, const char *topic, const void *payload, size_t len,
			    bool retained);

	/*
	 * MQTT adapter: subscribe to the Node's command topic (QoS 0) on the
	 * current connection. Messages on it arrive as NODE_EVENT_COMMAND.
	 */
	int (*mqtt_subscribe_commands)(void *ctx, const char *topic);

	/* Wi-Fi adapter: the RSSI of the current link, in dBm. */
	int (*wifi_rssi)(void *ctx, int *rssi);

	/* Wi-Fi adapter: the Node's IPv4 address, most significant byte first. */
	int (*wifi_ipv4)(void *ctx, uint8_t addr[4]);

	/* Free heap memory, in bytes. */
	size_t (*free_memory)(void *ctx);

	/* Wi-Fi adapter: start a Wi-Fi scan. */
	int (*wifi_scan)(void *ctx);

	/* Wi-Fi adapter: join the given network. */
	int (*wifi_connect)(void *ctx, const char *ssid, const char *password);

	/* Wi-Fi adapter: leave the current network, or abandon a join. */
	int (*wifi_disconnect)(void *ctx);

	/* Image adapter: mark the running image as confirmed. */
	int (*image_confirm)(void *ctx);

	/* Restart the Node. On hardware it doesn't return. */
	void (*reboot)(void *ctx);

	/* Tell the maintainer about something the core ignored, e.g. in the log. */
	void (*log_warning)(void *ctx, const char *message);
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

/*
 * Broker settings. The host is a name or IPv4 address; an empty host means no
 * Broker is configured. An empty username connects anonymously.
 */
#define NODE_BROKER_HOST_MAX_LEN     64
#define NODE_BROKER_USERNAME_MAX_LEN 64
#define NODE_BROKER_PASSWORD_MAX_LEN 64
#define NODE_BROKER_DEFAULT_PORT     1883
#define NODE_FRIENDLY_NAME_MAX_LEN   32

/* The longest command payload the core reads; longer ones are malformed. */
#define NODE_COMMAND_MAX_LEN 128
/* How long the Node waits after a Reboot command, so "offline" gets out. */
#define NODE_REBOOT_DELAY_MS 1000

/* A Node ID: the Wi-Fi MAC address as 12 lowercase hex digits. */
#define NODE_ID_LEN  12
#define NODE_MAC_LEN 6

/*
 * Broker timeouts, in milliseconds.
 *
 * CONNECT: how long a connect may take before the Node abandons it.
 */
#define NODE_BROKER_CONNECT_TIMEOUT_MS 30000
/* RETRY: the pause before connecting again after a failure or disconnect. */
#define NODE_BROKER_RETRY_INTERVAL_MS  10000
/* STATUS: how often the Node publishes its status while connected. */
#define NODE_STATUS_INTERVAL_MS        60000
/* FAILOVER: how long the Broker may be unreachable before moving network. */
#define NODE_BROKER_FAILOVER_TIMEOUT_MS 300000

struct node_broker {
	char host[NODE_BROKER_HOST_MAX_LEN + 1];
	uint16_t port;
	char username[NODE_BROKER_USERNAME_MAX_LEN + 1];
	char password[NODE_BROKER_PASSWORD_MAX_LEN + 1];
};

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
	/* The Broker accepted the connection the core asked for. */
	NODE_EVENT_BROKER_CONNECTED,
	/* The Broker connection failed or was lost. */
	NODE_EVENT_BROKER_DISCONNECTED,
	/* A message arrived on the Node's command topic. */
	NODE_EVENT_COMMAND,
};

struct node_event {
	enum node_event_type type;
	union {
		struct {
			char ssid[NODE_SSID_MAX_LEN + 1];
			int rssi;
		} network_heard;
		struct {
			/* The first `len` bytes of the payload; not NUL-terminated. */
			char payload[NODE_COMMAND_MAX_LEN];
			size_t len;
			/* The payload was longer than NODE_COMMAND_MAX_LEN. */
			bool truncated;
			/* The Broker delivered it as a retained message. */
			bool retained;
		} command;
	};
};

/* Facts about this Node fixed at build or boot time. */
struct node_info {
	const char *fw_version;
	/* The Zephyr board target the image was built for. */
	const char *board;
	/* The Wi-Fi MAC address, from which the Node ID is derived. */
	uint8_t mac[NODE_MAC_LEN];
	/*
	 * How long the Broker may stay unreachable before the Node moves to
	 * another Known Network; 0 means NODE_BROKER_FAILOVER_TIMEOUT_MS.
	 */
	uint32_t broker_failover_ms;
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

/* What the Node is doing about the Broker. */
enum node_broker_state {
	/* Not connected and not trying: no Wi-Fi or no Broker configured. */
	NODE_BROKER_IDLE,
	NODE_BROKER_CONNECTING,
	/* The connection failed or dropped; connecting again shortly. */
	NODE_BROKER_RETRY_WAIT,
	NODE_BROKER_CONNECTED,
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

	char id[NODE_ID_LEN + 1];
	char friendly_name[NODE_FRIENDLY_NAME_MAX_LEN + 1];
	struct node_broker broker;
	enum node_broker_state broker_state;
	/*
	 * While connecting, when the connect is abandoned; while waiting to
	 * retry, when to connect again; while connected, when the next status
	 * is due.
	 */
	int64_t broker_timeout_ms;
	/*
	 * The Broker is unreachable on the current network (counting survives
	 * a Wi-Fi rejoin of the same network), and when that has gone on long
	 * enough to move to another Known Network.
	 */
	bool broker_outage;
	int64_t broker_failover_deadline_ms;

	/*
	 * The running image has been confirmed this boot. It is confirmed on the
	 * first Broker connection, so an image that never gets there is
	 * reverted by MCUboot on the next reboot.
	 */
	bool image_confirmed;

	/*
	 * A Reboot command arrived: the Node stays off the Broker and reboots
	 * at reboot_at_ms (INT64_MAX once the reboot port has been called).
	 */
	bool reboot_pending;
	int64_t reboot_at_ms;
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
 * Tell the core the time, in milliseconds since boot. Call it at least once a
 * second, and once before node_core_start(); the core measures its timeouts
 * from the latest time it was given and reports it as uptime.
 */
void node_core_tick(struct node_core *core, int64_t now_ms);

/* Feed a platform event into the core. */
void node_core_handle_event(struct node_core *core, const struct node_event *event);

/*
 * Set the Broker; it applies at once, reconnecting if needed. Returns 0, or
 * -EINVAL for a host, username or password that is too long, a port of 0, or
 * a password without a username.
 */
int node_core_set_broker(struct node_core *core, const struct node_broker *broker);

/* The Broker settings. */
const struct node_broker *node_core_broker(const struct node_core *core);

/* Whether the Node is connected to the Broker. */
bool node_core_broker_connected(const struct node_core *core);

/*
 * Set the friendly name reported in status; "" clears it. Returns 0, or
 * -EINVAL if it is longer than NODE_FRIENDLY_NAME_MAX_LEN.
 */
int node_core_set_friendly_name(struct node_core *core, const char *name);

/* The friendly name, "" when none is set. */
const char *node_core_friendly_name(const struct node_core *core);

/* The Node ID, derived from the Wi-Fi MAC address. */
const char *node_core_id(const struct node_core *core);

/* The firmware version this Node is running. */
const char *node_core_fw_version(const struct node_core *core);

#ifdef __cplusplus
}
#endif

#endif /* NODE_CORE_H */
