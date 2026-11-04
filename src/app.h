/* SPDX-License-Identifier: MIT */
#ifndef APP_H
#define APP_H

#include <node/core.h>

/* The running Node core, for management surfaces such as the shell. */
const struct node_core *app_core(void);

#endif /* APP_H */
