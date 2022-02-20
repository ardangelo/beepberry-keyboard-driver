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

#include "bbqX0kbd_i2cHelper.h"
#include "bbq10kbd_registers.h"
#include "bbq10kbd_codes.h"
#include "debug_levels.h"


#ifdef BBQ10KBD_REGISTERS_H_
#define BBQX0KBD_I2C_ADDRESS	BBQ10_I2C_ADDRESS
#define BBQX0KBD_FIFO_SIZE		BBQ10_FIFO_SIZE
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

struct bbqX0kbd_data {
	struct work_struct work_struct;
	uint8_t version_number;
	uint8_t modifier_keys_status;
	uint8_t lockStatus;
	uint8_t keyboardBrightness;
	uint8_t lastKeyboardBrightness;
	uint8_t screenBrightness;
	uint8_t lastScreenBrightness;
	uint8_t fifoCount;
	uint8_t fifoData[BBQX0KBD_FIFO_SIZE][2];
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
