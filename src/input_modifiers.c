// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * input_modifiers.c: Sticky modifier keys implementation
 */

#include <linux/input.h>
#include <linux/module.h>

#include "config.h"
#include "debug_levels.h"

#include "input_iface.h"
#include "params_iface.h"

#include "bbq20kbd_pmod_codes.h"

#include "indicators.h"

#define SYMBOL_OVERLAY_PATH "/sbin/symbol-overlay"

struct sticky_modifier
{
	uint8_t active;
	uint8_t held;
	uint8_t pending;
	uint8_t sticky;
	uint8_t locked;

	// Keycode to send to the input system when applied
	uint8_t keycode;

	// Display indicator index and code
	uint8_t indicator_idx;
	unsigned char const* indicator_pixels;

	// When sticky modifier system has determined that
	// modifier should be applied, run this callback
	// and report the returned keycode result to the input system
	void (*set_callback)(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier);
	void (*unset_callback)(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier);
	void (*lock_callback)(struct kbd_ctx* ctx, struct sticky_modifier* sticky_modifier);
	uint8_t(*map_callback)(struct kbd_ctx* ctx, uint8_t keycode);
};

// Globals

// "Real" modifiers like Shift and Control are handled by simulating
// input key events. Since phys. alt is hardcoded, the state is here
static uint8_t g_apply_phys_alt;
// Store the last keycode sent in the phys. alt map to simulate a key
// up event when the key is released after phys. alt is released
static uint8_t g_current_phys_alt_keycode;
// Clear the symbol menu overlay when Sym indicator cleared
static uint8_t g_showing_sym_menu;

// Sticky modifier structs
static struct sticky_modifier g_sticky_ctrl;
static struct sticky_modifier g_sticky_shift;
static struct sticky_modifier g_sticky_phys_alt;
static struct sticky_modifier g_sticky_alt;
static struct sticky_modifier g_sticky_altgr;

// Sticky modifier helpers

static void press_sticky_modifier(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier)
{
	input_report_key(ctx->input_dev, sticky_modifier->keycode, TRUE);
}

static void release_sticky_modifier(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier)
{
	input_report_key(ctx->input_dev, sticky_modifier->keycode, FALSE);
}

static void lock_sticky_modifier(struct kbd_ctx* ctx, struct sticky_modifier* sticky_modifier)
{
	sticky_modifier->locked = 1;

	// Report modifier to input system as pressed
	sticky_modifier->set_callback(ctx, sticky_modifier);
}

static void enable_phys_alt(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier)
{
	g_apply_phys_alt = 1;
}

static void disable_phys_alt(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier)
{
	// Send key up event if there is a current phys. alt key being held
	if (g_current_phys_alt_keycode) {
		input_report_key(ctx->input_dev, g_current_phys_alt_keycode, FALSE);
		g_current_phys_alt_keycode = 0;
	}

	g_apply_phys_alt = 0;
}

static uint8_t map_phys_alt_keycode(struct kbd_ctx* ctx, uint8_t keycode)
{
	if (!g_apply_phys_alt) {
		return keycode;
	}

	keycode += 119; // See map file for result keys
	g_current_phys_alt_keycode = keycode;
	return keycode;
}

// Call symbol menu overlay helper
// Will return normally if overlay is not installed
static void show_sym_menu(struct kbd_ctx* ctx, struct sticky_modifier* sticky_modifier)
{
	static char const* overlay_argv[] = {SYMBOL_OVERLAY_PATH, NULL, NULL};

	g_showing_sym_menu = 1;

	// Call overlay helper
	overlay_argv[1] = params_get_sharp_path();
	call_usermodehelper(overlay_argv[0], (char**)overlay_argv, NULL, UMH_NO_WAIT);
}

