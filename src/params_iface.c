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
static char *touch_act_setting = "click"; // "click" or "always"
static char *touch_shift_setting = "1"; // Hold Shift to temporarily enable touch
static char *touch_as_setting = "keys"; // "keys" or "mouse"
static char *touch_min_squal_setting = "16"; // Minimum surface quality to accept touch event
static char *touch_led_setting = "high"; // "low", "med", "high"
static uint32_t touch_threshold_setting = 8; // Touchpad move offset
static char *handle_poweroff_setting = "0"; // Enable to have module invoke poweroff
static char *shutdown_grace_setting = "30"; // 30 seconds between shutdown signal and poweroff
static char *sharp_path_setting = "/dev/dri/card0"; // Path to Sharp display device
static uint32_t sysfs_gid_setting = 0; // GID of files in /sys/firmware/beepy
static char *auto_off_setting = // Enable to trigger a 30 second poweroff timer on driver unload
#ifdef DEBUG
"0";
#else
"1";
#endif

// Update touchpad activation setting in global context, if available
static int set_touch_act_setting(struct kbd_ctx* ctx, char const* val)
{
	// If no state was passed, just update the local setting without
	// changing I2C touch interrupts
	if (!ctx) {
		return 0;
	}

	// Touchpad only active when clicked
	if (strcmp(val, "click") == 0) {
		input_touch_set_activation(ctx, TOUCH_ACT_CLICK);
		return 0;

	// Touchpad always active
	} else if (strcmp(val, "always") == 0) {
		input_touch_set_activation(ctx, TOUCH_ACT_ALWAYS);
		return 0;
	}

	// Invalid parameter value
	return -1;
}

// Copy provided value to buffer and strip it of newlines
static char* copy_and_strip(char* buf, size_t buf_len, const char* val)
{
	strncpy(buf, val, buf_len - 1);
	buf[buf_len - 1] = '\0';
	return strstrip(buf);
}

// Activate touch when clicked, or always enabled
static int touch_act_setting_param_set(const char *val, const struct kernel_param* kp)
{
	char buf[8];
	char *stripped_val;

	stripped_val = copy_and_strip(buf, sizeof(buf), val);

	return (set_touch_act_setting(g_ctx, stripped_val) < 0)
		? -EINVAL
		: param_set_charp(stripped_val, kp);
}

static const struct kernel_param_ops touch_act_setting_param_ops = {
	.set = touch_act_setting_param_set,
	.get = param_get_charp,
};
module_param_cb(touch_act, &touch_act_setting_param_ops, &touch_act_setting, 0664);
MODULE_PARM_DESC(touch_act_setting, "Touchpad enabled after clicking (\"click\") or always enabled (\"always\")");

// Update touch shift enable in global context
static int set_touch_shift_setting(struct kbd_ctx *ctx, char const* val)
{
	// If no state was passed, exit
	if (!ctx) {
		return 0;
	}

	ctx->touch.enable_while_shift_held = val[0] != '0';
	return 0;
}

// Hold shift to temporarily enable touch
static int touch_shift_setting_param_set(const char *val, const struct kernel_param *kp)
{
	char *stripped_val;
	char stripped_val_buf[2];

	// Copy provided value to buffer and strip it of newlines
	strncpy(stripped_val_buf, val, 2);
	stripped_val_buf[1] = '\0';
	stripped_val = strstrip(stripped_val_buf);

	return (set_touch_shift_setting(g_ctx, stripped_val) < 0)
		? -EINVAL
		: param_set_charp(stripped_val, kp);
}

static const struct kernel_param_ops touch_shift_setting_param_ops = {
	.set = touch_shift_setting_param_set,
	.get = param_get_charp,
};

module_param_cb(touch_shift, &touch_shift_setting_param_ops, &touch_shift_setting, 0664);
MODULE_PARM_DESC(touch_shift_setting, "Set to 1 to enable touch while Shift key is held");

