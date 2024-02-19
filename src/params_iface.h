
#ifndef PARAMS_IFACE_H_
#define PARAMS_IFACE_H_

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 */

#include "input_iface.h"

int params_probe(void);
void params_shutdown(void);

char const* params_get_sharp_path(void);
uint32_t params_get_sysfs_gid(void);

#endif
