/* SPDX-License-Identifier: MIT */
/*
 * Settings adapter: keeps the Node's settings in the settings subsystem (the
 * storage partition), which survives reboots and image updates.
 *
 * Every value is a string, so the stored form doesn't depend on any C struct
 * layout:
 *   node/kn/<i>/ssid, node/kn/<i>/password   Known Networks, in the order added
 *   node/broker/host, node/broker/port        the Broker (port in decimal)
 *   node/broker/username, node/broker/password
 *   node/name                                 the friendly name
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

static int load_known_networks(struct node_core *core)
{
	memset(loaded, 0, sizeof(loaded));
	int err = settings_load_subtree_direct(KNOWN_NETWORKS_KEY, load_known_network_cb, NULL);
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

#define BROKER_KEY "node/broker"
#define NAME_KEY   "node/name"

static struct node_broker loaded_broker;

/* Called for each value under node/broker. */
static int load_broker_cb(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg,
			  void *param)
{
	ARG_UNUSED(param);

	struct node_broker *b = &loaded_broker;
	char port[6];
	int err = -ENOENT;

	if (key == NULL) {
		return 0;
	} else if (strcmp(key, "host") == 0) {
		err = read_string(b->host, sizeof(b->host), len, read_cb, cb_arg);
	} else if (strcmp(key, "port") == 0) {
		err = read_string(port, sizeof(port), len, read_cb, cb_arg);
		if (err == 0) {
			char *end;
			unsigned long value = strtoul(port, &end, 10);

			err = *end == '\0' && value <= UINT16_MAX ? 0 : -EINVAL;
			/* An invalid port makes node_core_set_broker() reject it. */
			b->port = err == 0 ? (uint16_t)value : 0;
		}
	} else if (strcmp(key, "username") == 0) {
		err = read_string(b->username, sizeof(b->username), len, read_cb, cb_arg);
	} else if (strcmp(key, "password") == 0) {
		err = read_string(b->password, sizeof(b->password), len, read_cb, cb_arg);
	}
	if (err) {
		LOG_WRN("ignoring stored " BROKER_KEY "/%s (%d)", key, err);
	}
	return 0;
}

static int load_broker(struct node_core *core)
{
	memset(&loaded_broker, 0, sizeof(loaded_broker));
	loaded_broker.port = NODE_BROKER_DEFAULT_PORT;

	int err = settings_load_subtree_direct(BROKER_KEY, load_broker_cb, NULL);

	if (err) {
		LOG_ERR("loading the Broker failed: %d", err);
	}
	if (loaded_broker.host[0] != '\0') {
		int set_err = node_core_set_broker(core, &loaded_broker);

		if (set_err) {
			LOG_WRN("skipping stored Broker (%d)", set_err);
		} else {
			LOG_INF("Broker %s:%u loaded", loaded_broker.host, loaded_broker.port);
		}
	}
	memset(&loaded_broker, 0, sizeof(loaded_broker));
	return err;
}

static char loaded_name[NODE_FRIENDLY_NAME_MAX_LEN + 1];

static int load_name_cb(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg,
			void *param)
{
	ARG_UNUSED(param);

	/* The value of node/name itself has no key below it. */
	if (key != NULL) {
		return 0;
	}
	if (read_string(loaded_name, sizeof(loaded_name), len, read_cb, cb_arg) != 0) {
		LOG_WRN("ignoring stored " NAME_KEY);
		loaded_name[0] = '\0';
	}
	return 0;
}

static int load_friendly_name(struct node_core *core)
{
	loaded_name[0] = '\0';

	int err = settings_load_subtree_direct(NAME_KEY, load_name_cb, NULL);

	if (err) {
		LOG_ERR("loading the friendly name failed: %d", err);
	}
	(void)node_core_set_friendly_name(core, loaded_name);
	return err;
}

int settings_adapter_load(struct node_core *core)
{
	int err = settings_subsys_init();

	if (err) {
		LOG_ERR("settings init failed: %d", err);
		return err;
	}

	int kn_err = load_known_networks(core);
	int broker_err = load_broker(core);
	int name_err = load_friendly_name(core);

	return kn_err ? kn_err : broker_err ? broker_err : name_err;
}

/* Save a string, or delete the key for an empty one. */
static int save_string(const char *key, const char *value)
{
	return value[0] != '\0' ? settings_save_one(key, value, strlen(value))
				: settings_delete(key);
}

int settings_adapter_save_broker(const struct node_core *core)
{
	const struct node_broker *b = node_core_broker(core);
	char port[6];

	snprintf(port, sizeof(port), "%u", b->port);

	int err = save_string(BROKER_KEY "/host", b->host);

	err = err ? err : save_string(BROKER_KEY "/port", port);
	err = err ? err : save_string(BROKER_KEY "/username", b->username);
	err = err ? err : save_string(BROKER_KEY "/password", b->password);
	return err;
}

int settings_adapter_save_friendly_name(const struct node_core *core)
{
	return save_string(NAME_KEY, node_core_friendly_name(core));
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