// Update touchpad mode setting in global context, if available
static int set_touch_as_setting(struct kbd_ctx* ctx, char const* val)
{
	// If no state was passed, just update the local setting without
	// changing I2C touch interrupts
	if (!ctx) {
		return 0;
	}

	// Touchpad sends arrow keys
	if (strcmp(val, "keys") == 0) {
		input_touch_set_input_as(ctx, TOUCH_INPUT_AS_KEYS);
		return 0;

	// Touchpad sends mouse
	} else if (strcmp(val, "mouse") == 0) {
		input_touch_set_input_as(ctx, TOUCH_INPUT_AS_MOUSE);
		return 0;
	}

	// Invalid parameter value
	return -1;
}

// Touchpad sends arrow keys or mouse
static int touch_as_setting_param_set(const char *val, const struct kernel_param* kp)
{
	char buf[8];
	char *stripped_val;

	stripped_val = copy_and_strip(buf, sizeof(buf), val);

	return (set_touch_as_setting(g_ctx, stripped_val) < 0)
		? -EINVAL
		: param_set_charp(stripped_val, kp);
}

static const struct kernel_param_ops touch_as_setting_param_ops = {
	.set = touch_as_setting_param_set,
	.get = param_get_charp,
};
module_param_cb(touch_as, &touch_as_setting_param_ops, &touch_as_setting, 0664);
MODULE_PARM_DESC(touch_as_setting, "Touchpad sends arrow keys (\"keys\") or mouse (\"mouse\")");

// Set touchpad minimum surface quality level
static int set_touch_min_squal_setting(struct kbd_ctx *ctx, char const* val)
{
	int parsed_val;

	// If no state was passed, exit
	if (!ctx) {
		return 0;
	}

	// Parse setting
	if ((parsed_val = parse_u8(val)) < 0) {
		return parsed_val;
	}

	// Write setting to firmware
	(void)kbd_write_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_MIN_SQUAL, (uint8_t)parsed_val);

	return 0;
}

// Touchpad minimum surface quality level
static int touch_min_squal_param_set(const char *val, const struct kernel_param *kp)
{
	char *stripped_val;
	char stripped_val_buf[4];

	// Copy provided value to buffer and strip it of newlines
	strncpy(stripped_val_buf, val, 4);
	stripped_val_buf[3] = '\0';
	stripped_val = strstrip(stripped_val_buf);

	return (set_touch_min_squal_setting(g_ctx, stripped_val) < 0)
		? -EINVAL
		: param_set_charp(stripped_val, kp);
}

static const struct kernel_param_ops touch_min_squal_param_ops = {
	.set = touch_min_squal_param_set,
	.get = param_get_charp,
};

module_param_cb(touch_min_squal, &touch_min_squal_param_ops, &touch_min_squal_setting, 0664);
MODULE_PARM_DESC(touch_min_squal_setting, "Minimum surface quality to accept touch event");

// Set touchpad move threshold
static int set_touch_threshold_setting(struct kbd_ctx *ctx, char const* val)
{
	int parsed_val;

	// If no state was passed, exit
	if (!ctx) {
		return 0;
	}

	// Parse setting
	if ((parsed_val = parse_u8(val)) < 0) {
		return parsed_val;
	}

	// Check setting
	if ((parsed_val < 4) || (parsed_val > 255)) {
		return -1;
	}

	// Store setting
	input_touch_set_threshold(ctx, (uint8_t)parsed_val);

	return 0;
}

// Touchpad move threshold
static int touch_threshold_param_set(const char *val, const struct kernel_param *kp)
{
	char *stripped_val;
	char stripped_val_buf[4];

	// Copy provided value to buffer and strip it of newlines
	strncpy(stripped_val_buf, val, 4);
	stripped_val_buf[3] = '\0';
	stripped_val = strstrip(stripped_val_buf);

	return (set_touch_threshold_setting(g_ctx, stripped_val) < 0)
		? -EINVAL
		: param_set_uint(stripped_val, kp);
}

static const struct kernel_param_ops touch_threshold_param_ops = {
	.set = touch_threshold_param_set,
	.get = param_get_uint,
};

module_param_cb(touch_threshold, &touch_threshold_param_ops, &touch_threshold_setting, 0664);
MODULE_PARM_DESC(touch_threshold_setting, "Send touch event above this threshold (minimum 4, default 8)");

