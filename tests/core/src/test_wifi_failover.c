/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <string.h>

#include <zephyr/ztest.h>

#include <node/core.h>

#include "fake_ports.h"

static struct fake_ports fake;
static struct node_core core;
static const struct node_info info = {.fw_version = "1.2.3"};
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

static void network_heard(const char *ssid, int rssi)
{
	struct node_event ev = {.type = NODE_EVENT_WIFI_NETWORK_HEARD};

	strncpy(ev.network_heard.ssid, ssid, NODE_SSID_MAX_LEN);
	ev.network_heard.rssi = rssi;
	node_core_handle_event(&core, &ev);
}

static void wifi_event(enum node_event_type type)
{
	node_core_handle_event(&core, &(struct node_event){.type = type});
}

/* Start with "home" and "office" known, and get onto "home". */
static void connect_to_home(void)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	zassert_ok(node_core_add_known_network(&core, "office", "password2"));
	node_core_start(&core);
	network_heard("home", -50);
	network_heard("office", -70);
	wifi_event(NODE_EVENT_WIFI_SCAN_DONE);
	wifi_event(NODE_EVENT_WIFI_ASSOCIATED);
	wifi_event(NODE_EVENT_WIFI_CONNECTED);
	zassert_str_equal(node_core_current_network(&core), "home");
}

ZTEST_SUITE(wifi_failover, NULL, NULL, before_each, NULL, NULL);

ZTEST(wifi_failover, test_a_link_drop_rejoins_the_current_network_first)
{
	connect_to_home();

	wifi_event(NODE_EVENT_WIFI_DISCONNECTED);

	zassert_equal(fake.wifi_connect_count, 2);
	zassert_str_equal(fake.last_connect_ssid, "home");
	zassert_equal(fake.wifi_scan_count, 1);
}

ZTEST(wifi_failover, test_a_rejoin_that_times_out_moves_to_the_strongest_other_network)
{
	connect_to_home();
	wifi_event(NODE_EVENT_WIFI_DISCONNECTED);

	advance(NODE_WIFI_RECONNECT_TIMEOUT_MS - 1);
	zassert_equal(fake.wifi_scan_count, 1);

	advance(1);
	zassert_equal(fake.wifi_disconnect_count, 1);
	zassert_equal(fake.wifi_scan_count, 2);

	network_heard("home", -40);
	network_heard("office", -70);
	wifi_event(NODE_EVENT_WIFI_SCAN_DONE);

	zassert_str_equal(fake.last_connect_ssid, "office");
}

ZTEST(wifi_failover, test_no_ip_address_within_the_dhcp_timeout_moves_to_another_network)
{
	connect_to_home();
	wifi_event(NODE_EVENT_WIFI_DISCONNECTED);

	/* Associating late is fine: from here DHCP gets its own timeout. */
	advance(NODE_WIFI_RECONNECT_TIMEOUT_MS - 1);
	wifi_event(NODE_EVENT_WIFI_ASSOCIATED);
	advance(NODE_WIFI_DHCP_TIMEOUT_MS - 1);
	zassert_equal(fake.wifi_scan_count, 1);

	advance(1);
	zassert_equal(fake.wifi_disconnect_count, 1);
	zassert_equal(fake.wifi_scan_count, 2);

	network_heard("home", -40);
	network_heard("office", -70);
	wifi_event(NODE_EVENT_WIFI_SCAN_DONE);

	zassert_str_equal(fake.last_connect_ssid, "office");
}

ZTEST(wifi_failover, test_with_no_known_network_reachable_the_node_rescans_until_one_returns)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	node_core_start(&core);
	network_heard("neighbour", -40);
	wifi_event(NODE_EVENT_WIFI_SCAN_DONE);

	advance(NODE_WIFI_RESCAN_INTERVAL_MS - 1);
	zassert_equal(fake.wifi_scan_count, 1);
	advance(1);
	zassert_equal(fake.wifi_scan_count, 2);

	wifi_event(NODE_EVENT_WIFI_SCAN_DONE);
	advance(NODE_WIFI_RESCAN_INTERVAL_MS);
	zassert_equal(fake.wifi_scan_count, 3);
	zassert_equal(fake.wifi_connect_count, 0);

	network_heard("home", -60);
	wifi_event(NODE_EVENT_WIFI_SCAN_DONE);

	zassert_equal(fake.wifi_connect_count, 1);
	zassert_str_equal(fake.last_connect_ssid, "home");
}

