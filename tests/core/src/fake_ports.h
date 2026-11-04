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

#define FAKE_MAX_PUBLISHES 16
#define FAKE_TOPIC_MAX     128
#define FAKE_PAYLOAD_MAX   512

struct fake_publish {
	char topic[FAKE_TOPIC_MAX];
	char payload[FAKE_PAYLOAD_MAX];
	size_t len;
	bool retained;
};

struct fake_ports {
	struct node_ports ports;

	struct fake_publish publishes[FAKE_MAX_PUBLISHES];
	size_t publish_count;
	size_t wifi_scan_count;
	size_t wifi_connect_count;
	size_t image_confirm_count;
};

/* Reset all recordings and wire the fake into fake->ports. */
void fake_ports_init(struct fake_ports *fake);

#endif /* FAKE_PORTS_H */
