// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * input_iface.c: Key handler implementation
 */

#include <linux/input.h>
#include <linux/module.h>

#include "config.h"
#include "debug_levels.h"

#include "params_iface.h"
#include "input_iface.h"

#include "i2c_helper.h"

#include "bbq20kbd_pmod_codes.h"

// Global keyboard context and sysfs data
struct kbd_ctx *g_ctx = NULL;

int input_probe(struct i2c_client* i2c_client)
{
	int rc;

	// Allocate keyboard context (managed by device lifetime)
	g_ctx = devm_kzalloc(&i2c_client->dev, sizeof(*g_ctx), GFP_KERNEL);
	if (!g_ctx) {
		return -ENOMEM;
	}

	// Initialize keyboard context
	g_ctx->i2c_client = i2c_client;

	// Run subsystem probes
	if ((rc = input_fw_probe(i2c_client, g_ctx))) {
		dev_err(&i2c_client->dev, "emate-kbd: input_fw_probe failed\n");
		return rc;
	}

	return 0;
}

void input_shutdown(struct i2c_client* i2c_client)
{
	// Run subsystem shutdowns
	input_fw_shutdown(i2c_client, g_ctx);

	// Remove context from global state
	// (It is freed by the device-specific memory mananger)
	g_ctx = NULL;
}
