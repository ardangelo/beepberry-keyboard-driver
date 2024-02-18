// SPDX-License-Identifier: GPL-2.0-only
// Input meta mode subsystem

#include <linux/input.h>

#include "config.h"
#include "input_iface.h"
#include "params_iface.h"

#include "indicators.h"

#define SYMBOL_OVERLAY_PATH "/sbin/symbol-overlay"

// Globals

static uint8_t g_enabled;
static uint8_t g_showing_indicator;
// Clear the Meta menu overlay when Meta indicator cleared
static uint8_t g_showing_overlay;
// Store the last keycode sent to simulate a key
// up event when the key is released after Meta is exited
static uint8_t g_current_meta_keycode;

// Call Meta menu overlay helper
// Will return normally if overlay is not installed
static void show_meta_menu(struct kbd_ctx* ctx)
{
	static char const* overlay_argv[] = {SYMBOL_OVERLAY_PATH, "--meta", NULL, NULL};

	if (g_showing_overlay) {
		return;
	}

	g_showing_overlay = 1;

	// Call overlay helper
	overlay_argv[2] = params_get_sharp_path();
	call_usermodehelper(overlay_argv[0], (char**)overlay_argv, NULL, UMH_NO_WAIT);
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
	case KEY_ESC: return TRUE; // Exit meta mode
	case KEY_PROPS: return TRUE; // Exit meta mode
	}

	return FALSE;
}

// Return whether or not to exit meta mode
static int run_single_function_key(struct kbd_ctx* ctx, uint8_t keycode)
{
	switch (keycode) {

	case KEY_T:
		input_report_key(ctx->input_dev, KEY_TAB, 1);
		input_report_key(ctx->input_dev, KEY_TAB, 0);
		return 1;

	case KEY_X:
		input_modifiers_send_control(ctx);
		return 1;

	case KEY_C:
		input_modifiers_send_alt(ctx);
		return 1;

	case KEY_0:
		input_display_invert(ctx);
		return 1;

	case KEY_ESC:
	case KEY_PROPS:
		return 1;

	case KEY_N: input_fw_decrease_brightness(ctx); return 0;
	case KEY_M: input_fw_increase_brightness(ctx); return 0;
	case KEY_MUTE:
		input_fw_toggle_brightness(ctx);
		return 1;
	}

	return 0;
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

	return 0;
}

int input_meta_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	g_enabled = 0;
	g_showing_indicator = 0;
	g_showing_overlay = 0;

	return 0;
}

void input_meta_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	input_meta_disable(ctx);
}

int input_meta_consumes_keycode(struct kbd_ctx* ctx,
	uint8_t *remapped_keycode, uint8_t keycode, uint8_t state)
{
	uint8_t simulated_keycode;

	// Not in meta mode
	if (!g_enabled) {

		// Berry key enables meta mode
		if (keycode == KEY_PROPS) {

			if (state == KEY_STATE_RELEASED) {
				input_meta_enable(ctx);

			} else if (state == KEY_STATE_HOLD) {
				show_meta_menu(ctx);
			}
			return 1;
		}

		return 0;
	}

	// Ignore modifier keys in meta mode
	if ((keycode == KEY_LEFTSHIFT) || (keycode == KEY_RIGHTSHIFT)
	 || (keycode == KEY_LEFTALT) || (keycode == KEY_RIGHTALT)
	 || (keycode == KEY_LEFTCTRL) || (keycode == KEY_RIGHTCTRL)) {
		return 1;
	}

	// Keys in Meta mode will clear overlay
	if (g_showing_overlay) {
		input_display_clear_overlays();
		g_showing_overlay = 0;
		// Re-display indicator after clearing
		if (g_showing_indicator) {
			input_display_set_indicator(5, ind_meta);
		}
	}

	// Handle function dispatch meta mode keys
	if (is_single_function_key(ctx, keycode)) {
		if (state == KEY_STATE_RELEASED) {

			// Function will return whether to exit meta
			if (run_single_function_key(ctx, keycode)) {
				input_meta_disable(ctx);
			}
		}
		return 1;
	}

	// Remap to meta mode key
	simulated_keycode = map_repeatable_key(ctx, keycode);

	// No mapped meta mode key, disable and pass through key
	if (simulated_keycode == 0) {
		input_meta_disable(ctx);
		return 0;
	}

	// Report key to input system
	input_report_key(ctx->input_dev, simulated_keycode,
		state == KEY_STATE_PRESSED);

	// Save remapped key
	g_current_meta_keycode = (state == KEY_STATE_PRESSED)
		? simulated_keycode
		: 0;

	return 1;
}

void input_meta_enable(struct kbd_ctx* ctx)
{
	g_enabled = 1;
	g_current_meta_keycode = 0;

	// Set display indicator
	if (!g_showing_indicator) {
		input_display_set_indicator(5, ind_meta);
		g_showing_indicator = 1;
	}
}

void input_meta_disable(struct kbd_ctx* ctx)
{
	g_enabled = 0;

	// Clear display indicator
	if (g_showing_indicator) {
		input_display_clear_indicator(5);
		g_showing_indicator = 0;
	}

	// Simulate key up event
	if (g_current_meta_keycode) {
		input_report_key(ctx->input_dev, g_current_meta_keycode, FALSE);
		g_current_meta_keycode = 0;
	}
}
