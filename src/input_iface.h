#ifndef INPUT_IFACE_H_
#define INPUT_IFACE_H_

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 */

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>

#include "registers.h"

struct kbd_ctx
{
	uint8_t version_number;

	struct i2c_client *i2c_client;
};

// Shared global state for global interfaces such as sysfs
extern struct kbd_ctx *g_ctx;

// Public interface

int input_probe(struct i2c_client* i2c_client);
void input_shutdown(struct i2c_client* i2c_client);

// Internal interfaces

// Firmware

int input_fw_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx);
void input_fw_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx);

#endif
