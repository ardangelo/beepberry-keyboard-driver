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

#include "bbq20kbd_pmod_codes.h"

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

// Sticky modifier structs
static struct sticky_modifier sticky_ctrl;
static struct sticky_modifier sticky_shift;
static struct sticky_modifier sticky_phys_alt;
static struct sticky_modifier sticky_alt;
static struct sticky_modifier sticky_altgr;

// Sticky modifier helpers

static void press_sticky_modifier(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier)
{
	input_report_key(ctx->input_dev, sticky_modifier->keycode, TRUE);
}

static void release_sticky_modifier(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier)
{
	input_report_key(ctx->input_dev, sticky_modifier->keycode, FALSE);
}

static void enable_phys_alt(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier)
{
	ctx->modifiers.apply_phys_alt = 1;
}

static void disable_phys_alt(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier)
{
	// Send key up event if there is a current phys. alt key being held
	if (ctx->modifiers.current_phys_alt_keycode) {
		input_report_key(ctx->input_dev,
			ctx->modifiers.current_phys_alt_keycode, FALSE);
		ctx->modifiers.current_phys_alt_keycode = 0;
	}

	ctx->modifiers.apply_phys_alt = 0;
}

static uint8_t map_phys_alt_keycode(struct kbd_ctx* ctx, uint8_t keycode)
{
	if (!ctx->modifiers.apply_phys_alt) {
		return keycode;
	}

	keycode += 119; // See map file for result keys
	ctx->modifiers.current_phys_alt_keycode = keycode;
	return keycode;
}

// Sticky modifier keys follow BB Q10 convention
// Holding modifier while typing alpha keys will apply to all alpha keys
// until released.
// One press and release will enter sticky mode, apply modifier key to
// the next alpha key only. If the same modifier key is pressed and
// released again in sticky mode, it will be canceled.
static void transition_sticky_modifier(struct kbd_ctx* ctx,
	struct sticky_modifier const* sticky_modifier, enum rp2040_key_state state)
{
	if (state == KEY_STATE_PRESSED) {

		// Set "held" state
		ctx->modifiers.held_modifier_keys |= sticky_modifier->bit;

		// If pressed again while sticky, clear sticky
		if (ctx->modifiers.sticky_modifier_keys & sticky_modifier->bit) {
			ctx->modifiers.sticky_modifier_keys &= ~sticky_modifier->bit;

		// Otherwise, set pending sticky to be applied on release
		} else {
			ctx->modifiers.pending_sticky_modifier_keys |= sticky_modifier->bit;
		}

		// In locked mode
		if (ctx->modifiers.locked_modifier_keys & sticky_modifier->bit) {

			// Clear lock mode
			ctx->modifiers.locked_modifier_keys &= ~sticky_modifier->bit;

			// Clear pending sticky so that it is not applied to next key
			ctx->modifiers.pending_sticky_modifier_keys &= ~sticky_modifier->bit;
		}

		// Report modifier to input system as pressed
		sticky_modifier->set_callback(ctx, sticky_modifier);

		// Set display indicator
		input_display_set_indicator(sticky_modifier->indicator_idx,
			sticky_modifier->indicator_char);

	// Released
	} else if (state == KEY_STATE_RELEASED) {

		// Unset "held" state
		ctx->modifiers.held_modifier_keys &= ~sticky_modifier->bit;

		// Not in locked mode
		if (!(ctx->modifiers.locked_modifier_keys & sticky_modifier->bit)) {

			// If any alpha key was typed during hold,
			// `apply_sticky_modifiers` will clear "pending sticky" state.
			// If still in "pending sticky", set "sticky" state.
			if (ctx->modifiers.pending_sticky_modifier_keys & sticky_modifier->bit) {

				ctx->modifiers.sticky_modifier_keys |= sticky_modifier->bit;
				ctx->modifiers.pending_sticky_modifier_keys &= ~sticky_modifier->bit;

			} else {
				// Clear display indicator
				input_display_clear_indicator(sticky_modifier->indicator_idx);
			}

			// Report modifier to input system as released
			sticky_modifier->unset_callback(ctx, sticky_modifier);
		}

	// Held
	} else if (state == KEY_STATE_HOLD) {

		// If any alpha key was typed during hold,
		// `apply_sticky_modifiers` will clear "pending sticky" state.
		// If still in "pending sticky", set locked mode
		if (ctx->modifiers.pending_sticky_modifier_keys & sticky_modifier->bit) {
			ctx->modifiers.locked_modifier_keys |= sticky_modifier->bit;

			// Report modifier to input system as pressed
			sticky_modifier->set_callback(ctx, sticky_modifier);
		}
	}
}

// Called before sending an alpha key to apply any pending sticky modifiers
static void apply_sticky_modifier(struct kbd_ctx* ctx,
	struct sticky_modifier const* sticky_modifier)
{
	if (ctx->modifiers.held_modifier_keys & sticky_modifier->bit) {
		ctx->modifiers.pending_sticky_modifier_keys &= ~sticky_modifier->bit;

	} else if (ctx->modifiers.sticky_modifier_keys & sticky_modifier->bit) {
		sticky_modifier->set_callback(ctx, sticky_modifier);
	}
}

