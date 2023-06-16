// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * sysfs.c: /sys/firmware/beepberry interface
 */

#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

#include "config.h"

#include "i2c_helper.h"
#include "input_iface.h"
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

// Parse 0 to 255 from string
static int parse_u8(char const* buf)
{
	int rc, result;

	// Parse string value
	if ((rc = kstrtoint(buf, 10, &result)) || (result < 0) || (result > 0xff)) {
		return -EINVAL;
	}
	return result;
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
	return sprintf(buf, "%d", total_level);
}
struct kobj_attribute battery_raw_attr
	= __ATTR(battery_raw, 0440, battery_raw_show, NULL);

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
	volts_fp *= 330 * 2;
	volts_fp /= 4095;

	// Format into buffer
	return sprintf(buf, "%d.%d", volts_fp / 100, volts_fp % 100);
}
struct kobj_attribute battery_volts_attr
	= __ATTR(battery_volts, 0440, battery_volts_show, NULL);

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

// Sysfs attributes (entries)
struct kobject *beepberry_kobj = NULL;
static struct attribute *beepberry_attrs[] = {
    &battery_raw_attr.attr,
	&battery_volts_attr.attr,
	&led_attr.attr,
	&led_red_attr.attr,
	&led_green_attr.attr,
	&led_blue_attr.attr,
	&keyboard_backlight_attr.attr,	
    NULL,
};
static struct attribute_group beepberry_attr_group = {
    .attrs = beepberry_attrs
};

int sysfs_probe(void)
{
	// Create sysfs entry for beepberry
	if ((beepberry_kobj = kobject_create_and_add("beepberry", firmware_kobj)) == NULL) {
    	return -ENOMEM;
	}

	// Create sysfs attributes
	if (sysfs_create_group(beepberry_kobj, &beepberry_attr_group)){
		kobject_put(beepberry_kobj);
		return -ENOMEM;
	}

	return 0;
}

void sysfs_shutdown(void)
{
	// Remove sysfs entry
	if (beepberry_kobj) {
		kobject_put(beepberry_kobj);
		beepberry_kobj = NULL;
	}
}
