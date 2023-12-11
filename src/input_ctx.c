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

struct kbd_touch_cfg
{
	enum {
		TOUCH_ACT_ALWAYS = 0,
		TOUCH_ACT_META = 1
	} activation;

	enum {
		TOUCH_INPUT_AS_KEYS = 0,
		TOUCH_INPUT_AS_MOUSE = 1
	} input_as;
};

struct kbd_ctx
{
	struct work_struct work_struct;
	uint8_t version_number;

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
	struct kbd_touch_cfg touch_cfg;
	uint8_t touch_active;
	uint8_t touch_event_flag;
	int8_t touch_rel_x;
	int8_t touch_rel_y;

	// Meta mode settings
	uint8_t meta_enabled;

	// Firmware settings
	uint8_t fw_brightness;
	uint8_t fw_last_brightness;
	uint8_t fw_handle_poweroff;

	// Display settings
	uint8_t display_mono_invert;

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

// Internal interface

void input_send_control(struct kbd_ctx* ctx);
void input_send_alt(struct kbd_ctx* ctx);

// Display

void input_display_invert(struct kbd_ctx* ctx);

void input_display_set_indicator(uint8_t idx, char c);
void input_display_clear_indicator(uint8_t idx);

int input_display_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx);
void input_display_shutdown(struct i2c_client* i2c_client);

// Firmware

void input_fw_run_poweroff(struct kbd_ctx* ctx);

void input_fw_decrease_brightness(struct kbd_ctx* ctx);
void input_fw_increase_brightness(struct kbd_ctx* ctx);
void input_fw_toggle_brightness(struct kbd_ctx* ctx);

int input_fw_enable_touch_interrupts(struct kbd_ctx* ctx);
int input_fw_disable_touch_interrupts(struct kbd_ctx* ctx);

void input_fw_read_fifo(struct kbd_ctx* ctx);

int input_fw_get_rtc(uint8_t* year, uint8_t* mon, uint8_t* day,
	uint8_t* hour, uint8_t* min, uint8_t* sec);
int input_fw_set_rtc(uint8_t year, uint8_t mon, uint8_t day,
	uint8_t hour, uint8_t min, uint8_t sec);

int input_fw_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx);
void input_fw_shutdown(struct i2c_client* i2c_client);

// Meta mode

void input_meta_enable(struct kbd_ctx* ctx);
void input_meta_disable(struct kbd_ctx* ctx);

void input_meta_enable_touch_keys_mode(struct kbd_ctx* ctx);

bool input_meta_is_single_function_key(struct kbd_ctx* ctx, uint8_t keycode);
void input_meta_run_single_function_key(struct kbd_ctx* ctx, uint8_t keycode);
uint8_t input_meta_map_repeatable_key(struct kbd_ctx* ctx, uint8_t keycode);

int input_meta_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx);

// RTC

int input_rtc_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx);

#endif
