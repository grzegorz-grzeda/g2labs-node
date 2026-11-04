/* SPDX-License-Identifier: MIT */
#ifndef NODE_APP_H
#define NODE_APP_H

#include <node/core.h>

/*
 * The running Node core. The core isn't thread-safe: management surfaces such
 * as the shell must hold it with app_core_lock() / app_core_unlock().
 */
struct node_core *app_core_lock(void);
void app_core_unlock(void);

/* Queue a platform event for the core; safe from any thread. */
void app_post_event(const struct node_event *event);

#endif /* NODE_APP_H */
