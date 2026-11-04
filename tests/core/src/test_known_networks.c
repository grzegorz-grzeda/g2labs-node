/* SPDX-License-Identifier: MIT */
#include <string.h>

#include <zephyr/ztest.h>

#include <node/core.h>

#include "fake_ports.h"

static struct fake_ports fake;
static struct node_core core;
static const struct node_info info = {.fw_version = "1.2.3"};

static void before_each(void *fixture)
{
	ARG_UNUSED(fixture);
	fake_ports_init(&fake);
	node_core_init(&core, &fake.ports, &info);
}

static void network_heard(const char *ssid, int rssi)
{
	struct node_event ev = {.type = NODE_EVENT_WIFI_NETWORK_HEARD};

	strncpy(ev.network_heard.ssid, ssid, NODE_SSID_MAX_LEN);
	ev.network_heard.rssi = rssi;
	node_core_handle_event(&core, &ev);
}

static void scan_done(void)
{
	node_core_handle_event(&core, &(struct node_event){.type = NODE_EVENT_WIFI_SCAN_DONE});
}

static void wifi_event(enum node_event_type type)
{
	node_core_handle_event(&core, &(struct node_event){.type = type});
}

ZTEST_SUITE(known_networks, NULL, NULL, before_each, NULL, NULL);

ZTEST(known_networks, test_node_with_a_known_network_scans_at_start)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));

	node_core_start(&core);

	zassert_equal(fake.wifi_scan_count, 1);
	zassert_equal(fake.wifi_connect_count, 0);
}

ZTEST(known_networks, test_node_joins_the_strongest_reachable_known_network)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	zassert_ok(node_core_add_known_network(&core, "office", "password2"));
	node_core_start(&core);

	network_heard("neighbour", -30);
	network_heard("home", -70);
	network_heard("office", -55);
	scan_done();

	zassert_equal(fake.wifi_connect_count, 1);
	zassert_str_equal(fake.last_connect_ssid, "office");
	zassert_str_equal(fake.last_connect_password, "password2");
}

ZTEST(known_networks, test_node_without_a_reachable_known_network_joins_nothing)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	node_core_start(&core);

	network_heard("neighbour", -30);
	network_heard("cafe", -60);
	scan_done();

	zassert_equal(fake.wifi_connect_count, 0);
}

ZTEST(known_networks, test_a_sixth_known_network_is_rejected)
{
	static const char *const ssids[] = {"one", "two", "three", "four", "five"};

	ARRAY_FOR_EACH(ssids, i) {
		zassert_ok(node_core_add_known_network(&core, ssids[i], "password1"));
	}

	zassert_equal(node_core_add_known_network(&core, "six", "password1"), -ENOSPC);
	zassert_equal(node_core_known_network_count(&core), 5);
}

ZTEST(known_networks, test_adding_a_known_network_while_offline_scans_again)
{
	node_core_start(&core);
	zassert_equal(fake.wifi_scan_count, 0);

	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	zassert_equal(fake.wifi_scan_count, 1);

	network_heard("home", -60);
	scan_done();
	zassert_str_equal(fake.last_connect_ssid, "home");
}

ZTEST(known_networks, test_joined_network_is_current_only_once_connected)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	node_core_start(&core);
	network_heard("home", -60);
	scan_done();

	zassert_is_null(node_core_current_network(&core));

	wifi_event(NODE_EVENT_WIFI_CONNECTED);
	zassert_str_equal(node_core_current_network(&core), "home");

	wifi_event(NODE_EVENT_WIFI_DISCONNECTED);
	zassert_is_null(node_core_current_network(&core));
}

ZTEST(known_networks, test_adding_a_known_network_while_connected_does_not_scan)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	node_core_start(&core);
	network_heard("home", -60);
	scan_done();
	wifi_event(NODE_EVENT_WIFI_CONNECTED);

	zassert_ok(node_core_add_known_network(&core, "office", "password2"));

	zassert_equal(fake.wifi_scan_count, 1);
}

ZTEST(known_networks, test_a_removed_known_network_is_no_longer_joined)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	zassert_ok(node_core_add_known_network(&core, "office", "password2"));

	zassert_ok(node_core_remove_known_network(&core, "home"));
	zassert_equal(node_core_remove_known_network(&core, "home"), -ENOENT);
	node_core_start(&core);
	network_heard("home", -40);
	network_heard("office", -70);
	scan_done();

	zassert_equal(node_core_known_network_count(&core), 1);
	zassert_str_equal(fake.last_connect_ssid, "office");
}