// Set touchpad LED power level
static int set_touch_led_setting(struct kbd_ctx* ctx, char const* val)
{
	if (strcmp(val, "low") == 0) {
		(void)kbd_write_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_LED, TOUCHPAD_LED_LOW);
		return 0;

	} else if (strcmp(val, "med") == 0) {
		(void)kbd_write_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_LED, TOUCHPAD_LED_MED);
		return 0;

	} else if (strcmp(val, "high") == 0) {
		(void)kbd_write_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_LED, TOUCHPAD_LED_HIGH);
		return 0;
	}

	// Invalid parameter value
	return -1;
}

// Touchpad LED level
static int touch_led_setting_param_set(const char *val, const struct kernel_param* kp)
{
	char buf[8];
	char *stripped_val;

	stripped_val = copy_and_strip(buf, sizeof(buf), val);

	return (set_touch_led_setting(g_ctx, stripped_val) < 0)
		? -EINVAL
		: param_set_charp(stripped_val, kp);
}

static const struct kernel_param_ops touch_led_setting_param_ops = {
	.set = touch_led_setting_param_set,
	.get = param_get_charp,
};
module_param_cb(touch_led, &touch_led_setting_param_ops, &touch_led_setting, 0664);
MODULE_PARM_DESC(touch_led_setting, "Touchpad LED power level (\"low\", \"med\", \"high\")");

// Update poweroff setting in global context, if available
static int set_handle_poweroff_setting(struct kbd_ctx *ctx, char const* val)
{
	// If no state was passed, exit
	if (!ctx) {
		return 0;
	}

	input_fw_set_handle_poweroff(ctx, val[0] != '0');
	return 0;
}

// Whether or not to handle poweroff in driver (for minimal system without ACPI)
static int handle_poweroff_setting_param_set(const char *val, const struct kernel_param *kp)
{
	char *stripped_val;
	char stripped_val_buf[2];

	// Copy provided value to buffer and strip it of newlines
	strncpy(stripped_val_buf, val, 2);
	stripped_val_buf[1] = '\0';
	stripped_val = strstrip(stripped_val_buf);

	return (set_handle_poweroff_setting(g_ctx, stripped_val) < 0)
		? -EINVAL
		: param_set_charp(stripped_val, kp);
}

static const struct kernel_param_ops handle_poweroff_setting_param_ops = {
	.set = handle_poweroff_setting_param_set,
	.get = param_get_charp,
};

module_param_cb(handle_poweroff, &handle_poweroff_setting_param_ops, &handle_poweroff_setting, 0664);
MODULE_PARM_DESC(handle_poweroff_setting, "Set to 1 to invoke /sbin/poweroff when power key is held");

// Update shutdown grace time in firmware
static int set_shutdown_grace_setting(struct kbd_ctx *ctx, char const* val)
{
	int parsed_val;

	// If no state was passed, exit
	if (!ctx) {
		return 0;
	}

	// Parse setting
	if ((parsed_val = parse_u8(val)) < 0) {
		return parsed_val;
	}

	// Check setting (minimum 5 seconds)
	if ((parsed_val < 5) || (parsed_val > 255)) {
		return -EINVAL;
	}

	// Write setting to firmware
	(void)kbd_write_i2c_u8(ctx->i2c_client, REG_SHUTDOWN_GRACE, (uint8_t)parsed_val);

	return 0;
}

// Time between shutdown signal and power off in seconds
static int shutdown_grace_param_set(const char *val, const struct kernel_param *kp)
{
	char *stripped_val;
	char stripped_val_buf[4];

	// Copy provided value to buffer and strip it of newlines
	strncpy(stripped_val_buf, val, 4);
	stripped_val_buf[3] = '\0';
	stripped_val = strstrip(stripped_val_buf);

	return (set_shutdown_grace_setting(g_ctx, stripped_val) < 0)
		? -EINVAL
		: param_set_charp(stripped_val, kp);
}

static const struct kernel_param_ops shutdown_grace_param_ops = {
	.set = shutdown_grace_param_set,
	.get = param_get_charp,
};