ZTEST(wifi_failover, test_a_failed_rejoin_is_retried_until_the_reconnect_timeout)
{
	connect_to_home();
	wifi_event(NODE_EVENT_WIFI_DISCONNECTED);

	/* The access point is still coming back: the rejoin fails at once. */
	wifi_event(NODE_EVENT_WIFI_DISCONNECTED);
	advance(NODE_WIFI_RETRY_INTERVAL_MS);
	zassert_equal(fake.wifi_connect_count, 3);
	zassert_str_equal(fake.last_connect_ssid, "home");

	wifi_event(NODE_EVENT_WIFI_ASSOCIATED);
	wifi_event(NODE_EVENT_WIFI_CONNECTED);

	zassert_str_equal(node_core_current_network(&core), "home");
	zassert_equal(fake.wifi_scan_count, 1);
}

ZTEST(wifi_failover, test_a_rejoin_that_keeps_failing_moves_to_another_network)
{
	connect_to_home();
	wifi_event(NODE_EVENT_WIFI_DISCONNECTED);

	for (int64_t t = 0; t < NODE_WIFI_RECONNECT_TIMEOUT_MS; t += NODE_WIFI_RETRY_INTERVAL_MS) {
		wifi_event(NODE_EVENT_WIFI_DISCONNECTED);
		advance(NODE_WIFI_RETRY_INTERVAL_MS);
	}
	zassert_equal(fake.wifi_scan_count, 2);

	network_heard("office", -70);
	wifi_event(NODE_EVENT_WIFI_SCAN_DONE);

	zassert_str_equal(fake.last_connect_ssid, "office");
}

ZTEST(wifi_failover, test_a_healthy_node_never_roams_to_a_stronger_known_network)
{
	connect_to_home();

	for (int i = 0; i < 10; i++) {
		advance(NODE_WIFI_RESCAN_INTERVAL_MS);
	}
	network_heard("office", -20);
	wifi_event(NODE_EVENT_WIFI_SCAN_DONE);

	zassert_equal(fake.wifi_scan_count, 1);
	zassert_equal(fake.wifi_connect_count, 1);
	zassert_equal(fake.wifi_disconnect_count, 0);
	zassert_str_equal(node_core_current_network(&core), "home");
}

ZTEST(wifi_failover, test_a_scan_that_never_finishes_is_given_up)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	node_core_start(&core);
	network_heard("home", -60);

	/* SCAN_DONE never comes: join from what was heard. */
	advance(NODE_WIFI_SCAN_TIMEOUT_MS);

	zassert_equal(fake.wifi_connect_count, 1);
	zassert_str_equal(fake.last_connect_ssid, "home");
}

ZTEST(wifi_failover, test_a_refused_scan_is_retried_shortly)
{
	connect_to_home();
	wifi_event(NODE_EVENT_WIFI_DISCONNECTED);

	/* Still busy leaving the old network when asked to scan. */
	fake.wifi_scan_result = -EAGAIN;
	advance(NODE_WIFI_RECONNECT_TIMEOUT_MS);
	zassert_equal(fake.wifi_scan_count, 2);

	fake.wifi_scan_result = 0;
	advance(NODE_WIFI_RETRY_INTERVAL_MS);
	zassert_equal(fake.wifi_scan_count, 3);
}

ZTEST(wifi_failover, test_a_refused_rejoin_moves_to_another_network_at_once)
{
	connect_to_home();

	fake.wifi_connect_result = -EALREADY;
	wifi_event(NODE_EVENT_WIFI_DISCONNECTED);
	zassert_equal(fake.wifi_scan_count, 2);

	fake.wifi_connect_result = 0;
	network_heard("home", -40);
	network_heard("office", -70);
	wifi_event(NODE_EVENT_WIFI_SCAN_DONE);

	zassert_str_equal(fake.last_connect_ssid, "office");
}
