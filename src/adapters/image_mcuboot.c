/* SPDX-License-Identifier: MIT */
#include "adapters.h"

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/logging/log.h>

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
