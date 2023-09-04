// SPDX-License-Identifier: GPL-2.0-only
// Input meta mode subsystem

#include <linux/input.h>

#include "config.h"
#include "input_iface.h"

// Meta mode helpers

void input_meta_enable(struct kbd_ctx* ctx)
{
	ctx->meta_enabled = 1;

	// Set display indicator
	input_display_set_indicator(5, 'm');
}

void input_meta_disable(struct kbd_ctx* ctx)
{
	ctx->meta_enabled = 0;

	// Clear display indicator
	input_display_clear_indicator(5);

	// Disable touch interrupts on I2C
	input_fw_disable_touch_interrupts(ctx);

	ctx->meta_touch_keys_mode = 0;
}

void input_meta_enable_touch_keys_mode(struct kbd_ctx* ctx)
{
	// Enable touch interrupts on I2C
	input_fw_enable_touch_interrupts(ctx);

	ctx->meta_touch_keys_mode = 1;
}

// Called before checking "repeatable" meta mode keys,
// These keys map to an internal driver function rather than another key
// They will not be sent to the input system
// The check is separate from the run so that key-up events can be ignored
bool input_meta_is_single_function_key(struct kbd_ctx* ctx, uint8_t keycode)
{
	switch (keycode) {
	case KEY_T: return TRUE; // Tab
	case KEY_X: return TRUE; // Control
	case KEY_C: return TRUE; // Alt
	case KEY_N: return TRUE; // Decrease brightness
	case KEY_M: return TRUE; // Increase brightness
	case KEY_MUTE: return TRUE; // Toggle brightness
	case KEY_0: return TRUE; // Invert display
	case KEY_COMPOSE: return TRUE; // Turn on touch keys mode
	}

	return FALSE;
}

void input_meta_run_single_function_key(struct kbd_ctx* ctx, uint8_t keycode)
{
	switch (keycode) {

	case KEY_T:
		input_report_key(ctx->input_dev, KEY_TAB, 1);
		input_report_key(ctx->input_dev, KEY_TAB, 0);
		input_meta_disable(ctx);
		return;

	case KEY_X:
		input_send_control(ctx);
		input_meta_disable(ctx);
		return;

	case KEY_C:
		input_send_alt(ctx);
		input_meta_disable(ctx);
		return;

	case KEY_0:
		input_display_invert(ctx);
		input_meta_disable(ctx);
		return;

	case KEY_COMPOSE:
		// First click of Compose enters meta mode (already here)
		// Second click of Compose enters touch keys mode.
		// Subsequent clicks are Enter.
		if (ctx->meta_touch_keys_mode) {
			input_report_key(ctx->input_dev, KEY_ENTER, 1);
			input_report_key(ctx->input_dev, KEY_ENTER, 0);

		} else {
			input_meta_enable_touch_keys_mode(ctx);
		}
		return;

	case KEY_N: input_fw_decrease_brightness(ctx); return;
	case KEY_M: input_fw_increase_brightness(ctx); return;
	case KEY_MUTE:
		input_fw_toggle_brightness(ctx);
		input_meta_disable(ctx);
		return;
	}
}

// Called after checking "single function" meta mode keys,
// These keys, both press and release events, will be sent to the input system
uint8_t input_meta_map_repeatable_key(struct kbd_ctx* ctx, uint8_t keycode)
{
	switch (keycode) {

	case KEY_E: return KEY_UP;
	case KEY_S: return KEY_DOWN;
	case KEY_W: return KEY_LEFT;
	case KEY_D: return KEY_RIGHT;

	case KEY_R: return KEY_HOME;
	case KEY_F: return KEY_END;

	case KEY_O: return KEY_PAGEUP;
	case KEY_P: return KEY_PAGEDOWN;

	case KEY_Q: return 172;
	case KEY_A: return 173;
	}

	// No meta mode match, disable and return original key
	input_meta_disable(ctx);
	return keycode;
}

int input_meta_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	ctx->meta_enabled = 0;
	ctx->meta_touch_keys_mode = 0;

	return 0;
}

