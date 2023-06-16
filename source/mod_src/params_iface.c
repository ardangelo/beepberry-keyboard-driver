// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * params_iface.c: Module parameters
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "config.h"

#include "i2c_helper.h"
#include "input_iface.h"
#include "params_iface.h"

// Kernel module parameters
static char* touchpad_setting = "meta"; // Use meta mode by default

// Update touchpad setting in global context, if available
static int set_touchpad_setting(struct kbd_ctx *ctx, char const* val)
{
	// If no state was passed, just update the local setting without
	// changing I2C touch interrupts
	if (!ctx) {
		return 0;
	}

	if (strncmp(val, "keys", 4) == 0) {

		// Update global setting and enable touchpad interrupts
		ctx->touchpad_always_keys = 1;
		(void)kbd_write_i2c_u8(ctx->i2c_client, REG_CF2, REG_CF2_TOUCH_INT);

		return 0;

	} else if (strncmp(val, "meta", 4) == 0) {

		// Update global setting and disable touchpad interrupts
		// if meta mode does not have touch keys enabled
		ctx->touchpad_always_keys = 0;
		(void)kbd_write_i2c_u8(ctx->i2c_client, REG_CF2,
			(ctx->meta_touch_keys_mode) ? REG_CF2_TOUCH_INT : 0);

		return 0;
	}

	// Invalid parameter value
	return -1;
}

// Use touchpad for meta mode, or use it as arrow keys
static int touchpad_setting_param_set(const char *val, const struct kernel_param *kp)
{
	char *stripped_val;
	char stripped_val_buf[5];

	// Copy provided value to buffer and strip it of newlines
	strncpy(stripped_val_buf, val, 5);
	stripped_val_buf[4] = '\0';
	stripped_val = strstrip(stripped_val_buf);

	return (set_touchpad_setting(g_ctx, stripped_val) < 0)
		? -EINVAL
		: param_set_charp(stripped_val, kp);
}

static const struct kernel_param_ops touchpad_setting_param_ops = {
	.set = touchpad_setting_param_set,
	.get = param_get_charp,
};

module_param_cb(touchpad, &touchpad_setting_param_ops, &touchpad_setting, 0664);
MODULE_PARM_DESC(touchpad_setting, "Touchpad as arrow keys (\"keys\") or click for meta mode (\"meta\")");

// No setup
int params_probe(void)
{
	int rc;

	// Set initial touchpad setting based on module's parameter
	if ((rc = set_touchpad_setting(g_ctx, touchpad_setting)) < 0) {
		return rc;
	}

	return 0;
}

// No cleanup
void params_shutdown(void)
{
	return;
}
