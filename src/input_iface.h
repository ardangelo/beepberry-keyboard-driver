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
struct key_fifo_item
{
	uint8_t scancode;

	uint8_t _ : 4;
	enum rp2040_key_state state : 4;
};

struct touch_ctx
{
	enum {
		TOUCH_ACT_ALWAYS = 0,
		TOUCH_ACT_CLICK = 1
	} activation;

	enum {
		TOUCH_INPUT_AS_KEYS = 0,
		TOUCH_INPUT_AS_MOUSE = 1
	} input_as;

	uint8_t enabled;
	uint8_t enable_while_shift_held;
	uint8_t entry_while_shift_held;
	uint8_t threshold;
	int x, dx, y, dy;
};

struct kbd_ctx
{
	struct work_struct work_struct;
	uint8_t version_number;

	struct i2c_client *i2c_client;
	struct input_dev *input_dev;

	// Map from input HID scancodes to Linux keycodes
	uint8_t *keycode_map;

	// Key state and touch FIFO queue
	uint8_t key_fifo_count;
	struct key_fifo_item key_fifo_data[BBQX0KBD_FIFO_SIZE];
	uint64_t last_keypress_at;

	uint8_t raised_touch_event;
	struct touch_ctx touch;
};

// Shared global state for global interfaces such as sysfs
extern struct kbd_ctx *g_ctx;

// Public interface

int input_probe(struct i2c_client* i2c_client);
void input_shutdown(struct i2c_client* i2c_client);

// Internal interfaces

// Firmware

int input_fw_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx);
void input_fw_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx);

int input_fw_consumes_keycode(struct kbd_ctx* ctx,
	uint8_t *remapped_keycode, uint8_t keycode, uint8_t state);

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

void input_fw_set_handle_poweroff(struct kbd_ctx* ctx, uint8_t handle_poweroff);
void input_fw_set_auto_off(struct kbd_ctx* ctx, uint8_t auto_off);

// RTC

int input_rtc_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx);
void input_rtc_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx);

// Display

int input_display_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx);
void input_display_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx);

int input_display_valid_sharp_path(char const* path);

void input_display_invert(struct kbd_ctx* ctx);

void input_display_set_indicator(int idx, unsigned char const* pixels);
void input_display_clear_indicator(int idx);
void input_display_clear_overlays(void);

// Modifiers

int input_modifiers_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx);
void input_modifiers_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx);

int input_modifiers_consumes_keycode(struct kbd_ctx* ctx,
	uint8_t *remapped_keycode, uint8_t keycode, uint8_t state);

uint8_t input_modifiers_apply_pending(struct kbd_ctx* ctx, uint8_t keycode);
void input_modifiers_reset(struct kbd_ctx* ctx);

void input_modifiers_send_control(struct kbd_ctx* ctx);
void input_modifiers_send_alt(struct kbd_ctx* ctx);

void input_modifiers_reset_shift(struct kbd_ctx* ctx);

// Touch

int input_touch_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx);
void input_touch_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx);

void input_touch_report_event(struct kbd_ctx *ctx);

int input_touch_consumes_keycode(struct kbd_ctx* ctx,
	uint8_t *remapped_keycode, uint8_t keycode, uint8_t state);

void input_touch_enable(struct kbd_ctx *ctx);
void input_touch_disable(struct kbd_ctx *ctx);

void input_touch_set_activation(struct kbd_ctx *ctx, uint8_t activation);
void input_touch_set_input_as(struct kbd_ctx *ctx, uint8_t input_as);

void input_touch_set_threshold(struct kbd_ctx *ctx, uint8_t threshold);
void input_touch_set_indicator(struct kbd_ctx *ctx);

// Meta mode

int input_meta_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx);
void input_meta_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx);

int input_meta_consumes_keycode(struct kbd_ctx* ctx,
	uint8_t *remapped_keycode, uint8_t keycode, uint8_t state);

void input_meta_enable(struct kbd_ctx* ctx);
void input_meta_disable(struct kbd_ctx* ctx);

#endif
