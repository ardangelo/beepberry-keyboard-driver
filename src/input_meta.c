// SPDX-License-Identifier: GPL-2.0-only
// Input meta mode subsystem

#include <linux/input.h>

#include "config.h"
#include "input_iface.h"

// Globals

static uint8_t g_enabled;

// Meta mode helpers

static void input_meta_enable(struct kbd_ctx* ctx)
{
	g_enabled = 1;

	// Set display indicator
	input_display_set_indicator(5, 'm');
}

static void input_meta_disable(struct kbd_ctx* ctx)
{
	g_enabled = 0;

	// Clear display indicator
	input_display_clear_indicator(5);


	// Reset touch mode
	if (ctx->touch.activation == TOUCH_ACT_META) {

		// Disable touch interrupts on I2C
		input_fw_disable_touch_interrupts(ctx);
	}
}

// Called before checking "repeatable" meta mode keys,
// These keys map to an internal driver function rather than another key
// They will not be sent to the input system
// The check is separate from the run so that key-up events can be ignored
static bool is_single_function_key(struct kbd_ctx* ctx, uint8_t keycode)
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

static void run_single_function_key(struct kbd_ctx* ctx, uint8_t keycode)
{
	switch (keycode) {

	case KEY_T:
		input_report_key(ctx->input_dev, KEY_TAB, 1);
		input_report_key(ctx->input_dev, KEY_TAB, 0);
		input_meta_disable(ctx);
		return;

	case KEY_X:
		input_modifiers_send_control(ctx);
		input_meta_disable(ctx);
		return;

	case KEY_C:
		input_modifiers_send_alt(ctx);
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
		if (ctx->touch.enabled) {
			input_report_key(ctx->input_dev, KEY_ENTER, 1);
			input_report_key(ctx->input_dev, KEY_ENTER, 0);

		} else if (ctx->touch.activation == TOUCH_ACT_META) {

			// Enable touch interrupts on I2C
			input_fw_enable_touch_interrupts(ctx);
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
static uint8_t map_repeatable_key(struct kbd_ctx* ctx, uint8_t keycode)
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
	g_enabled = 0;

	return 0;
}

void input_meta_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{}

int input_meta_consumes_keycode(struct kbd_ctx* ctx,
	uint8_t *remapped_keycode, uint8_t keycode, uint8_t state)
{
	uint8_t simulated_keycode;

	if (!g_enabled) {
		return 0;
	}

	// Compose key enters meta mode if not always enabled
	if ((keycode == KEY_COMPOSE)
	    && (ctx->touch.activation == TOUCH_ACT_META)
	    && !g_enabled) {

		if (state == KEY_STATE_RELEASED) {
			input_meta_enable(ctx);
		}

		return 1;
	}

	// Ignore modifier keys in meta mode
	if ((keycode == KEY_LEFTSHIFT) || (keycode == KEY_RIGHTSHIFT)
	 || (keycode == KEY_LEFTALT) || (keycode == KEY_RIGHTALT)
	 || (keycode == KEY_LEFTCTRL) || (keycode == KEY_RIGHTCTRL)) {
		return 1;
	}

	// Escape key exits meta mode
	if (keycode == KEY_ESC) {
		if (state == KEY_STATE_RELEASED) {
			input_meta_disable(ctx);
		}
		return 1;
	}

	// Handle function dispatch meta mode keys
	if (is_single_function_key(ctx, keycode)) {
		if (state == KEY_STATE_RELEASED) {
			run_single_function_key(ctx, keycode);
		}
		return 1;
	}

	// Remap to meta mode key
	simulated_keycode = map_repeatable_key(ctx, keycode);

	// Input consumed by meta mode with no remmaped key
	if (simulated_keycode == 0) {
		return 1;
	}

	// Report key to input system
	input_report_key(ctx->input_dev, simulated_keycode,
		state == KEY_STATE_PRESSED);

	// If exited meta mode, simulate key up event. Otherwise, input system
	// will have remapped key as in the down state
	if (!g_enabled && (simulated_keycode != keycode)) {
		input_report_key(ctx->input_dev, simulated_keycode, FALSE);
	}

	return 0;
}
