/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <node/core.h>

#include "fake_ports.h"

static struct fake_ports fake;
static struct node_core core;
static const struct node_info info = {
	.fw_version = "1.2.3",
	.mac = {0x40, 0x4c, 0xca, 0x0a, 0xb1, 0xf2},
};
static int64_t now_ms;

static void advance(int64_t ms)
{
	now_ms += ms;
	node_core_tick(&core, now_ms);
}

static void event(enum node_event_type type)
{
	node_core_handle_event(&core, &(struct node_event){.type = type});
}

/* On "home" with a Broker configured, and connected to it. */
static void connect_to_broker(void)
{
	struct node_broker broker = {.port = 1883};
	struct node_event heard = {.type = NODE_EVENT_WIFI_NETWORK_HEARD};

	strcpy(broker.host, "broker.lan");
	zassert_ok(node_core_set_broker(&core, &broker));
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	node_core_start(&core);
	strcpy(heard.network_heard.ssid, "home");
	heard.network_heard.rssi = -50;
	node_core_handle_event(&core, &heard);
	event(NODE_EVENT_WIFI_SCAN_DONE);
	event(NODE_EVENT_WIFI_ASSOCIATED);
	event(NODE_EVENT_WIFI_CONNECTED);
	event(NODE_EVENT_BROKER_CONNECTED);
}

static void before_each(void *fixture)
{
	ARG_UNUSED(fixture);
	fake_ports_init(&fake);
	node_core_init(&core, &fake.ports, &info);
	now_ms = 0;
	node_core_tick(&core, now_ms);
}

ZTEST_SUITE(commands, NULL, NULL, before_each, NULL, NULL);

ZTEST(commands, test_on_connect_the_node_subscribes_to_its_command_topic)
{
	connect_to_broker();

	zassert_equal(fake.mqtt_subscribe_count, 1);
	zassert_str_equal(fake.last_subscribe_topic, "g2labs/404cca0ab1f2/command");
}

/* A message on the command topic, as the MQTT adapter reports it. */
static void command_bytes(const char *payload, size_t len, bool retained)
{
	struct node_event e = {.type = NODE_EVENT_COMMAND};

	zassert_true(len <= sizeof(e.command.payload));
	e.command.len = len;
	e.command.retained = retained;
	memcpy(e.command.payload, payload, len);
	node_core_handle_event(&core, &e);
}

static void command(const char *payload)
{
	command_bytes(payload, strlen(payload), false);
}

/* The command did nothing but leave a warning. */
static void assert_ignored(const char *what)
{
	advance(NODE_REBOOT_DELAY_MS);
	zassert_equal(fake.publish_count, 0, "%s: published", what);
	zassert_equal(fake.reboot_count, 0, "%s: rebooted", what);
	zassert_equal(fake.warning_count, 1, "%s: not logged", what);
}

ZTEST(commands, test_publish_status_publishes_status_at_once)
{
	connect_to_broker();
	fake.publish_count = 0;

	command("{\"command\":\"publish_status\"}");

	zassert_equal(fake.publish_count, 1);
	zassert_str_equal(fake.publishes[0].topic, "g2labs/404cca0ab1f2/status");
	zassert_false(fake.publishes[0].retained);
	zassert_equal(fake.warning_count, 0);
}

ZTEST(commands, test_reboot_says_offline_disconnects_then_reboots)
{
	connect_to_broker();
	fake.publish_count = 0;
	size_t disconnects = fake.mqtt_disconnect_count;

	command("{\"command\":\"reboot\"}");

	zassert_equal(fake.publish_count, 1);
	zassert_str_equal(fake.publishes[0].topic, "g2labs/404cca0ab1f2/availability");
	zassert_str_equal(fake.publishes[0].payload, "offline");
	zassert_true(fake.publishes[0].retained);
	zassert_equal(fake.mqtt_disconnect_count, disconnects + 1);
	zassert_equal(fake.reboot_count, 0, "rebooted before offline could get out");

	advance(NODE_REBOOT_DELAY_MS - 1);
	zassert_equal(fake.reboot_count, 0);

	advance(1);
	zassert_equal(fake.reboot_count, 1);
	zassert_equal(fake.warning_count, 0);
}

