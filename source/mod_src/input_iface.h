#ifndef INPUT_IFACE_H_
#define INPUT_IFACE_H_

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 */

#include <linux/types.h>
#include <linux/interrupt.h>

#include "registers.h"

#define BBQX0KBD_BUS_TYPE		BUS_I2C
#define BBQX0KBD_VENDOR_ID		0x0001
#define BBQX0KBD_PRODUCT_ID		0x0001
#define BBQX0KBD_VERSION_ID		0x0001

// From keyboard firmware source
enum rp2040_key_state
{
	KEY_STATE_IDLE = 0,
	KEY_STATE_PRESSED = 1,
	KEY_STATE_HOLD = 2,
	KEY_STATE_RELEASED = 3,
	KEY_STATE_LONG_HOLD = 4,
};
struct fifo_item
{
	uint8_t scancode;

	uint8_t _ : 1;
	uint8_t shift_modifier : 1;
	uint8_t ctrl_modifier : 1;
	uint8_t altgr_modifier : 1;
	enum rp2040_key_state state : 4;
};

// It's just hard to keep typing bbqX0kbd_data...
struct kbd_ctx
{
	struct work_struct work_struct;
	uint8_t version_number;
	uint8_t touchpad_always_keys; // Set by parameter `touchpad_setting`

	// Map from input HID scancodes to Linux keycodes
	uint8_t *keycode_map;

	// Key state FIFO queue
	uint8_t fifo_count;
	struct fifo_item fifo_data[BBQX0KBD_FIFO_SIZE];

	// Touch and mouse flags
	uint8_t touch_event_flag;
	int8_t touch_rel_x;
	int8_t touch_rel_y;

	// Meta mode enabled flag
	uint8_t meta_mode;
	uint8_t meta_touch_keys_mode;

	// Apply modifiers to the next alpha keypress
	uint8_t apply_control;
	uint8_t apply_alt;

	// Keyboard brightness
	uint8_t brightness;
	uint8_t last_brightness;

	struct i2c_client *i2c_client;
	struct input_dev *input_dev;
};

// Shared global state for global interfaces such as sysfs
extern struct kbd_ctx *g_ctx;

// Public interface

int input_probe(struct i2c_client* i2c_client);
void input_shutdown(struct i2c_client* i2c_client);

irqreturn_t input_irq_handler(int irq, void *param);
void input_workqueue_handler(struct work_struct *work_struct_ptr);

#endif
