// SPDX-License-Identifier: GPL-2.0-only
/*
 * Kernel driver for Q20 keyboard by ardangelo
 * References work by arturo182, wallComputer
 * sysfs.c: /sys/firmware/beepy interface
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/timekeeping.h>
#include <linux/math64.h>

#include "config.h"

#include "i2c_helper.h"
#include "input_iface.h"
#include "params_iface.h"
#include "sysfs_iface.h"

// Read battery level over I2C
static int read_raw_battery_level(void)
{
	int rc;
	uint8_t battery_level[2];

	// Make sure I2C client was initialized
	if ((g_ctx == NULL) || (g_ctx->i2c_client == NULL)) {
		return -EINVAL;
	}

	// Read battery level
	if ((rc = kbd_read_i2c_2u8(g_ctx->i2c_client, REG_ADC, battery_level)) < 0) {
		return rc;
	}

	// Calculate raw battery level
	return (battery_level[1] << 8) | battery_level[0];
}

static int parse_and_write_i2c_u8(char const* buf, size_t count, uint8_t reg)
{
	int parsed;

	// Parse string entry
	if ((parsed = parse_u8(buf)) < 0) {
		return -EINVAL;
	}

	// Write value to LED register if available
	if (g_ctx && g_ctx->i2c_client) {
		kbd_write_i2c_u8(g_ctx->i2c_client, reg, (uint8_t)parsed);
	}

	return count;
}

// Sysfs entries

// Raw battery level
static ssize_t battery_raw_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	int total_level;

	// Read raw level
	if ((total_level = read_raw_battery_level()) < 0) {
		return total_level;
	}

	// Format into buffer
	return sprintf(buf, "%d\n", total_level);
}
struct kobj_attribute battery_raw_attr
	= __ATTR(battery_raw, 0444, battery_raw_show, NULL);

// Battery volts level
static ssize_t battery_volts_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	int volts_fp;

	// Read raw level
	if ((volts_fp = read_raw_battery_level()) < 0) {
		return volts_fp;
	}

	// Calculate voltage in fixed point
	volts_fp *= 330 * 21;
	volts_fp /= 4095;

	// Format into buffer
	return sprintf(buf, "%d.%03d\n", volts_fp / 1000, volts_fp % 1000);
}
struct kobj_attribute battery_volts_attr
	= __ATTR(battery_volts, 0444, battery_volts_show, NULL);

// Battery percent approximate
static ssize_t battery_percent_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	int percent;

	// Read raw level
	if ((percent = read_raw_battery_level()) < 0) {
		return percent;
	}

	// Calculate voltage in fixed point
	percent *= 330 * 21;
	percent /= 4095;

	// Range from 3.2V min to 4.2V max
	// `percent` currently contains the fp voltage value (v * 1000; e.g. in the range from 3200 to 4200)
	// To convert to a percentage in the range from 0 - 1 we subtract the value at 0% (3200)
	// and divide by the difference between lowest value and higest value (4200 - 3200). Then
	// multiply by 100 to get 0 - 100. This can all be simplified to:
	percent -= 3200;
	percent /= 10;

	// If the voltage goes above 4.2v the percentage will go above 100. This can happen while plugged in.
	if (percent > 100) {
		percent = 100;
	// If the voltage drops below 3.2v the percentage will go below 0, so cap at 0.
	} else if (percent < 0) {
		percent = 0;
	}

	// Format into buffer
	return sprintf(buf, "%d\n", percent);
}
struct kobj_attribute battery_percent_attr
	= __ATTR(battery_percent, 0444, battery_percent_show, NULL);

// LED on or off
static ssize_t led_store(struct kobject *kobj, struct kobj_attribute *attr,
	char const *buf, size_t count)
{
	return parse_and_write_i2c_u8(buf, count, REG_LED);
}
struct kobj_attribute led_attr = __ATTR(led, 0220, NULL, led_store);

// LED red value
static ssize_t led_red_store(struct kobject *kobj, struct kobj_attribute *attr,
	char const *buf, size_t count)
{
	return parse_and_write_i2c_u8(buf, count, REG_LED_R);
}
struct kobj_attribute led_red_attr = __ATTR(led_red, 0220, NULL, led_red_store);

// LED green value
static ssize_t led_green_store(struct kobject *kobj, struct kobj_attribute *attr,
	char const *buf, size_t count)
{
	return parse_and_write_i2c_u8(buf, count, REG_LED_G);
}
struct kobj_attribute led_green_attr = __ATTR(led_green, 0220, NULL, led_green_store);

// LED blue value
static ssize_t __used led_blue_store(struct kobject *kobj, struct kobj_attribute *attr,
	char const *buf, size_t count)
{
	return parse_and_write_i2c_u8(buf, count, REG_LED_B);
}
struct kobj_attribute led_blue_attr = __ATTR(led_blue, 0220, NULL, led_blue_store);

// Keyboard backlight value
static ssize_t __used keyboard_backlight_store(struct kobject *kobj,
	struct kobj_attribute *attr, char const *buf, size_t count)
{
	return parse_and_write_i2c_u8(buf, count, REG_BKL);
}
struct kobj_attribute keyboard_backlight_attr
	= __ATTR(keyboard_backlight, 0220, NULL, keyboard_backlight_store);

// Shutdown and rewake timer in minutes
static ssize_t __used rewake_timer_store(struct kobject *kobj,
	struct kobj_attribute *attr, char const *buf, size_t count)
{
	return parse_and_write_i2c_u8(buf, count, REG_REWAKE_MINS);
}
struct kobj_attribute rewake_timer_attr
	= __ATTR(rewake_timer, 0220, NULL, rewake_timer_store);

// Firmware version
static ssize_t fw_version_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	int rc;
	uint8_t version;

	// Make sure I2C client was initialized
	if ((g_ctx == NULL) || (g_ctx->i2c_client == NULL)) {
		return -EINVAL;
	}

	// Read firmware version
	if ((rc = kbd_read_i2c_u8(g_ctx->i2c_client, REG_VER, &version)) < 0) {
		return rc;
	}

	return sprintf(buf, "%d.%d\n", version >> 4, version & 0xf);
}
struct kobj_attribute fw_version_attr
	= __ATTR(fw_version, 0444, fw_version_show, NULL);

// Why the Pi was powered on
static ssize_t startup_reason_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	int rc;
	uint8_t reason;

	// Make sure I2C client was initialized
	if ((g_ctx == NULL) || (g_ctx->i2c_client == NULL)) {
		return -EINVAL;
	}

	// Read startup reason
	if ((rc = kbd_read_i2c_u8(g_ctx->i2c_client, REG_STARTUP_REASON, &reason)) < 0) {
		return rc;
	}

	switch (reason) {

		case STARTUP_REASON_FW_INIT: return sprintf(buf, "fw_init\n");
		case STARTUP_REASON_BUTTON: return sprintf(buf, "power_button\n");
		case STARTUP_REASON_REWAKE: return sprintf(buf, "rewake\n");
		case STARTUP_REASON_REWAKE_CANCELED: return sprintf(buf, "rewake_canceled\n");
	}

	return sprintf(buf, "unknown: %d\n", reason);
}
struct kobj_attribute startup_reason_attr
	= __ATTR(startup_reason, 0444, startup_reason_show, NULL);

// Firmware update
static ssize_t __used fw_update_store(struct kobject *kobj,
	struct kobj_attribute *attr, char const *buf, size_t count)
{
	int rc;
	size_t i;
	uint8_t update_state;
	char const* update_error = NULL;

	// Write value to update
	if (g_ctx && g_ctx->i2c_client) {

		// Read update status
		if ((rc = kbd_read_i2c_u8(g_ctx->i2c_client, REG_UPDATE_DATA, &update_state)) < 0) {
			return rc;
		}

		// Start a new update
		if ((update_state == UPDATE_OFF) || (update_state >= UPDATE_FAILED)) {
			dev_info(&g_ctx->i2c_client->dev,
				"fw_update: starting new update, writing %zu bytes\n", count);

		// In-progress update
		} else if (update_state == UPDATE_RECV) {
			dev_info(&g_ctx->i2c_client->dev,
				"fw_update: writing %zu bytes\n", count);
		}

		for (i = 0; i < count; i++) {
			kbd_write_i2c_u8(g_ctx->i2c_client, REG_UPDATE_DATA, (uint8_t)buf[i]);
		}

		// Read update status
		if ((rc = kbd_read_i2c_u8(g_ctx->i2c_client, REG_UPDATE_DATA, &update_state)) < 0) {
			return rc;
		}

		// Successful update
		if (update_state == UPDATE_OFF) {
			dev_info(&g_ctx->i2c_client->dev,
				"fw_update: wrote %zu bytes, update completed\n", count);

		// Update still in-progress
		} else if (update_state == UPDATE_RECV) {
			dev_info(&g_ctx->i2c_client->dev,
				"fw_update: wrote %zu bytes\n", count);

		// Update failed
		} else if (update_state >= UPDATE_FAILED) {

			update_error = "update failed";

			switch (update_state) {
			case UPDATE_FAILED_LINE_OVERFLOW:
				update_error = "hex line too long"; break;
			case UPDATE_FAILED_FLASH_EMPTY:
				update_error = "flash image empty"; break;
			case UPDATE_FAILED_FLASH_OVERFLOW:
				update_error = "flash image > 64k"; break;
			case UPDATE_FAILED_BAD_LINE:
				update_error = "could not parse hex line"; break;
			case UPDATE_FAILED_BAD_CHECKSUM:
				update_error = "bad checksum"; break;
			}

			dev_info(&g_ctx->i2c_client->dev,
				"fw_update: failed: %s\n", update_error);
			return -EINVAL;
		}
	}

	return count;
}
struct kobj_attribute fw_update_attr
	= __ATTR(fw_update, 0220, NULL, fw_update_store);

// Time since last keypress in milliseconds
static ssize_t last_keypress_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	uint64_t last_keypress_ms;

	if (g_ctx) {

		// Get time in ns
		last_keypress_ms = ktime_get_boottime_ns();
		if (g_ctx->last_keypress_at < last_keypress_ms) {
			last_keypress_ms -= g_ctx->last_keypress_at;

			// Calculate time in milliseconds
			last_keypress_ms = div_u64(last_keypress_ms, 1000000);

			// Format into buffer
			return sprintf(buf, "%lld\n", last_keypress_ms);
		}
	}

	return sprintf(buf, "-1\n");
}
struct kobj_attribute last_keypress_attr
	= __ATTR(last_keypress, 0444, last_keypress_show, NULL);

// Sysfs attributes (entries)
struct kobject *beepy_kobj = NULL;
static struct attribute *beepy_attrs[] = {
	&battery_raw_attr.attr,
	&battery_volts_attr.attr,
	&battery_percent_attr.attr,
	&led_attr.attr,
	&led_red_attr.attr,
	&led_green_attr.attr,
	&led_blue_attr.attr,
	&keyboard_backlight_attr.attr,
	&rewake_timer_attr.attr,
	&startup_reason_attr.attr,
	&fw_version_attr.attr,
	&fw_update_attr.attr,
	&last_keypress_attr.attr,
	NULL,
};
static struct attribute_group beepy_attr_group = {
	.attrs = beepy_attrs
};

static void beepy_get_ownership
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
(struct kobject *kobj, kuid_t *uid, kgid_t *gid)
#else
(struct kobject const *kobj, kuid_t *uid, kgid_t *gid)
#endif
{
	if (gid != NULL) {
		gid->val = params_get_sysfs_gid();
	}
}

static struct kobj_type beepy_ktype = {
	.get_ownership = beepy_get_ownership,
	.sysfs_ops = &kobj_sysfs_ops
};

int sysfs_probe(struct i2c_client* i2c_client)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
	int rc;
#endif

	// Allocate custom sysfs type
	if ((beepy_kobj = devm_kzalloc(&i2c_client->dev, sizeof(*beepy_kobj), GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	}

	// Create sysfs entries for beepy with custom type
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
	rc =
#endif
	kobject_init_and_add(beepy_kobj, &beepy_ktype, firmware_kobj, "beepy");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
	if (rc < 0) {
		kobject_put(beepy_kobj);
		return rc;
	}
#endif

	// Create sysfs attributes
	if (sysfs_create_group(beepy_kobj, &beepy_attr_group)) {
		kobject_put(beepy_kobj);
		return -ENOMEM;
	}

	return 0;
}

void sysfs_shutdown(struct i2c_client* i2c_client)
{
	// Remove sysfs entry
	if (beepy_kobj) {
		kobject_put(beepy_kobj);
		beepy_kobj = NULL;
	}
}
