/* SPDX-License-Identifier: MIT */
#include "fake_ports.h"

#include <errno.h>
#include <string.h>

#include <zephyr/sys/util.h>

static int fake_mqtt_publish(void *ctx, const char *topic, const void *payload, size_t len,
			     bool retained)
{
	struct fake_ports *fake = ctx;

	if (fake->publish_count >= FAKE_MAX_PUBLISHES) {
		return -ENOMEM;
	}

	struct fake_publish *p = &fake->publishes[fake->publish_count++];

	strncpy(p->topic, topic, sizeof(p->topic) - 1);
	p->len = len < sizeof(p->payload) - 1 ? len : sizeof(p->payload) - 1;
	memcpy(p->payload, payload, p->len);
	p->payload[p->len] = '\0';
	p->retained = retained;
	return 0;
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
		.mqtt_publish = fake_mqtt_publish,
		.wifi_scan = fake_wifi_scan,
		.wifi_connect = fake_wifi_connect,
		.wifi_disconnect = fake_wifi_disconnect,
		.image_confirm = fake_image_confirm,
	};
}
