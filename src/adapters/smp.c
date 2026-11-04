/* SPDX-License-Identifier: MIT */
/*
 * SMP adapter: remote management over SMP-UDP (port 1337) with the image, OS,
 * shell and settings groups, all set up by Kconfig. This adapter only guards
 * settings access: SMP-UDP has no authentication (docs/management.md), so it
 * must never hand out a password.
 */
#include "adapters.h"

#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt_defines.h>
/* Needs callbacks.h first. */
#include <zephyr/mgmt/mcumgr/grp/settings_mgmt/settings_mgmt_callbacks.h>

LOG_MODULE_REGISTER(smp_adapter, LOG_LEVEL_INF);

/* "node" itself or any key below it. */
static bool is_node_key(const char *name)
{
	return name != NULL && (strcmp(name, "node") == 0 || strncmp(name, "node/", 5) == 0);
}

static bool is_password(const char *name)
{
	const char *slash = strrchr(name, '/');

	return strcmp(slash != NULL ? slash + 1 : name, "password") == 0;
}

static enum mgmt_cb_return settings_access(uint32_t event, enum mgmt_cb_return prev_status,
					   int32_t *rc, uint16_t *group, bool *abort_more,
					   void *data, size_t data_size)
{
	ARG_UNUSED(event);
	ARG_UNUSED(prev_status);
	ARG_UNUSED(group);
	ARG_UNUSED(abort_more);
	ARG_UNUSED(data_size);

	const struct settings_mgmt_access *access = data;
	const char *name = access->name;
	bool refuse = false;

	switch (access->access) {
	case SETTINGS_ACCESS_READ:
		/* Any password, under node/ or not. */
		refuse = name != NULL && is_password(name);
		break;
	case SETTINGS_ACCESS_DELETE:
		/*
		 * A delete would change storage behind the running core; writing
		 * an empty value clears a key in both.
		 */
		refuse = is_node_key(name);
		break;
	case SETTINGS_ACCESS_LOAD:
		/*
		 * settings_load() runs the "node" handler holding the settings
		 * lock, the reverse of the core-then-settings lock order. Settings
		 * are loaded at boot and saved on every change anyway.
		 */
		refuse = true;
		break;
	default:
		break;
	}
	if (refuse) {
		LOG_WRN("SMP settings access %d to %s refused", access->access,
			name != NULL ? name : "(all)");
		*rc = MGMT_ERR_EACCESSDENIED;
		return MGMT_CB_ERROR_RC;
	}
	return MGMT_CB_OK;
}

static struct mgmt_callback settings_access_cb = {
	.callback = settings_access,
	.event_id = MGMT_EVT_OP_SETTINGS_MGMT_ACCESS,
};

void smp_adapter_init(void)
{
	mgmt_callback_register(&settings_access_cb);
}
