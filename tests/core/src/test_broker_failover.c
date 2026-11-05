/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <node/core.h>

#include "fake_ports.h"

static struct fake_ports fake;
static struct node_core core;
static struct node_info info;
static int64_t now_ms;

static void before_each(void *fixture)
{
	ARG_UNUSED(fixture);
	fake_ports_init(&fake);
	info = (struct node_info){
		.fw_version = "1.2.3",
		.board = "esp32c6_devkitc/esp32c6/hpcore",
	};
	node_core_init(&core, &fake.ports, &info);
	now_ms = 0;
	node_core_tick(&core, now_ms);
}

static void advance(int64_t ms)
{
	now_ms += ms;
	node_core_tick(&core, now_ms);
}

/* Advance a second at a time, as the main loop does. */
static void run_for(int64_t ms)
{
	for (int64_t t = 0; t < ms; t += 1000) {
		advance(ms - t < 1000 ? ms - t : 1000);
	}
}

static void event(enum node_event_type type)
{
	node_core_handle_event(&core, &(struct node_event){.type = type});
}

static void network_heard(const char *ssid, int rssi)
{
	struct node_event ev = {.type = NODE_EVENT_WIFI_NETWORK_HEARD};

	strncpy(ev.network_heard.ssid, ssid, NODE_SSID_MAX_LEN);
	ev.network_heard.rssi = rssi;
	node_core_handle_event(&core, &ev);
}

static void set_broker(void)
{
	struct node_broker broker = {.port = 1883};

	strcpy(broker.host, "broker.lan");
	zassert_ok(node_core_set_broker(&core, &broker));
}

/* Hear home (strongest) and office, finish the scan and join whatever it picks. */
static void scan_and_join(void)
{
	network_heard("home", -50);
	network_heard("office", -70);
	event(NODE_EVENT_WIFI_SCAN_DONE);
	event(NODE_EVENT_WIFI_ASSOCIATED);
	event(NODE_EVENT_WIFI_CONNECTED);
}

/* Know "home" and "office" (unless only_home), set the Broker, get onto "home". */
static void join_home(bool only_home)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	if (!only_home) {
		zassert_ok(node_core_add_known_network(&core, "office", "password2"));
	}
	set_broker();
	node_core_start(&core);
	scan_and_join();
	zassert_str_equal(node_core_current_network(&core), "home");
}

ZTEST_SUITE(broker_failover, NULL, NULL, before_each, NULL, NULL);

ZTEST(broker_failover, test_an_unreachable_broker_moves_the_node_to_the_strongest_other_network)
{
	join_home(false);

	run_for(NODE_BROKER_FAILOVER_TIMEOUT_MS - 1000);
	zassert_equal(fake.wifi_disconnect_count, 0);
	zassert_equal(fake.wifi_scan_count, 1);

	run_for(1000);
	zassert_equal(fake.wifi_disconnect_count, 1);
	zassert_equal(fake.wifi_scan_count, 2);

	/* home is still the strongest, but the Node moves away from it. */
	scan_and_join();
	zassert_str_equal(node_core_current_network(&core), "office");
	zassert_str_equal(fake.last_connect_ssid, "office");
}

ZTEST(broker_failover, test_the_node_retries_the_broker_on_the_network_it_moved_to)
{
	join_home(false);
	run_for(NODE_BROKER_FAILOVER_TIMEOUT_MS);
	size_t connects = fake.mqtt_connect_count;

	scan_and_join();

	zassert_str_equal(node_core_current_network(&core), "office");
	zassert_equal(fake.mqtt_connect_count, connects + 1);
}

ZTEST(broker_failover, test_wifi_glitches_during_an_outage_dont_restart_the_period)
{
	join_home(false);

	for (int i = 0; i < 4; i++) {
		run_for(NODE_BROKER_FAILOVER_TIMEOUT_MS / 4 - 5000);
		/* The link drops and the Node rejoins home straight away. */
		event(NODE_EVENT_WIFI_DISCONNECTED);
		event(NODE_EVENT_WIFI_ASSOCIATED);
		event(NODE_EVENT_WIFI_CONNECTED);
		zassert_equal(fake.wifi_disconnect_count, 0);
	}

	run_for(20000);
	zassert_equal(fake.wifi_disconnect_count, 1);
	zassert_equal(fake.wifi_scan_count, 2);
}