// Sticky modifier keys follow BB Q10 convention
// Holding modifier while typing alpha keys will apply to all alpha keys
// until released.
// One press and release will enter sticky mode, apply modifier key to
// the next alpha key only. If the same modifier key is pressed and
// released again in sticky mode, it will be canceled.
static void transition_sticky_modifier(struct kbd_ctx* ctx,
	struct sticky_modifier* mod, enum rp2040_key_state state)
{
	if (state == KEY_STATE_PRESSED) {

		// Set "held" state
		mod->held = 1;

		// If pressed again while sticky, clear sticky
		if (mod->sticky) {
			mod->sticky = 0;

		// Otherwise, set pending sticky to be applied on release
		} else {
			mod->pending = 1;
		}

		// In locked mode
		if (mod->locked) {

			// Clear lock mode
			mod->locked = 0;

			// Clear pending sticky so that it is not applied to next key
			mod->pending = 0;
		}

		// Report modifier to input system as pressed
		mod->set_callback(ctx, mod);

		// Set display indicator
		if (mod->indicator_pixels) {
			input_display_set_indicator(mod->indicator_idx,
				mod->indicator_pixels);
		}

	// Released
	} else if (state == KEY_STATE_RELEASED) {

		// Unset "held" state
		mod->held = 0;

		// Not in locked mode
		if (!mod->locked) {

			// If any alpha key was typed during hold,
			// `apply_sticky_modifiers` will clear "pending sticky" state.
			// If still in "pending sticky", set "sticky" state.
			if (mod->pending) {

				mod->sticky = 1;
				mod->pending = 0;

			} else {
				// Clear display indicator
				input_display_clear_indicator(mod->indicator_idx);

				// Clear symbol menu overlay if it was showing
				if (mod->keycode == KEY_RIGHTALT) {
					input_display_clear_overlays();
					g_showing_sym_menu = 0;
				}
			}

			// Report modifier to input system as released
			mod->unset_callback(ctx, mod);
		}

	// Held
	} else if (state == KEY_STATE_HOLD) {

		// If any alpha key was typed during hold,
		// `apply_sticky_modifiers` will clear "pending sticky" state.
		// If still in "pending sticky", set locked mode
		if (mod->pending && mod->lock_callback) {
			mod->lock_callback(ctx, mod);
		}
	}
}

// Called before sending an alpha key to apply any pending sticky modifiers
static void apply_sticky_modifier(struct kbd_ctx* ctx,
	struct sticky_modifier* mod)
{
	if (mod->held) {
		mod->pending = 0;

	} else if (mod->sticky) {
		mod->set_callback(ctx, mod);
	}
}

// Called after sending the alpha key to reset
// any sticky modifiers
static void reset_sticky_modifier(struct kbd_ctx* ctx,
	struct sticky_modifier* mod)
{
	if (mod->sticky) {
		mod->sticky = 0;

		mod->unset_callback(ctx, mod);

		// Clear display indicator
		input_display_clear_indicator(mod->indicator_idx);

		// Clear symbol menu overlay if it was showing
		if (mod->keycode == KEY_RIGHTALT) {
			input_display_clear_overlays();
			g_showing_sym_menu = 0;
		}
	}
}

int input_modifiers_consumes_keycode(struct kbd_ctx* ctx,
	uint8_t *remapped_keycode, uint8_t keycode, uint8_t state)
{

	if ((keycode == KEY_LEFTSHIFT) || (keycode == KEY_RIGHTSHIFT)) {
		transition_sticky_modifier(ctx, &g_sticky_shift, state);
		return 1;

	} else if (keycode == KEY_LEFTALT) {
		transition_sticky_modifier(ctx, &g_sticky_phys_alt, state);
		return 1;

	} else if (keycode == KEY_RIGHTALT) {
		transition_sticky_modifier(ctx, &g_sticky_altgr, state);
		return 1;

	} else if (keycode == KEY_OPEN) {
		transition_sticky_modifier(ctx, &g_sticky_ctrl, state);
		return 1;
	}

	return 0;
}

uint8_t input_modifiers_apply_pending(struct kbd_ctx* ctx, uint8_t keycode)
{
	// Apply pending sticky modifiers
	apply_sticky_modifier(ctx, &g_sticky_shift);
	apply_sticky_modifier(ctx, &g_sticky_ctrl);
	apply_sticky_modifier(ctx, &g_sticky_phys_alt);
	apply_sticky_modifier(ctx, &g_sticky_alt);
	apply_sticky_modifier(ctx, &g_sticky_altgr);

	// Map phys. alt
	return g_sticky_phys_alt.map_callback(ctx, keycode);
}

