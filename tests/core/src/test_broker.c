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
	.mac = {0x40, 0x4c, 0xca, 0x0a, 0xb1, 0xF2},
};
static int64_t now_ms;

static void before_each(void *fixture)
{
	ARG_UNUSED(fixture);
	fake_ports_init(&fake);
	node_core_init(&core, &fake.ports, &info);
	now_ms = 0;
	node_core_tick(&core, now_ms);
}

static void advance(int64_t ms)
{
	now_ms += ms;
	node_core_tick(&core, now_ms);
}

static void event(enum node_event_type type)
{
	node_core_handle_event(&core, &(struct node_event){.type = type});
}

static void set_broker(const char *host, uint16_t port, const char *username,
		       const char *password)
{
	struct node_broker broker = {.port = port};

	strcpy(broker.host, host);
	strcpy(broker.username, username);
	strcpy(broker.password, password);
	zassert_ok(node_core_set_broker(&core, &broker));
}

/* Start with "home" known and get onto it. */
static void join_home(void)
{
	struct node_event heard = {.type = NODE_EVENT_WIFI_NETWORK_HEARD};

	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	node_core_start(&core);
	strcpy(heard.network_heard.ssid, "home");
	heard.network_heard.rssi = -50;
	node_core_handle_event(&core, &heard);
	event(NODE_EVENT_WIFI_SCAN_DONE);
	event(NODE_EVENT_WIFI_ASSOCIATED);
	event(NODE_EVENT_WIFI_CONNECTED);
}

ZTEST_SUITE(broker, NULL, NULL, before_each, NULL, NULL);

ZTEST(broker, test_on_wifi_the_node_connects_with_an_offline_last_will)
{
	set_broker("broker.lan", 1883, "node", "secret123");

	join_home();

	zassert_equal(fake.mqtt_connect_count, 1);
	zassert_str_equal(fake.last_mqtt_connect.host, "broker.lan");
	zassert_equal(fake.last_mqtt_connect.port, 1883);
	zassert_str_equal(fake.last_mqtt_connect.client_id, "404cca0ab1f2");
	zassert_true(fake.last_mqtt_connect.has_username);
	zassert_str_equal(fake.last_mqtt_connect.username, "node");
	zassert_true(fake.last_mqtt_connect.has_password);
	zassert_str_equal(fake.last_mqtt_connect.password, "secret123");
	zassert_str_equal(fake.last_mqtt_connect.will_topic, "g2labs/404cca0ab1f2/availability");
	zassert_str_equal(fake.last_mqtt_connect.will_payload, "offline");
	zassert_true(fake.last_mqtt_connect.will_retained);
}

/* Get onto "home" and connected to a Broker. */
static void connect_to_broker(void)
{
	set_broker("broker.lan", 1883, "node", "secret123");
	join_home();
	event(NODE_EVENT_BROKER_CONNECTED);
}

ZTEST(broker, test_on_connect_the_node_publishes_a_retained_online_status)
{
	connect_to_broker();

	zassert_true(fake.publish_count >= 1);
	zassert_str_equal(fake.publishes[0].topic, "g2labs/404cca0ab1f2/availability");
	zassert_str_equal(fake.publishes[0].payload, "online");
	zassert_true(fake.publishes[0].retained);
}

ZTEST(broker, test_on_connect_the_node_publishes_its_status)
{
	fake.rssi = -61;
	fake.free_memory = 54321;
	advance(83500);

	connect_to_broker();

	zassert_equal(fake.publish_count, 2);
	zassert_str_equal(fake.publishes[1].topic, "g2labs/404cca0ab1f2/status");
	zassert_str_equal(fake.publishes[1].payload,
			  "{\"node_id\":\"404cca0ab1f2\",\"fw_version\":\"1.2.3\","
			  "\"uptime_s\":83,\"network\":\"home\",\"rssi\":-61,"
			  "\"free_heap\":54321}");
	zassert_false(fake.publishes[1].retained);
}

