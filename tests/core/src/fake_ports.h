/* SPDX-License-Identifier: MIT */
/*
 * Fake adapters for driving the Node core in tests. They record every output
 * the core produces so tests can assert on them.
 */
#ifndef FAKE_PORTS_H
#define FAKE_PORTS_H

#include <stdbool.h>
#include <stddef.h>

#include <node/core.h>

#define FAKE_MAX_PUBLISHES 32
#define FAKE_TOPIC_MAX     128
#define FAKE_PAYLOAD_MAX   512

struct fake_publish {
	char topic[FAKE_TOPIC_MAX];
	char payload[FAKE_PAYLOAD_MAX];
	size_t len;
	bool retained;
};

/* A copy of the parameters of the latest mqtt_connect. */
struct fake_mqtt_connect {
	char host[NODE_BROKER_HOST_MAX_LEN + 1];
	uint16_t port;
	char client_id[NODE_ID_LEN + 1];
	/* NULL pointers are recorded as has_* = false. */
	bool has_username;
	char username[NODE_BROKER_USERNAME_MAX_LEN + 1];
	bool has_password;
	char password[NODE_BROKER_PASSWORD_MAX_LEN + 1];
	char will_topic[FAKE_TOPIC_MAX];
	char will_payload[FAKE_PAYLOAD_MAX];
	bool will_retained;
};

struct fake_ports {
	struct node_ports ports;

	struct fake_publish publishes[FAKE_MAX_PUBLISHES];
	size_t publish_count;
	size_t wifi_scan_count;
	size_t wifi_connect_count;
	size_t wifi_disconnect_count;
	char last_connect_ssid[NODE_SSID_MAX_LEN + 1];
	char last_connect_password[NODE_PASSWORD_MAX_LEN + 1];
	size_t image_confirm_count;
	size_t mqtt_connect_count;
	struct fake_mqtt_connect last_mqtt_connect;
	size_t mqtt_disconnect_count;
	size_t mqtt_subscribe_count;
	char last_subscribe_topic[FAKE_TOPIC_MAX];
	size_t reboot_count;
	size_t warning_count;
	char last_warning[FAKE_PAYLOAD_MAX];

	/* What the fake platform reports: link RSSI (or rssi_result), free memory. */
	int rssi;
	int rssi_result;
	size_t free_memory;

	/* Return codes the fake Wi-Fi requests give back (0 = accepted). */
	int wifi_scan_result;
	int wifi_connect_result;
};

/* Reset all recordings and wire the fake into fake->ports. */
void fake_ports_init(struct fake_ports *fake);

#endif /* FAKE_PORTS_H */
