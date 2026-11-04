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
 *
 * At boot the values are read straight from storage into the core. The "node"
 * settings handler below serves the same keys at runtime, to the settings
 * management group over SMP: reads come from the core, passwords never;
 * writes are staged and applied to the core and saved on commit.
 */
#include "adapters.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/settings/settings.h>

#include "../app.h"

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

/* Whether the core holds the stored settings, so runtime access may use it. */
static atomic_t runtime_ready;

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

	atomic_set(&runtime_ready, 1);

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

/*
 * Runtime access to the "node" subtree, for settings management over SMP.
 *
 * Keys are relative to "node/": name, broker/host, broker/port,
 * broker/username, broker/password, kn/<i>/ssid and kn/<i>/password (i from 0
 * to NODE_KNOWN_NETWORKS_MAX - 1). An empty value clears a key; an empty SSID
 * removes that Known Network.
 *
 * Writes are staged on a copy of the core's settings and reach the core only
 * on commit, so a Known Network's SSID and password or the Broker's host and
 * port change together. Commit applies each changed group through the core's
 * public API, which validates it, and saves what the core then holds, so
 * storage always matches the running Node.
 *
 * Lock order is the core lock, then the settings subsystem's lock (the shell
 * saves while holding the core). settings_load() calls these handlers holding
 * the settings lock, so the SMP adapter refuses it; nothing else calls it.
 */

static struct {
	bool active; /* the copy below was taken from the core */
	bool name_changed;
	bool broker_changed;
	bool networks_changed;
	char name[NODE_FRIENDLY_NAME_MAX_LEN + 1];
	struct node_broker broker;
	struct node_known_network networks[NODE_KNOWN_NETWORKS_MAX];
} staged;

static void stage_from_core(const struct node_core *core)
{
	if (staged.active) {
		return;
	}
	memset(&staged, 0, sizeof(staged));
	strcpy(staged.name, node_core_friendly_name(core));
	staged.broker = *node_core_broker(core);
	for (size_t i = 0; i < NODE_KNOWN_NETWORKS_MAX; i++) {
		const struct node_known_network *kn = node_core_known_network(core, i);

		if (kn != NULL) {
			staged.networks[i] = *kn;
		}
	}
	staged.active = true;
}

/*
 * Parse "kn/<i>/<field>" into the index and the field. Returns false for any
 * other key.
 */
static bool parse_network_key(const char *name, size_t *i, const char **field)
{
	if (strncmp(name, "kn/", 3) != 0) {
		return false;
	}

	const char *next = name + 3;
	char *end;
	unsigned long index = strtoul(next, &end, 10);

	if (end == next || *end != '/' || index >= NODE_KNOWN_NETWORKS_MAX) {
		return false;
	}
	*i = index;
	*field = end + 1;
	return true;
}

static bool is_password_key(const char *name)
{
	const char *slash = strrchr(name, '/');

	return strcmp(slash != NULL ? slash + 1 : name, "password") == 0;
}

static int node_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	if (!atomic_get(&runtime_ready) || name == NULL) {
		return -EAGAIN;
	}

	struct node_core *core = app_core_lock();
	struct node_broker *b = &staged.broker;
	const char *field;
	size_t i;
	char port[6];
	int err = -ENOENT;

	stage_from_core(core);
	if (strcmp(name, "name") == 0) {
		err = read_string(staged.name, sizeof(staged.name), len, read_cb, cb_arg);
		staged.name_changed |= err == 0;
	} else if (strcmp(name, "broker/host") == 0) {
		err = read_string(b->host, sizeof(b->host), len, read_cb, cb_arg);
		staged.broker_changed |= err == 0;
	} else if (strcmp(name, "broker/port") == 0) {
		err = read_string(port, sizeof(port), len, read_cb, cb_arg);
		if (err == 0) {
			char *end;
			unsigned long value = port[0] != '\0' ? strtoul(port, &end, 10)
							      : NODE_BROKER_DEFAULT_PORT;

			err = (port[0] == '\0' || *end == '\0') && value > 0 &&
					      value <= UINT16_MAX
				      ? 0
				      : -EINVAL;
			if (err == 0) {
				b->port = (uint16_t)value;
				staged.broker_changed = true;
			}
		}
	} else if (strcmp(name, "broker/username") == 0) {
		err = read_string(b->username, sizeof(b->username), len, read_cb, cb_arg);
		staged.broker_changed |= err == 0;
	} else if (strcmp(name, "broker/password") == 0) {
		err = read_string(b->password, sizeof(b->password), len, read_cb, cb_arg);
		staged.broker_changed |= err == 0;
	} else if (parse_network_key(name, &i, &field)) {
		struct node_known_network *kn = &staged.networks[i];

		if (strcmp(field, "ssid") == 0) {
			err = read_string(kn->ssid, sizeof(kn->ssid), len, read_cb, cb_arg);
		} else if (strcmp(field, "password") == 0) {
			err = read_string(kn->password, sizeof(kn->password), len, read_cb,
					  cb_arg);
		}
		staged.networks_changed |= err == 0;
	}
	app_core_unlock();
	return err;
}