ZTEST(broker, test_status_is_published_every_minute_while_connected)
{
	connect_to_broker();
	zassert_equal(fake.publish_count, 2);

	advance(NODE_STATUS_INTERVAL_MS - 1);
	zassert_equal(fake.publish_count, 2);

	advance(1);
	zassert_equal(fake.publish_count, 3);
	zassert_str_equal(fake.publishes[2].topic, "g2labs/404cca0ab1f2/status");
	zassert_not_null(strstr(fake.publishes[2].payload, "\"uptime_s\":60,"));

	advance(NODE_STATUS_INTERVAL_MS);
	zassert_equal(fake.publish_count, 4);
}

ZTEST(broker, test_after_a_disconnect_the_node_reconnects_and_announces_itself_again)
{
	connect_to_broker();
	zassert_true(node_core_broker_connected(&core));
	event(NODE_EVENT_BROKER_DISCONNECTED);
	zassert_false(node_core_broker_connected(&core));

	advance(NODE_BROKER_RETRY_INTERVAL_MS - 1);
	zassert_equal(fake.mqtt_connect_count, 1);

	advance(1);
	zassert_equal(fake.mqtt_connect_count, 2);

	event(NODE_EVENT_BROKER_CONNECTED);
	zassert_equal(fake.publish_count, 4);
	zassert_str_equal(fake.publishes[2].topic, "g2labs/404cca0ab1f2/availability");
	zassert_str_equal(fake.publishes[2].payload, "online");
	zassert_true(fake.publishes[2].retained);
	zassert_str_equal(fake.publishes[3].topic, "g2labs/404cca0ab1f2/status");
}

ZTEST(broker, test_a_failed_connect_is_retried)
{
	set_broker("broker.lan", 1883, "node", "secret123");
	join_home();
	event(NODE_EVENT_BROKER_DISCONNECTED);

	advance(NODE_BROKER_RETRY_INTERVAL_MS);

	zassert_equal(fake.mqtt_connect_count, 2);
	zassert_equal(fake.publish_count, 0);
}

ZTEST(broker, test_no_status_is_published_while_disconnected)
{
	connect_to_broker();
	event(NODE_EVENT_BROKER_DISCONNECTED);
	event(NODE_EVENT_BROKER_DISCONNECTED);

	advance(NODE_STATUS_INTERVAL_MS);

	zassert_equal(fake.publish_count, 2);
}

ZTEST(broker, test_a_connect_that_never_answers_is_abandoned_and_retried)
{
	set_broker("broker.lan", 1883, "node", "secret123");
	join_home();

	advance(NODE_BROKER_CONNECT_TIMEOUT_MS - 1);
	zassert_equal(fake.mqtt_disconnect_count, 0);

	advance(1);
	zassert_equal(fake.mqtt_disconnect_count, 1);
	zassert_equal(fake.mqtt_connect_count, 1);

	advance(NODE_BROKER_RETRY_INTERVAL_MS);
	zassert_equal(fake.mqtt_connect_count, 2);
}

ZTEST(broker, test_losing_wifi_drops_the_broker_until_wifi_is_back)
{
	connect_to_broker();

	event(NODE_EVENT_WIFI_DISCONNECTED);
	zassert_equal(fake.mqtt_disconnect_count, 1);
	/* If the link is still up, a clean disconnect wouldn't fire the will. */
	zassert_equal(fake.publish_count, 3);
	zassert_str_equal(fake.publishes[2].topic, "g2labs/404cca0ab1f2/availability");
	zassert_str_equal(fake.publishes[2].payload, "offline");
	zassert_true(fake.publishes[2].retained);

	/* Rejoining "home" takes longer than a Broker retry. */
	advance(NODE_BROKER_RETRY_INTERVAL_MS);
	zassert_equal(fake.mqtt_connect_count, 1);
	zassert_equal(fake.publish_count, 3);

	event(NODE_EVENT_WIFI_ASSOCIATED);
	event(NODE_EVENT_WIFI_CONNECTED);
	zassert_equal(fake.mqtt_connect_count, 2);
}

ZTEST(broker, test_changing_the_broker_reconnects_with_the_new_settings)
{
	connect_to_broker();

	set_broker("10.0.0.5", 8883, "other", "secret456");

	zassert_equal(fake.mqtt_disconnect_count, 1);
	zassert_equal(fake.mqtt_connect_count, 2);
	zassert_str_equal(fake.last_mqtt_connect.host, "10.0.0.5");
	zassert_equal(fake.last_mqtt_connect.port, 8883);
	zassert_str_equal(fake.last_mqtt_connect.username, "other");
	zassert_str_equal(fake.last_mqtt_connect.password, "secret456");
}

