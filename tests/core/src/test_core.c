/* SPDX-License-Identifier: MIT */
#include <zephyr/ztest.h>

#include <node/core.h>

#include "fake_ports.h"

static struct fake_ports fake;
static struct node_core core;

static void before_each(void *fixture)
{
	ARG_UNUSED(fixture);
	fake_ports_init(&fake);
}

ZTEST_SUITE(node_core, NULL, NULL, before_each, NULL, NULL);

ZTEST(node_core, test_started_node_without_settings_or_events_stays_silent)
{
	const struct node_info info = {.fw_version = "1.2.3"};

	node_core_init(&core, &fake.ports, &info);
	node_core_start(&core);

	zassert_equal(fake.publish_count, 0, "unexpected MQTT publish");
	zassert_equal(fake.wifi_scan_count, 0, "unexpected Wi-Fi scan");
	zassert_equal(fake.wifi_connect_count, 0, "unexpected Wi-Fi connect");
	zassert_equal(fake.image_confirm_count, 0, "image confirmed without a Broker");
}

ZTEST(node_core, test_node_reports_the_firmware_version_it_was_started_with)
{
	const struct node_info info = {.fw_version = "0.1.0-dev"};

	node_core_init(&core, &fake.ports, &info);
	node_core_start(&core);

	zassert_str_equal(node_core_fw_version(&core), "0.1.0-dev");
}