static int copy_value(char *val, int val_len_max, const char *value)
{
	size_t len = strlen(value);

	if (len > (size_t)val_len_max) {
		return -ENOMEM;
	}
	memcpy(val, value, len);
	return (int)len;
}

static int node_settings_get(const char *name, char *val, int val_len_max)
{
	if (!atomic_get(&runtime_ready) || name == NULL) {
		return -EAGAIN;
	}
	/* Passwords are never returned; the SMP adapter refuses them first. */
	if (is_password_key(name)) {
		return -EACCES;
	}

	struct node_core *core = app_core_lock();
	const struct node_broker *b = node_core_broker(core);
	const struct node_known_network *kn;
	const char *field;
	size_t i;
	char port[6];
	int ret = -ENOENT;

	if (strcmp(name, "name") == 0) {
		ret = copy_value(val, val_len_max, node_core_friendly_name(core));
	} else if (strcmp(name, "broker/host") == 0) {
		ret = copy_value(val, val_len_max, b->host);
	} else if (strcmp(name, "broker/port") == 0) {
		snprintf(port, sizeof(port), "%u", b->port);
		ret = copy_value(val, val_len_max, port);
	} else if (strcmp(name, "broker/username") == 0) {
		ret = copy_value(val, val_len_max, b->username);
	} else if (parse_network_key(name, &i, &field) && strcmp(field, "ssid") == 0) {
		kn = node_core_known_network(core, i);
		ret = copy_value(val, val_len_max, kn != NULL ? kn->ssid : "");
	}
	app_core_unlock();
	return ret;
}

/* Make the core's Known Networks the staged ones with an SSID, in index order. */
static int apply_networks(struct node_core *core)
{
	int err = 0;

	for (size_t i = node_core_known_network_count(core); i-- > 0;) {
		const char *ssid = node_core_known_network(core, i)->ssid;
		bool kept = false;

		for (size_t j = 0; j < NODE_KNOWN_NETWORKS_MAX && !kept; j++) {
			kept = strcmp(staged.networks[j].ssid, ssid) == 0;
		}
		if (!kept) {
			(void)node_core_remove_known_network(core, ssid);
		}
	}
	for (size_t j = 0; j < NODE_KNOWN_NETWORKS_MAX; j++) {
		const struct node_known_network *kn = &staged.networks[j];

		if (kn->ssid[0] == '\0') {
			continue;
		}

		int add_err = node_core_add_known_network(core, kn->ssid, kn->password);

		if (add_err) {
			LOG_WRN("Known Network %zu not applied (%d)", j, add_err);
			err = err ? err : add_err;
		}
	}
	return err;
}

static int node_settings_commit(void)
{
	if (!atomic_get(&runtime_ready)) {
		return 0;
	}

	struct node_core *core = app_core_lock();
	int err = 0;
	int step;

	if (staged.active && staged.name_changed) {
		step = node_core_set_friendly_name(core, staged.name);
		step = step ? step : settings_adapter_save_friendly_name(core);
		if (step) {
			LOG_WRN("friendly name not applied (%d)", step);
		}
		err = err ? err : step;
	}
	if (staged.active && staged.broker_changed) {
		step = node_core_set_broker(core, &staged.broker);
		step = step ? step : settings_adapter_save_broker(core);
		if (step) {
			LOG_WRN("Broker not applied (%d)", step);
		}
		err = err ? err : step;
	}
	if (staged.active && staged.networks_changed) {
		step = apply_networks(core);

		int save_err = settings_adapter_save_known_networks(core);

		if (save_err) {
			LOG_WRN("Known Networks not saved (%d)", save_err);
		}
		err = err ? err : step ? step : save_err;
	}
	if (staged.active) {
		LOG_INF("settings committed over SMP%s", err ? ", with errors" : "");
	}
	memset(&staged, 0, sizeof(staged));
	app_core_unlock();
	return err;
}

SETTINGS_STATIC_HANDLER_DEFINE(node, "node", node_settings_get, node_settings_set,
			       node_settings_commit, NULL);
