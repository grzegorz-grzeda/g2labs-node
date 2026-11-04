/* SPDX-License-Identifier: MIT */
#include "adapters.h"

#include <string.h>

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/logging/log.h>
#include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt.h>

LOG_MODULE_REGISTER(image_adapter, LOG_LEVEL_INF);

int image_adapter_confirm(void *ctx)
{
	ARG_UNUSED(ctx);

	if (boot_is_img_confirmed()) {
		return 0;
	}

	int err = boot_write_img_confirmed();

	if (err) {
		LOG_ERR("image confirm failed: %d", err);
	} else {
		LOG_INF("image confirmed");
	}
	return err;
}

size_t image_adapter_list(struct image_adapter_slot slots[IMAGE_ADAPTER_SLOTS])
{
	size_t count = 0;

	BUILD_ASSERT(IMAGE_ADAPTER_VERSION_LEN + 1 >= IMG_MGMT_VER_MAX_STR_LEN);
	/* img_mgmt writes a SHA-512 hash when MCUboot uses one. */
	BUILD_ASSERT(!IS_ENABLED(CONFIG_MCUBOOT_BOOTLOADER_USES_SHA512));

	for (int slot = 0; slot < IMAGE_ADAPTER_SLOTS; slot++) {
		struct image_adapter_slot *s = &slots[count];
		struct image_version ver;

		memset(s, 0, sizeof(*s));
		/* Fails for an empty or invalid slot, which SMP doesn't list either. */
		if (img_mgmt_read_info(slot, &ver, s->hash, NULL) != 0) {
			continue;
		}

		uint8_t flags = img_mgmt_state_flags(slot);

		s->slot = slot;
		img_mgmt_ver_str(&ver, s->version);
		s->active = flags & IMG_MGMT_STATE_F_ACTIVE;
		s->confirmed = flags & IMG_MGMT_STATE_F_CONFIRMED;
		s->pending = flags & IMG_MGMT_STATE_F_PENDING;
		s->permanent = flags & IMG_MGMT_STATE_F_PERMANENT;
		count++;
	}
	return count;
}
