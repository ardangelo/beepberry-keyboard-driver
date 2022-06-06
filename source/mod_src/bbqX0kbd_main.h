/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * bbqX0kbd_main.h: Main H File.
 */
#ifndef BBQX0KBD_MAIN_H_
#define BBQX0KBD_MAIN_H_

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>

#include "config.h"
#include "bbqX0kbd_i2cHelper.h"
#include "bbqX0kbd_registers.h"
#include "debug_levels.h"
#if (BBQX0KBD_TYPE == BBQ10KBD_PMOD)
#include "bbq10kbd_pmod_codes.h"
#endif
#if (BBQX0KBD_TYPE == BBQ10KBD_FEATHERWING)
#include "bbq10kbd_featherwing_codes.h"
#endif
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
#include "bbq20kbd_pmod_codes.h"
#endif

#define COMPATIBLE_NAME			"wallComputer,bbqX0kbd"
#define DEVICE_NAME				"bbqX0kbd"
#define BBQX0KBD_BUS_TYPE		BUS_I2C
#define BBQX0KBD_VENDOR_ID		0x0001
#define BBQX0KBD_PRODUCT_ID		0x0001
#define BBQX0KBD_VERSION_ID		0x0001


#define LEFT_CTRL_BIT			BIT(0)
#define LEFT_SHIFT_BIT			BIT(1)
#define LEFT_ALT_BIT			BIT(2)
#define LEFT_GUI_BIT			BIT(3)
#define RIGHT_CTRL_BIT			BIT(4)
#define RIGHT_SHIFT_BIT			BIT(5)
#define RIGHT_ALT_BIT			BIT(6)
#define RIGHT_GUI_BIT			BIT(7)

#define CAPS_LOCK_BIT			BIT(0)
#define NUMS_LOCK_BIT			BIT(1)

#if (BBQX0KBD_INT == BBQX0KBD_NO_INT)
#define BBQX0KBD_DEFAULT_WORK_RATE	40
#define BBQX0KBD_MINIMUM_WORK_RATE	10
#define BBQX0KBD_MAXIMUM_WORK_RATE	1000
#endif

struct bbqX0kbd_data {
#if (BBQX0KBD_INT == BBQX0KBD_USE_INT)
	struct work_struct work_struct;
	uint8_t fifoCount;
	uint8_t fifoData[BBQX0KBD_FIFO_SIZE][2];
#else
	struct delayed_work delayed_work;
	struct workqueue_struct *workqueue_struct;
	uint8_t work_rate_ms;
#endif

#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
	uint8_t touchInt;
	int8_t rel_x;
	int8_t rel_y;
#endif
	uint8_t version_number;
	uint8_t modifier_keys_status;
	uint8_t lockStatus;
	uint8_t keyboardBrightness;
	uint8_t lastKeyboardBrightness;
#if (BBQX0KBD_TYPE == BBQ10KBD_FEATHERWING)
	uint8_t screenBrightness;
	uint8_t lastScreenBrightness;
#endif
	unsigned short keycode[NUM_KEYCODES];
	struct i2c_client *i2c_client;
	struct input_dev *input_dev;
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wallComputer");
MODULE_DESCRIPTION("Keyboard driver for BBQ10, hardware by arturo182 <arturo182@tlen.pl>, software by wallComputer.");
MODULE_VERSION("0.4") ; // To Match software version on SAM-D Controller of arturo182's keyboard featherwing Rev 2.

static const struct i2c_device_id bbqX0kbd_i2c_device_id[] = {
	{ DEVICE_NAME, 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bbqX0kbd_i2c_device_id);

static const struct of_device_id bbqX0kbd_of_device_id[] = {
	{ .compatible = COMPATIBLE_NAME, },
	{ }
};
MODULE_DEVICE_TABLE(of, bbqX0kbd_of_device_id);

#endif