module_param_cb(shutdown_grace, &shutdown_grace_param_ops, &shutdown_grace_setting, 0664);
MODULE_PARM_DESC(shutdown_grace_setting, "Set delay in seconds from shutdown signal to poweroff");

// Path to Sharp DRM device
static int sharp_path_param_set(const char *val, const struct kernel_param *kp)
{
	char *stripped_val;
	char stripped_val_buf[64];

	// Copy provided value to buffer and strip it of newlines
	strncpy(stripped_val_buf, val, sizeof(stripped_val_buf));
	stripped_val_buf[sizeof(stripped_val_buf) - 1] = '\0';
	stripped_val = strstrip(stripped_val_buf);

	return (input_display_valid_sharp_path(stripped_val))
		? param_set_charp(stripped_val, kp)
		: -EINVAL;
}

static const struct kernel_param_ops sharp_path_param_ops = {
	.set = param_set_charp,
	.get = param_get_charp,
};

module_param_cb(sharp_path, &sharp_path_param_ops, &sharp_path_setting, 0664);
MODULE_PARM_DESC(sharp_path_setting, "Set path to Sharp display DRM device");

// Time between shutdown signal and power off in seconds
static int sysfs_gid_param_set(const char *val, const struct kernel_param *kp)
{
	char *stripped_val;
	char stripped_val_buf[11];

	// Copy provided value to buffer and strip it of newlines
	strncpy(stripped_val_buf, val, 11);
	stripped_val_buf[10] = '\0';
	stripped_val = strstrip(stripped_val_buf);

	return param_set_uint(stripped_val, kp);
}

static const struct kernel_param_ops sysfs_gid_param_ops = {
	.set = sysfs_gid_param_set,
	.get = param_get_uint,
};

module_param_cb(sysfs_gid, &sysfs_gid_param_ops, &sysfs_gid_setting, 0664);
MODULE_PARM_DESC(sysfs_gid_setting, "Set group ID for entries in /sys/firmware/beepy");

// Trigger shutdown on driver unload
static int set_auto_off_setting(struct kbd_ctx *ctx, char const* val)
{
	// If no state was passed, exit
	if (!ctx) {
		return 0;
	}

	input_fw_set_auto_off(ctx, val[0] != '0');
	return 0;
}

static int auto_off_setting_param_set(const char *val, const struct kernel_param *kp)
{
	char *stripped_val;
	char stripped_val_buf[2];

	// Copy provided value to buffer and strip it of newlines
	strncpy(stripped_val_buf, val, 2);
	stripped_val_buf[1] = '\0';
	stripped_val = strstrip(stripped_val_buf);

	return (set_auto_off_setting(g_ctx, stripped_val) < 0)
		? -EINVAL
		: param_set_charp(stripped_val, kp);
}

static const struct kernel_param_ops auto_off_setting_param_ops = {
	.set = auto_off_setting_param_set,
	.get = param_get_charp,
};

module_param_cb(auto_off, &auto_off_setting_param_ops, &auto_off_setting, 0664);
MODULE_PARM_DESC(auto_off_setting, "Automatically shut down and enter deep sleep when driver is unloaded");

// No setup
int params_probe(void)
{
	int rc;

	// Set initial touchpad settings based on module's parameter
	if ((rc = set_touch_act_setting(g_ctx, touch_act_setting)) < 0) {
		return rc;
	}
	if ((rc = set_touch_as_setting(g_ctx, touch_as_setting)) < 0) {
		return rc;
	}
	if ((rc = set_touch_min_squal_setting(g_ctx, touch_min_squal_setting)) < 0) {
		return rc;
	}
	if ((rc = set_handle_poweroff_setting(g_ctx, handle_poweroff_setting)) < 0) {
		return rc;
	}
	if ((rc = set_auto_off_setting(g_ctx, auto_off_setting)) < 0) {
		return rc;
	}

	return 0;
}

// No cleanup
void params_shutdown(void)
{
	return;
}

char const* params_get_sharp_path(void)
{
	return sharp_path_setting;
}

uint32_t params_get_sysfs_gid(void)
{
	return sysfs_gid_setting;
}
