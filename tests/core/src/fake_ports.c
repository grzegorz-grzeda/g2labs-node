/* SPDX-License-Identifier: MIT */
#include "fake_ports.h"

#include <errno.h>
#include <string.h>

#include <zephyr/sys/util.h>

static void copy(char *dst, size_t size, const char *src)
{
	strncpy(dst, src, size - 1);
	dst[size - 1] = '\0';
}

static int fake_mqtt_publish(void *ctx, const char *topic, const void *payload, size_t len,
			     bool retained)
{
	struct fake_ports *fake = ctx;

	if (fake->publish_count >= FAKE_MAX_PUBLISHES) {
		return -ENOMEM;
	}

	struct fake_publish *p = &fake->publishes[fake->publish_count++];

	copy(p->topic, sizeof(p->topic), topic);
	p->len = len < sizeof(p->payload) - 1 ? len : sizeof(p->payload) - 1;
	memcpy(p->payload, payload, p->len);
	p->payload[p->len] = '\0';
	p->retained = retained;
	return 0;
}

static int fake_mqtt_connect(void *ctx, const struct node_mqtt_connect *params)
{
	struct fake_ports *fake = ctx;
	struct fake_mqtt_connect *c = &fake->last_mqtt_connect;

	fake->mqtt_connect_count++;
	memset(c, 0, sizeof(*c));
	copy(c->host, sizeof(c->host), params->host);
	c->port = params->port;
	copy(c->client_id, sizeof(c->client_id), params->client_id);
	c->has_username = params->username != NULL;
	if (c->has_username) {
		copy(c->username, sizeof(c->username), params->username);
	}
	c->has_password = params->password != NULL;
	if (c->has_password) {
		copy(c->password, sizeof(c->password), params->password);
	}
	copy(c->will_topic, sizeof(c->will_topic), params->will_topic);
	copy(c->will_payload, sizeof(c->will_payload), params->will_payload);
	c->will_retained = params->will_retained;
	return 0;
}

static int fake_mqtt_disconnect(void *ctx)
{
	struct fake_ports *fake = ctx;

	fake->mqtt_disconnect_count++;
	return 0;
}

static int fake_wifi_rssi(void *ctx, int *rssi)
{
	struct fake_ports *fake = ctx;

	*rssi = fake->rssi;
	return fake->rssi_result;
}

static size_t fake_free_memory(void *ctx)
{
	struct fake_ports *fake = ctx;

	return fake->free_memory;
}

static int fake_wifi_scan(void *ctx)
{
	struct fake_ports *fake = ctx;

	fake->wifi_scan_count++;
	return fake->wifi_scan_result;
}

static int fake_wifi_connect(void *ctx, const char *ssid, const char *password)
{
	struct fake_ports *fake = ctx;

	fake->wifi_connect_count++;
	strncpy(fake->last_connect_ssid, ssid, sizeof(fake->last_connect_ssid) - 1);
	strncpy(fake->last_connect_password, password, sizeof(fake->last_connect_password) - 1);
	return fake->wifi_connect_result;
}

static int fake_wifi_disconnect(void *ctx)
{
	struct fake_ports *fake = ctx;

	fake->wifi_disconnect_count++;
	return 0;
}

static int fake_image_confirm(void *ctx)
{
	struct fake_ports *fake = ctx;

	fake->image_confirm_count++;
	return 0;
}

void fake_ports_init(struct fake_ports *fake)
{
	memset(fake, 0, sizeof(*fake));
	fake->ports = (struct node_ports){
		.ctx = fake,
		.mqtt_connect = fake_mqtt_connect,
		.mqtt_disconnect = fake_mqtt_disconnect,
		.mqtt_publish = fake_mqtt_publish,
		.wifi_rssi = fake_wifi_rssi,
		.free_memory = fake_free_memory,
		.wifi_scan = fake_wifi_scan,
		.wifi_connect = fake_wifi_connect,
		.wifi_disconnect = fake_wifi_disconnect,
		.image_confirm = fake_image_confirm,
	};
}
