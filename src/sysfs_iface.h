#ifndef SYSFS_IFACE_H_
#define SYSFS_IFACE_H_

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 */

int sysfs_probe(struct i2c_client* i2c_client);
void sysfs_shutdown(struct i2c_client* i2c_client);

#endif