ZTEST(commands, test_while_waiting_to_reboot_the_node_stays_off_the_broker)
{
	connect_to_broker();
	size_t connects = fake.mqtt_connect_count;

	command("{\"command\":\"reboot\"}");
	event(NODE_EVENT_WIFI_DISCONNECTED);
	event(NODE_EVENT_WIFI_ASSOCIATED);
	event(NODE_EVENT_WIFI_CONNECTED);
	advance(NODE_REBOOT_DELAY_MS - 1);

	zassert_equal(fake.mqtt_connect_count, connects);
}

ZTEST(commands, test_unknown_commands_are_ignored_and_logged_by_name)
{
	connect_to_broker();
	fake.publish_count = 0;

	command("{\"command\":\"self_destruct\"}");

	assert_ignored("unknown command");
	zassert_not_null(strstr(fake.last_warning, "\"self_destruct\""), "%s", fake.last_warning);
}

ZTEST(commands, test_malformed_commands_are_ignored_and_logged)
{
	static const char *const payloads[] = {
		"",
		"reboot",
		"\"reboot\"",
		"[\"reboot\"]",
		"{",
		"{}",
		"{\"cmd\":\"reboot\"}",
		"{\"command\":1}",
		"{\"command\":null}",
		"{\"command\":\"reboot\"",
		"{\"command\":\"reboot\",}",
		"{\"command\":\"reboot\"} {}",
		"{\"command\":\"reboot\",\"command\":\"publish_status\"}",
		"{\"command\":\"reb\\qot\"}",
		"{\"command\":\"reb\\u00\"}",
		"{\"command\":\"reboot\",\"x\":tru}",
		"{\"command\":\"reboot\",\"x\":[[[[[[[[[[1]]]]]]]]]]}",
	};

	connect_to_broker();

	for (size_t i = 0; i < ARRAY_SIZE(payloads); i++) {
		fake.publish_count = 0;
		fake.warning_count = 0;
		command(payloads[i]);
		assert_ignored(payloads[i]);
	}
}

ZTEST(commands, test_a_command_with_a_nul_byte_is_malformed)
{
	static const char payload[] = "{\"command\":\"reboot\0\"}";

	connect_to_broker();
	fake.publish_count = 0;

	command_bytes(payload, sizeof(payload) - 1, false);

	assert_ignored("NUL byte");
}

ZTEST(commands, test_a_command_too_long_to_read_is_malformed)
{
	struct node_event e = {.type = NODE_EVENT_COMMAND};
	const char *payload = "{\"command\":\"publish_status\"}";

	connect_to_broker();
	fake.publish_count = 0;
	e.command.len = strlen(payload);
	memcpy(e.command.payload, payload, e.command.len);
	e.command.truncated = true;

	node_core_handle_event(&core, &e);

	assert_ignored("truncated");
}

ZTEST(commands, test_a_retained_command_is_ignored_so_it_cannot_reboot_the_node_forever)
{
	connect_to_broker();
	fake.publish_count = 0;

	command_bytes("{\"command\":\"reboot\"}", strlen("{\"command\":\"reboot\"}"), true);

	assert_ignored("retained");
}

ZTEST(commands, test_members_the_node_does_not_know_are_skipped)
{
	connect_to_broker();
	fake.publish_count = 0;

	command(" { \"by\" : \"hub\\\"s \\u00e9\", \"n\": -1.5e3, \"x\": [true, false, null, {\"a\": []}],"
		" \"command\" : \"publish_\\u0073tatus\" } ");

	zassert_equal(fake.publish_count, 1);
	zassert_str_equal(fake.publishes[0].topic, "g2labs/404cca0ab1f2/status");
	zassert_equal(fake.warning_count, 0, "%s", fake.last_warning);
}

ZTEST(commands, test_a_command_left_over_from_a_lost_connection_is_dropped)
{
	connect_to_broker();
	event(NODE_EVENT_BROKER_DISCONNECTED);
	fake.publish_count = 0;

	command("{\"command\":\"reboot\"}");

	assert_ignored("left over");
}

ZTEST(commands, test_a_failed_subscription_is_logged)
{
	fake.mqtt_subscribe_result = -ENOMEM;

	connect_to_broker();

	zassert_equal(fake.warning_count, 1);
	zassert_not_null(strstr(fake.last_warning, "command"), "%s", fake.last_warning);
}
