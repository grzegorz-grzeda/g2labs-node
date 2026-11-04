/* SPDX-License-Identifier: MIT */
/*
 * Settings adapter: keeps Known Networks in the settings subsystem (the
 * storage partition), which survives reboots and image updates.
 *
 * Each Known Network is stored as two string values, in the order added:
 *   node/kn/<i>/ssid
 *   node/kn/<i>/password
 * so the stored form doesn't depend on any C struct layout.
 */
#include "adapters.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(settings_adapter, LOG_LEVEL_INF);

#define KNOWN_NETWORKS_KEY "node/kn"
#define KEY_LEN            sizeof(KNOWN_NETWORKS_KEY "/0/password")

static struct node_known_network loaded[NODE_KNOWN_NETWORKS_MAX];

static int read_string(char *dst, size_t dst_size, size_t len, settings_read_cb read_cb,
		       void *cb_arg)
{
	if (len >= dst_size) {
		return -EINVAL;
	}

	ssize_t read = read_cb(cb_arg, dst, len);

	if (read != (ssize_t)len) {
		return -EIO;
	}
	dst[len] = '\0';
	return 0;
}

/* Called for each "<i>/ssid" and "<i>/password" under node/kn. */
static int load_known_network_cb(const char *key, size_t len, settings_read_cb read_cb,
				 void *cb_arg, void *param)
{
	ARG_UNUSED(param);

	const char *field;
	char *end;
	long i = key != NULL ? strtol(key, &end, 10) : -1;

	if (i < 0 || i >= NODE_KNOWN_NETWORKS_MAX || *end != '/') {
		return 0;
	}
	field = end + 1;

	int err = -ENOENT;

	if (strcmp(field, "ssid") == 0) {
		err = read_string(loaded[i].ssid, sizeof(loaded[i].ssid), len, read_cb, cb_arg);
	} else if (strcmp(field, "password") == 0) {
		err = read_string(loaded[i].password, sizeof(loaded[i].password), len, read_cb,
				  cb_arg);
	}
	if (err) {
		LOG_WRN("ignoring stored node/kn/%s (%d)", key, err);
	}
	return 0;
}

int settings_adapter_load_known_networks(struct node_core *core)
{
	int err = settings_subsys_init();

	if (err) {
		LOG_ERR("settings init failed: %d", err);
		return err;
	}

	memset(loaded, 0, sizeof(loaded));
	err = settings_load_subtree_direct(KNOWN_NETWORKS_KEY, load_known_network_cb, NULL);
	if (err) {
		LOG_ERR("loading Known Networks failed: %d", err);
	}

	for (size_t i = 0; i < NODE_KNOWN_NETWORKS_MAX; i++) {
		if (loaded[i].ssid[0] == '\0') {
			continue;
		}
		int add_err = node_core_add_known_network(core, loaded[i].ssid,
							  loaded[i].password);

		if (add_err) {
			LOG_WRN("skipping stored Known Network %zu (%d)", i, add_err);
		}
	}
	memset(loaded, 0, sizeof(loaded));
	LOG_INF("%zu Known Network(s) loaded", node_core_known_network_count(core));
	return err;
}

int settings_adapter_save_known_networks(const struct node_core *core)
{
	char key[KEY_LEN];
	int err = 0;

	for (size_t i = 0; i < NODE_KNOWN_NETWORKS_MAX && err == 0; i++) {
		const struct node_known_network *kn = node_core_known_network(core, i);

		snprintf(key, sizeof(key), KNOWN_NETWORKS_KEY "/%zu/ssid", i);
		err = kn ? settings_save_one(key, kn->ssid, strlen(kn->ssid))
			 : settings_delete(key);
		if (err == 0) {
			snprintf(key, sizeof(key), KNOWN_NETWORKS_KEY "/%zu/password", i);
			err = kn ? settings_save_one(key, kn->password, strlen(kn->password))
				 : settings_delete(key);
		}
	}
	return err;
}