ZTEST(known_networks, test_adding_a_known_ssid_again_changes_its_password)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	zassert_ok(node_core_add_known_network(&core, "home", "password2"));
	node_core_start(&core);
	network_heard("home", -40);
	scan_done();

	zassert_equal(node_core_known_network_count(&core), 1);
	zassert_str_equal(fake.last_connect_password, "password2");
}

ZTEST(known_networks, test_invalid_known_networks_are_rejected)
{
	zassert_equal(node_core_add_known_network(&core, "", "password1"), -EINVAL);
	zassert_equal(node_core_add_known_network(&core, "ssid-that-is-longer-than-32-bytes", ""),
		      -EINVAL);
	zassert_equal(node_core_add_known_network(&core, "home", "short"), -EINVAL);
	zassert_equal(node_core_add_known_network(&core, "home",
		"password-that-is-longer-than-sixty-four-characters-which-wpa-forbids-xx"), -EINVAL);
	zassert_equal(node_core_known_network_count(&core), 0);

	zassert_ok(node_core_add_known_network(&core, "open-cafe", ""));
}

ZTEST(known_networks, test_known_networks_are_listed_in_the_order_added)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	zassert_ok(node_core_add_known_network(&core, "office", "password2"));

	zassert_str_equal(node_core_known_network(&core, 0)->ssid, "home");
	zassert_str_equal(node_core_known_network(&core, 1)->ssid, "office");
	zassert_is_null(node_core_known_network(&core, 2));
}

ZTEST(known_networks, test_removing_a_known_network_during_a_scan_joins_one_still_known)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	zassert_ok(node_core_add_known_network(&core, "office", "password2"));
	node_core_start(&core);
	network_heard("office", -50);
	network_heard("home", -60);

	zassert_ok(node_core_remove_known_network(&core, "office"));
	scan_done();

	zassert_equal(fake.wifi_connect_count, 1);
	zassert_str_equal(fake.last_connect_ssid, "home");
}

ZTEST(known_networks, test_scan_events_outside_a_scan_are_ignored)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	node_core_start(&core);
	network_heard("home", -60);
	scan_done();
	wifi_event(NODE_EVENT_WIFI_CONNECTED);

	network_heard("home", -40);
	scan_done();

	zassert_equal(fake.wifi_connect_count, 1);
	zassert_str_equal(node_core_current_network(&core), "home");
}

ZTEST(known_networks, test_connected_without_a_join_is_ignored)
{
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	node_core_start(&core);

	wifi_event(NODE_EVENT_WIFI_CONNECTED);

	zassert_is_null(node_core_current_network(&core));
}

ZTEST(known_networks, test_a_refused_scan_request_lets_the_next_change_scan_again)
{
	fake.wifi_scan_result = -EIO;
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	node_core_start(&core);

	fake.wifi_scan_result = 0;
	zassert_ok(node_core_add_known_network(&core, "office", "password2"));

	zassert_equal(fake.wifi_scan_count, 2);
}

ZTEST(known_networks, test_a_refused_join_request_lets_the_next_change_scan_again)
{
	fake.wifi_connect_result = -EIO;
	zassert_ok(node_core_add_known_network(&core, "home", "password1"));
	node_core_start(&core);
	network_heard("home", -60);
	scan_done();

	zassert_ok(node_core_add_known_network(&core, "office", "password2"));

	zassert_equal(fake.wifi_scan_count, 2);
}

ZTEST(known_networks, test_a_64_character_password_must_be_a_hex_key)
{
	static const char hex_key[] =
		"00112233445566778899aabbccddeeff00112233445566778899AABBCCDDEEFF";
	static const char not_hex[] =
		"this-passphrase-is-exactly-sixty-four-characters-long-not-hex-xx";

	zassert_equal(strlen(hex_key), 64);
	zassert_equal(strlen(not_hex), 64);
	zassert_ok(node_core_add_known_network(&core, "home", hex_key));
	zassert_equal(node_core_add_known_network(&core, "office", not_hex), -EINVAL);
}