ZTEST(broker_failover, test_with_no_other_network_heard_the_node_rejoins_and_counts_afresh)
{
	join_home(false);
	run_for(NODE_BROKER_FAILOVER_TIMEOUT_MS);

	network_heard("home", -50);
	event(NODE_EVENT_WIFI_SCAN_DONE);
	event(NODE_EVENT_WIFI_ASSOCIATED);
	event(NODE_EVENT_WIFI_CONNECTED);
	zassert_str_equal(node_core_current_network(&core), "home");

	run_for(NODE_BROKER_FAILOVER_TIMEOUT_MS - 1000);
	zassert_equal(fake.wifi_disconnect_count, 1);
	run_for(1000);
	zassert_equal(fake.wifi_disconnect_count, 2);
}

ZTEST(broker_failover, test_a_broker_lost_after_connecting_moves_the_node_after_the_period)
{
	join_home(false);
	run_for(60000);
	event(NODE_EVENT_BROKER_CONNECTED);
	run_for(60000);

	event(NODE_EVENT_BROKER_DISCONNECTED);
	run_for(NODE_BROKER_FAILOVER_TIMEOUT_MS - 1000);
	zassert_equal(fake.wifi_disconnect_count, 0);

	run_for(1000);
	zassert_equal(fake.wifi_disconnect_count, 1);
}

ZTEST(broker_failover, test_shorter_outages_only_reconnect_on_the_same_network)
{
	join_home(false);
	event(NODE_EVENT_BROKER_CONNECTED);

	for (int i = 0; i < 3; i++) {
		size_t connects = fake.mqtt_connect_count;

		event(NODE_EVENT_BROKER_DISCONNECTED);
		run_for(NODE_BROKER_FAILOVER_TIMEOUT_MS - 1000);
		zassert_true(fake.mqtt_connect_count > connects);
		event(NODE_EVENT_BROKER_CONNECTED);
	}

	run_for(NODE_BROKER_FAILOVER_TIMEOUT_MS);
	zassert_equal(fake.wifi_disconnect_count, 0);
	zassert_equal(fake.wifi_scan_count, 1);
	zassert_str_equal(node_core_current_network(&core), "home");
}

ZTEST(broker_failover, test_with_one_known_network_the_node_stays_and_keeps_retrying)
{
	join_home(true);

	run_for(NODE_BROKER_FAILOVER_TIMEOUT_MS);
	size_t connects = fake.mqtt_connect_count;

	run_for(3 * NODE_BROKER_FAILOVER_TIMEOUT_MS);

	zassert_equal(fake.wifi_disconnect_count, 0);
	zassert_equal(fake.wifi_scan_count, 1);
	zassert_true(fake.mqtt_connect_count > connects);
	zassert_str_equal(node_core_current_network(&core), "home");
}

ZTEST(broker_failover, test_without_a_broker_the_node_stays_on_its_network)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	zassert_ok(node_core_add_known_network(&core, "office", "password2"));
	node_core_start(&core);
	scan_and_join();

	run_for(2 * NODE_BROKER_FAILOVER_TIMEOUT_MS);

	zassert_equal(fake.wifi_disconnect_count, 0);
	zassert_equal(fake.wifi_scan_count, 1);
}

ZTEST(broker_failover, test_the_period_is_configurable)
{
	info.broker_failover_ms = 120000;
	node_core_init(&core, &fake.ports, &info);
	node_core_tick(&core, now_ms);
	join_home(false);

	run_for(119000);
	zassert_equal(fake.wifi_disconnect_count, 0);

	run_for(1000);
	zassert_equal(fake.wifi_disconnect_count, 1);
}
