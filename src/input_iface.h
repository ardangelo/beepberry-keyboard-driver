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

	uint8_t _ : 4;
	enum rp2040_key_state state : 4;
};

// It's just hard to keep typing bbqX0kbd_data...
struct kbd_ctx
{
	struct work_struct work_struct;
	uint8_t version_number;
	uint8_t touchpad_always_keys; // Set by parameter `touchpad_setting`
	uint8_t handle_poweroff; // Set by parameter `handle_poweroff`

	// Map from input HID scancodes to Linux keycodes
	uint8_t *keycode_map;

	// Key state FIFO queue
	uint8_t fifo_count;
	struct fifo_item fifo_data[BBQX0KBD_FIFO_SIZE];

	// Bitfield for active sticky modifiers
	uint8_t active_modifiers;
	uint8_t held_modifier_keys;
	uint8_t pending_sticky_modifier_keys;
	uint8_t sticky_modifier_keys;
	uint8_t locked_modifier_keys;

	uint8_t apply_phys_alt; // "Real" modifiers like
	// Shift and Control are handled by simulating input key
	// events. Since phys. alt is hardcoded, the state is here.
	uint8_t current_phys_alt_keycode; // Store the last keycode
	// sent in the phys. alt map to simulate a key up event
	// when the key is released after phys. alt is released

	// Touch and mouse flags
	uint8_t touch_event_flag;
	int8_t touch_rel_x;
	int8_t touch_rel_y;

	// Meta mode enabled flag
	uint8_t meta_mode;
	uint8_t meta_touch_keys_mode;

	// Keyboard brightness
	uint8_t brightness;
	uint8_t last_brightness;

	// Display flags
	uint8_t mono_invert;

	struct i2c_client *i2c_client;
	struct input_dev *input_dev;
};

struct sticky_modifier
{
	// Internal tracking bitfield to determine sticky state
	// All sticky modifiers need to have a unique bitfield
	uint8_t bit;

	// Keycode to send to the input system when applied
	uint8_t keycode;

	// Display indicator index and code
	uint8_t indicator_idx;
	char indicator_char;

	// When sticky modifier system has determined that
	// modifier should be applied, run this callback
	// and report the returned keycode result to the input system
	void (*set_callback)(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier);
	void (*unset_callback)(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier);
	uint8_t(*map_callback)(struct kbd_ctx* ctx, uint8_t keycode);
};

// Shared global state for global interfaces such as sysfs
extern struct kbd_ctx *g_ctx;

// Public interface

int input_probe(struct i2c_client* i2c_client);
void input_shutdown(struct i2c_client* i2c_client);

irqreturn_t input_irq_handler(int irq, void *param);
void input_workqueue_handler(struct work_struct *work_struct_ptr);

int input_get_rtc(uint8_t* year, uint8_t* mon, uint8_t* day,
	uint8_t* hour, uint8_t* min, uint8_t* sec);
int input_set_rtc(uint8_t year, uint8_t mon, uint8_t day,
	uint8_t hour, uint8_t min, uint8_t sec);

#endif
