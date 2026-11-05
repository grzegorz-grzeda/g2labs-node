/* SPDX-License-Identifier: MIT */
#include "fake_ports.h"

#include <errno.h>
#include <string.h>

#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

/* Host side of the publish dump, in publish_dump_bottom.c. */
void publish_dump_bottom_write_line(const char *line);

/*
 * Every publish the core makes is also dumped, one JSON object per line
 * ({"test", "topic", "payload", "retained"}), for the contract checker
 * (contract/check.py) to validate against contract/.
 */
static char current_test[128];

static void remember_test(const struct ztest_unit_test *test, void *data)
{
	ARG_UNUSED(data);
	snprintk(current_test, sizeof(current_test), "%s.%s", test->test_suite_name, test->name);
}

ZTEST_RULE(fake_ports_test_name, remember_test, NULL);

struct dump_writer {
	char buf[FAKE_TOPIC_MAX * 6 + FAKE_PAYLOAD_MAX * 6 + sizeof(current_test) * 6 + 128];
	size_t len;
};

static void dump_raw(struct dump_writer *w, const char *s)
{
	while (*s != '\0' && w->len < sizeof(w->buf) - 1) {
		w->buf[w->len++] = *s++;
	}
}

/* A JSON string of the len bytes at s, escaped. */
static void dump_string(struct dump_writer *w, const char *s, size_t len)
{
	char esc[8];

	dump_raw(w, "\"");
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];

		if (c == '"' || c == '\\') {
			snprintk(esc, sizeof(esc), "\\%c", c);
		} else if (c < 0x20) {
			snprintk(esc, sizeof(esc), "\\u%04x", c);
		} else {
			esc[0] = (char)c;
			esc[1] = '\0';
		}
		dump_raw(w, esc);
	}
	dump_raw(w, "\"");
}

static void dump_publish(const char *topic, const void *payload, size_t len, bool retained)
{
	static struct dump_writer w;

	w.len = 0;
	dump_raw(&w, "{\"test\":");
	dump_string(&w, current_test, strlen(current_test));
	dump_raw(&w, ",\"topic\":");
	dump_string(&w, topic, strlen(topic));
	dump_raw(&w, ",\"payload\":");
	dump_string(&w, payload, len);
	dump_raw(&w, retained ? ",\"retained\":true}" : ",\"retained\":false}");
	w.buf[w.len] = '\0';
	publish_dump_bottom_write_line(w.buf);
}

static void copy(char *dst, size_t size, const char *src)
{
	strncpy(dst, src, size - 1);
	dst[size - 1] = '\0';
}

static int fake_mqtt_publish(void *ctx, const char *topic, const void *payload, size_t len,
			     bool retained)
{
	struct fake_ports *fake = ctx;

	dump_publish(topic, payload, len, retained);
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

static int fake_mqtt_subscribe(void *ctx, const char *topic)
{
	struct fake_ports *fake = ctx;

	fake->mqtt_subscribe_count++;
	copy(fake->last_subscribe_topic, sizeof(fake->last_subscribe_topic), topic);
	return fake->mqtt_subscribe_result;
}

static void fake_reboot(void *ctx)
{
	struct fake_ports *fake = ctx;

	fake->reboot_count++;
}

static void fake_log_warning(void *ctx, const char *message)
{
	struct fake_ports *fake = ctx;

	fake->warning_count++;
	copy(fake->last_warning, sizeof(fake->last_warning), message);
}

static int fake_wifi_rssi(void *ctx, int *rssi)
{
	struct fake_ports *fake = ctx;

	*rssi = fake->rssi;
	return fake->rssi_result;
}

static int fake_wifi_ipv4(void *ctx, uint8_t addr[4])
{
	struct fake_ports *fake = ctx;

	memcpy(addr, fake->ipv4, sizeof(fake->ipv4));
	return fake->ipv4_result;
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
	return fake->image_confirm_result;
}

void fake_ports_init(struct fake_ports *fake)
{
	memset(fake, 0, sizeof(*fake));
	fake->ports = (struct node_ports){
		.ctx = fake,
		.mqtt_connect = fake_mqtt_connect,
		.mqtt_disconnect = fake_mqtt_disconnect,
		.mqtt_publish = fake_mqtt_publish,
		.mqtt_subscribe_commands = fake_mqtt_subscribe,
		.wifi_rssi = fake_wifi_rssi,
		.wifi_ipv4 = fake_wifi_ipv4,
		.free_memory = fake_free_memory,
		.wifi_scan = fake_wifi_scan,
		.wifi_connect = fake_wifi_connect,
		.wifi_disconnect = fake_wifi_disconnect,
		.image_confirm = fake_image_confirm,
		.reboot = fake_reboot,
		.log_warning = fake_log_warning,
	};
}
