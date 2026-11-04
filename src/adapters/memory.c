/* SPDX-License-Identifier: MIT */
/* Memory adapter: free heap, summed over every sys_heap (system heap, libc). */
#include "adapters.h"

#include <zephyr/sys/sys_heap.h>
#include <zephyr/sys/util.h>

size_t memory_adapter_free(void *ctx)
{
	ARG_UNUSED(ctx);

	struct sys_heap **heaps;
	int count = sys_heap_array_get(&heaps);
	size_t free_bytes = 0;

	for (int i = 0; i < count; i++) {
		struct sys_memory_stats stats;

		if (sys_heap_runtime_stats_get(heaps[i], &stats) == 0) {
			free_bytes += stats.free_bytes;
		}
	}
	return free_bytes;
}