void input_modifiers_reset(struct kbd_ctx* ctx)
{
	// Reset sticky modifiers
	reset_sticky_modifier(ctx, &g_sticky_shift);
	reset_sticky_modifier(ctx, &g_sticky_ctrl);
	reset_sticky_modifier(ctx, &g_sticky_phys_alt);
	reset_sticky_modifier(ctx, &g_sticky_alt);
	reset_sticky_modifier(ctx, &g_sticky_altgr);
}

void input_modifiers_send_control(struct kbd_ctx* ctx)
{
	transition_sticky_modifier(ctx, &g_sticky_ctrl, KEY_STATE_PRESSED);
	transition_sticky_modifier(ctx, &g_sticky_ctrl, KEY_STATE_RELEASED);
}

void input_modifiers_send_alt(struct kbd_ctx* ctx)
{
	transition_sticky_modifier(ctx, &g_sticky_alt, KEY_STATE_PRESSED);
	transition_sticky_modifier(ctx, &g_sticky_alt, KEY_STATE_RELEASED);
}

static void default_init_sticky_modifier(struct sticky_modifier* mod)
{
	mod->held = 0;
	mod->pending = 0;
	mod->sticky = 0;
	mod->locked = 0;

	mod->keycode = 0;
	mod->set_callback = NULL;
	mod->unset_callback = NULL;
	mod->map_callback = NULL;
	mod->indicator_idx = 0;
	mod->indicator_pixels = NULL;
}

int input_modifiers_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	g_apply_phys_alt = 0;
	g_current_phys_alt_keycode = 0;
	g_showing_sym_menu = 0;

	// Initialize sticky modifiers
	default_init_sticky_modifier(&g_sticky_ctrl);
	g_sticky_ctrl.keycode = KEY_LEFTCTRL;
	g_sticky_ctrl.set_callback = press_sticky_modifier;
	g_sticky_ctrl.unset_callback = release_sticky_modifier;
	g_sticky_ctrl.lock_callback = lock_sticky_modifier;
	g_sticky_ctrl.indicator_idx = 2;
	g_sticky_ctrl.indicator_pixels = ind_control;

	default_init_sticky_modifier(&g_sticky_shift);
	g_sticky_shift.keycode = KEY_LEFTSHIFT;
	g_sticky_shift.set_callback = press_sticky_modifier;
	g_sticky_shift.unset_callback = release_sticky_modifier;
	g_sticky_shift.lock_callback = lock_sticky_modifier;
	g_sticky_shift.indicator_idx = 0;
	g_sticky_shift.indicator_pixels = ind_shift;

	default_init_sticky_modifier(&g_sticky_phys_alt);
	g_sticky_phys_alt.keycode = KEY_RIGHTCTRL;
	g_sticky_phys_alt.set_callback = enable_phys_alt;
	g_sticky_phys_alt.unset_callback = disable_phys_alt;
	g_sticky_phys_alt.lock_callback = lock_sticky_modifier;
	g_sticky_phys_alt.map_callback = map_phys_alt_keycode;
	g_sticky_phys_alt.indicator_idx = 1;
	g_sticky_phys_alt.indicator_pixels = ind_phys_alt;

	default_init_sticky_modifier(&g_sticky_alt);
	g_sticky_alt.keycode = KEY_LEFTALT;
	g_sticky_alt.set_callback = press_sticky_modifier;
	g_sticky_alt.unset_callback = release_sticky_modifier;
	g_sticky_alt.lock_callback = lock_sticky_modifier;
	g_sticky_alt.indicator_idx = 3;
	g_sticky_alt.indicator_pixels = ind_alt;

	default_init_sticky_modifier(&g_sticky_altgr);
	g_sticky_altgr.keycode = KEY_RIGHTALT;
	g_sticky_altgr.set_callback = press_sticky_modifier;
	g_sticky_altgr.unset_callback = release_sticky_modifier;
	g_sticky_altgr.lock_callback = show_sym_menu;
	g_sticky_altgr.indicator_idx = 4;
	g_sticky_altgr.indicator_pixels = ind_altgr;

	return 0;
}

void input_modifiers_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{}

// Clear the shift held state
// Touch layer enables touch scrolling while shift is held,
// so if any touch input was entered, it will clear pending shift
void input_modifiers_reset_shift(struct kbd_ctx* ctx)
{
	g_sticky_shift.pending = 0;
	g_sticky_shift.unset_callback(ctx, &g_sticky_shift);
	input_display_clear_indicator(g_sticky_shift.indicator_idx);
}
