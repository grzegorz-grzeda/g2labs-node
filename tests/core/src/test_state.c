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

/* Advance a second at a time, as the main loop does. */
static void run_for(int64_t ms)
{
	for (int64_t t = 0; t < ms; t += 1000) {
		now_ms += ms - t < 1000 ? ms - t : 1000;
		node_core_tick(&core, now_ms);
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

static void command(const char *payload)
{
	struct node_event e = {.type = NODE_EVENT_COMMAND};

	e.command.len = strlen(payload);
	memcpy(e.command.payload, payload, e.command.len);
	node_core_handle_event(&core, &e);
}

/* Know "home" (and "office" unless only_home), set the Broker, get onto "home". */
static void join_home(bool only_home)
{
	struct node_broker broker = {.port = 1883};

	strcpy(broker.host, "broker.lan");
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	if (!only_home) {
		zassert_ok(node_core_add_known_network(&core, "office", "password2"));
	}
	zassert_ok(node_core_set_broker(&core, &broker));
	node_core_start(&core);
	network_heard("home", -50);
	network_heard("office", -70);
	event(NODE_EVENT_WIFI_SCAN_DONE);
	event(NODE_EVENT_WIFI_ASSOCIATED);
	event(NODE_EVENT_WIFI_CONNECTED);
}

static struct node_state state(void)
{
	struct node_state s;

	node_core_state(&core, &s);
	return s;
}

ZTEST_SUITE(state, NULL, NULL, before_each, NULL, NULL);

ZTEST(state, test_a_node_without_settings_is_idle_everywhere)
{
	node_core_start(&core);
	struct node_state s = state();

	zassert_equal(s.wifi, NODE_WIFI_IDLE);
	zassert_str_equal(s.ssid, "");
	zassert_equal(s.broker, NODE_BROKER_IDLE);
	zassert_equal(s.broker_connected_ms, 0);
	zassert_equal(s.broker_unreachable_ms, 0);
	zassert_equal(s.failover_in_ms, -1);
	zassert_false(s.image_confirmed);
	zassert_false(s.reboot_pending);
}

ZTEST(state, test_the_wifi_phase_and_network_follow_the_join)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	node_core_start(&core);
	zassert_equal(state().wifi, NODE_WIFI_SCANNING);
	zassert_str_equal(state().ssid, "");

	network_heard("home", -50);
	event(NODE_EVENT_WIFI_SCAN_DONE);
	zassert_equal(state().wifi, NODE_WIFI_JOINING);
	zassert_str_equal(state().ssid, "home");

	event(NODE_EVENT_WIFI_DISCONNECTED);
	zassert_equal(state().wifi, NODE_WIFI_JOIN_RETRY_WAIT);
	zassert_str_equal(state().ssid, "home");

	run_for(NODE_WIFI_RETRY_INTERVAL_MS);
	zassert_equal(state().wifi, NODE_WIFI_JOINING);
	event(NODE_EVENT_WIFI_ASSOCIATED);
	zassert_equal(state().wifi, NODE_WIFI_AWAITING_IP);

	event(NODE_EVENT_WIFI_CONNECTED);
	zassert_equal(state().wifi, NODE_WIFI_CONNECTED);
	zassert_str_equal(state().ssid, "home");
}

ZTEST(state, test_a_connected_broker_reports_for_how_long)
{
	join_home(false);
	event(NODE_EVENT_BROKER_CONNECTED);
	run_for(5000);
	struct node_state s = state();

	zassert_equal(s.broker, NODE_BROKER_CONNECTED);
	zassert_equal(s.broker_connected_ms, 5000);
	zassert_equal(s.broker_unreachable_ms, 0);
	zassert_equal(s.failover_in_ms, -1);
	zassert_true(s.image_confirmed);
}

ZTEST(state, test_an_unreachable_broker_counts_down_to_failover)
{
	join_home(false);
	run_for(30000);
	struct node_state s = state();

	zassert_true(s.broker == NODE_BROKER_CONNECTING || s.broker == NODE_BROKER_RETRY_WAIT);
	zassert_equal(s.broker_unreachable_ms, 30000);
	zassert_equal(s.failover_in_ms, NODE_BROKER_FAILOVER_TIMEOUT_MS - 30000);
	zassert_false(s.image_confirmed);
}

ZTEST(state, test_a_lost_broker_counts_from_when_it_was_lost)
{
	join_home(false);
	event(NODE_EVENT_BROKER_CONNECTED);
	run_for(60000);
	event(NODE_EVENT_BROKER_DISCONNECTED);
	run_for(20000);
	struct node_state s = state();

	zassert_true(s.broker == NODE_BROKER_CONNECTING || s.broker == NODE_BROKER_RETRY_WAIT);
	zassert_equal(s.broker_connected_ms, 0);
	zassert_equal(s.broker_unreachable_ms, 20000);
	zassert_equal(s.failover_in_ms, NODE_BROKER_FAILOVER_TIMEOUT_MS - 20000);
}

ZTEST(state, test_with_one_known_network_there_is_no_failover_and_the_outage_keeps_counting)
{
	join_home(true);
	run_for(NODE_BROKER_FAILOVER_TIMEOUT_MS + 10000);
	struct node_state s = state();

	zassert_str_equal(s.ssid, "home");
	zassert_equal(s.broker_unreachable_ms, NODE_BROKER_FAILOVER_TIMEOUT_MS + 10000);
	zassert_equal(s.failover_in_ms, -1);
}

ZTEST(state, test_a_reboot_command_is_reported_as_pending)
{
	join_home(false);
	event(NODE_EVENT_BROKER_CONNECTED);
	command("{\"command\":\"reboot\"}");

	zassert_true(state().reboot_pending);
}