ZTEST(broker, test_setting_the_same_broker_again_keeps_the_connection)
{
	connect_to_broker();

	set_broker("broker.lan", 1883, "node", "secret123");

	zassert_equal(fake.mqtt_disconnect_count, 0);
	zassert_equal(fake.mqtt_connect_count, 1);
}

ZTEST(broker, test_clearing_the_broker_disconnects_for_good)
{
	connect_to_broker();

	set_broker("", 1883, "", "");
	advance(NODE_BROKER_RETRY_INTERVAL_MS);

	zassert_equal(fake.mqtt_disconnect_count, 1);
	zassert_equal(fake.mqtt_connect_count, 1);
}

ZTEST(broker, test_without_a_username_the_node_connects_anonymously)
{
	set_broker("broker.lan", 1883, "", "");

	join_home();

	zassert_equal(fake.mqtt_connect_count, 1);
	zassert_false(fake.last_mqtt_connect.has_username);
	zassert_false(fake.last_mqtt_connect.has_password);
}

ZTEST(broker, test_without_a_broker_the_node_stays_off_mqtt)
{
	join_home();
	advance(NODE_STATUS_INTERVAL_MS);

	zassert_equal(fake.mqtt_connect_count, 0);
	zassert_equal(fake.publish_count, 0);
}

ZTEST(broker, test_invalid_broker_settings_are_rejected_and_the_old_ones_kept)
{
	struct node_broker broker = {.port = 1883};

	set_broker("broker.lan", 1883, "node", "secret123");

	memset(broker.host, 'h', NODE_BROKER_HOST_MAX_LEN + 1);
	zassert_equal(node_core_set_broker(&core, &broker), -EINVAL, "host too long");

	strcpy(broker.host, "broker.lan");
	broker.port = 0;
	zassert_equal(node_core_set_broker(&core, &broker), -EINVAL, "port 0");

	broker.port = 1883;
	strcpy(broker.password, "secret123");
	zassert_equal(node_core_set_broker(&core, &broker), -EINVAL, "password, no username");

	zassert_str_equal(node_core_broker(&core)->username, "node");
	zassert_str_equal(node_core_broker(&core)->password, "secret123");
}

ZTEST(broker, test_status_carries_the_friendly_name)
{
	zassert_ok(node_core_set_friendly_name(&core, "Kitchen \"north\""));

	connect_to_broker();

	zassert_str_equal(node_core_friendly_name(&core), "Kitchen \"north\"");
	zassert_not_null(strstr(fake.publishes[1].payload,
				"\"node_id\":\"404cca0ab1f2\",\"name\":\"Kitchen \\\"north\\\"\","),
			 "%s", fake.publishes[1].payload);
}

ZTEST(broker, test_a_friendly_name_too_long_is_rejected)
{
	char name[NODE_FRIENDLY_NAME_MAX_LEN + 2];

	memset(name, 'n', sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';

	zassert_equal(node_core_set_friendly_name(&core, name), -EINVAL);
	zassert_str_equal(node_core_friendly_name(&core), "");
}

ZTEST(broker, test_status_reports_a_null_rssi_when_the_link_cannot_tell)
{
	fake.rssi_result = -EIO;

	connect_to_broker();

	zassert_not_null(strstr(fake.publishes[1].payload, "\"rssi\":null,"), "%s",
			 fake.publishes[1].payload);
}

ZTEST(broker, test_a_new_node_has_no_broker_on_the_default_port)
{
	zassert_str_equal(node_core_broker(&core)->host, "");
	zassert_equal(node_core_broker(&core)->port, NODE_BROKER_DEFAULT_PORT);
}

ZTEST(broker, test_leaving_a_broker_on_purpose_publishes_offline_first)
{
	connect_to_broker();

	set_broker("", 1883, "", "");

	zassert_equal(fake.publish_count, 3);
	zassert_str_equal(fake.publishes[2].topic, "g2labs/404cca0ab1f2/availability");
	zassert_str_equal(fake.publishes[2].payload, "offline");
	zassert_true(fake.publishes[2].retained);
	zassert_equal(fake.mqtt_disconnect_count, 1);
}