// Called after sending the alpha key to reset
// any sticky modifiers
static void reset_sticky_modifier(struct kbd_ctx* ctx,
	struct sticky_modifier const* sticky_modifier)
{
	if (ctx->modifiers.sticky_modifier_keys & sticky_modifier->bit) {
		ctx->modifiers.sticky_modifier_keys &= ~sticky_modifier->bit;

		sticky_modifier->unset_callback(ctx, sticky_modifier);

		// Clear display indicator
		input_display_clear_indicator(sticky_modifier->indicator_idx);
	}
}

int input_modifiers_consumes_keycode(struct kbd_ctx* ctx,
	uint8_t *remapped_keycode, uint8_t keycode, uint8_t state)
{

	if ((keycode == KEY_LEFTSHIFT) || (keycode == KEY_RIGHTSHIFT)) {
		transition_sticky_modifier(ctx, &sticky_shift, state);
		return 1;

	} else if (keycode == KEY_LEFTALT) {
		transition_sticky_modifier(ctx, &sticky_phys_alt, state);
		return 1;

	} else if (keycode == KEY_RIGHTALT) {
		transition_sticky_modifier(ctx, &sticky_altgr, state);
		return 1;

	} else if (keycode == KEY_OPEN) {
		transition_sticky_modifier(ctx, &sticky_ctrl, state);
		return 1;
	}

	return 0;
}

uint8_t input_modifiers_apply_pending(struct kbd_ctx* ctx, uint8_t keycode)
{
	// Apply pending sticky modifiers
	apply_sticky_modifier(ctx, &sticky_shift);
	apply_sticky_modifier(ctx, &sticky_ctrl);
	apply_sticky_modifier(ctx, &sticky_phys_alt);
	apply_sticky_modifier(ctx, &sticky_alt);
	apply_sticky_modifier(ctx, &sticky_altgr);

	// Map phys. alt
	return sticky_phys_alt.map_callback(ctx, keycode);
}

void input_modifiers_reset(struct kbd_ctx* ctx)
{
	// Reset sticky modifiers
	reset_sticky_modifier(ctx, &sticky_shift);
	reset_sticky_modifier(ctx, &sticky_ctrl);
	reset_sticky_modifier(ctx, &sticky_phys_alt);
	reset_sticky_modifier(ctx, &sticky_alt);
	reset_sticky_modifier(ctx, &sticky_altgr);
}

void input_modifiers_send_control(struct kbd_ctx* ctx)
{
	transition_sticky_modifier(ctx, &sticky_ctrl, KEY_STATE_PRESSED);
	transition_sticky_modifier(ctx, &sticky_ctrl, KEY_STATE_RELEASED);
}

void input_modifiers_send_alt(struct kbd_ctx* ctx)
{
	transition_sticky_modifier(ctx, &sticky_alt, KEY_STATE_PRESSED);
	transition_sticky_modifier(ctx, &sticky_alt, KEY_STATE_RELEASED);
}

int input_modifiers_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	ctx->modifiers.held_modifier_keys = 0;
	ctx->modifiers.pending_sticky_modifier_keys = 0;
	ctx->modifiers.sticky_modifier_keys = 0;
	ctx->modifiers.locked_modifier_keys = 0;
	ctx->modifiers.apply_phys_alt = 0;
	ctx->modifiers.current_phys_alt_keycode = 0;

	// Initialize sticky modifiers
	sticky_ctrl.bit = (1 << 0);
	sticky_ctrl.keycode = KEY_LEFTCTRL;
	sticky_ctrl.set_callback = press_sticky_modifier;
	sticky_ctrl.unset_callback = release_sticky_modifier;
	sticky_ctrl.map_callback = NULL;
	sticky_ctrl.indicator_idx = 2;
	sticky_ctrl.indicator_char = 'c';

	sticky_shift.bit = (1 << 1);
	sticky_shift.keycode = KEY_LEFTSHIFT;
	sticky_shift.set_callback = press_sticky_modifier;
	sticky_shift.unset_callback = release_sticky_modifier;
	sticky_shift.map_callback = NULL;
	sticky_shift.indicator_idx = 0;
	sticky_shift.indicator_char = 's';

	sticky_phys_alt.bit = (1 << 2);
	sticky_phys_alt.keycode = KEY_RIGHTCTRL;
	sticky_phys_alt.set_callback = enable_phys_alt;
	sticky_phys_alt.unset_callback = disable_phys_alt;
	sticky_phys_alt.map_callback = map_phys_alt_keycode;
	sticky_phys_alt.indicator_idx = 1;
	sticky_phys_alt.indicator_char = 'p';

	sticky_alt.bit = (1 << 3);
	sticky_alt.keycode = KEY_LEFTALT;
	sticky_alt.set_callback = press_sticky_modifier;
	sticky_alt.unset_callback = release_sticky_modifier;
	sticky_alt.map_callback = NULL;
	sticky_alt.indicator_idx = 3;
	sticky_alt.indicator_char = 'a';

	sticky_altgr.bit = (1 << 4);
	sticky_altgr.keycode = KEY_RIGHTALT;
	sticky_altgr.set_callback = press_sticky_modifier;
	sticky_altgr.unset_callback = release_sticky_modifier;
	sticky_altgr.map_callback = NULL;
	sticky_altgr.indicator_idx = 4;
	sticky_altgr.indicator_char = 'g';

	return 0;
}

void input_modifiers_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{}