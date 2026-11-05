/* SPDX-License-Identifier: MIT */
/*
 * A new image confirms itself only once it has reached the Broker, so an
 * image that can't get there is reverted by MCUboot on the next reboot.
 */
#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <node/core.h>

#include "fake_ports.h"

static struct fake_ports fake;
static struct node_core core;
static const struct node_info info = {
	.fw_version = "1.2.3",
	.board = "esp32c6_devkitc/esp32c6/hpcore",
	.mac = {0x40, 0x4c, 0xca, 0x0a, 0xb1, 0xf2},
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

static void event(enum node_event_type type)
{
	node_core_handle_event(&core, &(struct node_event){.type = type});
}

static void advance(int64_t ms)
{
	now_ms += ms;
	node_core_tick(&core, now_ms);
}

/* Start with "home" known and a Broker set, and get onto Wi-Fi. */
static void join_home(void)
{
	struct node_event heard = {.type = NODE_EVENT_WIFI_NETWORK_HEARD};
	struct node_broker broker = {.port = 1883};

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
}

ZTEST_SUITE(image_confirm, NULL, NULL, before_each, NULL, NULL);

ZTEST(image_confirm, test_image_is_confirmed_on_the_first_broker_connection)
{
	join_home();
	zassert_equal(fake.mqtt_connect_count, 1);
	zassert_equal(fake.image_confirm_count, 0, "confirmed before reaching the Broker");

	event(NODE_EVENT_BROKER_CONNECTED);

	zassert_equal(fake.image_confirm_count, 1);
}

ZTEST(image_confirm, test_a_broker_that_refuses_or_never_answers_confirms_nothing)
{
	join_home();

	event(NODE_EVENT_BROKER_DISCONNECTED);
	advance(NODE_BROKER_RETRY_INTERVAL_MS);
	zassert_equal(fake.mqtt_connect_count, 2, "no retry after a refused connect");
	advance(NODE_BROKER_CONNECT_TIMEOUT_MS);
	advance(NODE_BROKER_RETRY_INTERVAL_MS);
	zassert_equal(fake.mqtt_connect_count, 3, "no retry after a connect timeout");

	zassert_equal(fake.image_confirm_count, 0, "confirmed without reaching the Broker");
}

ZTEST(image_confirm, test_reconnecting_to_the_broker_does_not_confirm_again)
{
	join_home();
	event(NODE_EVENT_BROKER_CONNECTED);

	event(NODE_EVENT_BROKER_DISCONNECTED);
	advance(NODE_BROKER_RETRY_INTERVAL_MS);
	event(NODE_EVENT_BROKER_CONNECTED);

	zassert_equal(fake.image_confirm_count, 1);
}

ZTEST(image_confirm, test_a_failed_confirm_is_logged_and_retried_on_the_next_connection)
{
	join_home();
	fake.image_confirm_result = -EIO;
	event(NODE_EVENT_BROKER_CONNECTED);
	zassert_equal(fake.image_confirm_count, 1);
	zassert_equal(fake.warning_count, 1, "a failed confirm went unreported");

	fake.image_confirm_result = 0;
	event(NODE_EVENT_BROKER_DISCONNECTED);
	advance(NODE_BROKER_RETRY_INTERVAL_MS);
	event(NODE_EVENT_BROKER_CONNECTED);
	event(NODE_EVENT_BROKER_DISCONNECTED);
	advance(NODE_BROKER_RETRY_INTERVAL_MS);
	event(NODE_EVENT_BROKER_CONNECTED);

	zassert_equal(fake.image_confirm_count, 2);
	zassert_equal(fake.warning_count, 1);
}
