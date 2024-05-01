// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * input_iface.c: Key handler implementation
 */

#include <linux/input.h>
#include <linux/module.h>

#include "config.h"
#include "debug_levels.h"

#include "input_iface.h"
#include "i2c_helper.h"

#include "indicators.h"

static uint8_t g_touch_indicator = 0;

static void enable_scale_2x(struct kbd_ctx* ctx)
{
	uint8_t reg;

	// Set touchpad scaling factor to 2x for X and Y
	kbd_write_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_REG, REG_TOUCHPAD_REG_SPEED);
	kbd_read_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_VAL, &reg);
	reg |= REG_TOUCHPAD_SPEED_X_SCALE2;
	reg |= REG_TOUCHPAD_SPEED_Y_SCALE2;
	kbd_write_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_VAL, reg);

	// Enable touchpad scaling
	kbd_write_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_REG, REG_TOUCHPAD_REG_ENGINE);
	kbd_read_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_VAL, &reg);
	reg |= REG_TOUCHPAD_ENGINE_XY_SCALE;
	kbd_write_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_VAL, reg);
}

static void disable_scale_2x(struct kbd_ctx* ctx)
{
	uint8_t reg;

	// Clear touchpad scaling factor to 2x for X and Y
	kbd_write_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_REG, REG_TOUCHPAD_REG_SPEED);
	kbd_read_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_VAL, &reg);
	reg &= ~REG_TOUCHPAD_SPEED_X_SCALE2;
	reg &= ~REG_TOUCHPAD_SPEED_Y_SCALE2;
	kbd_write_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_VAL, reg);

	// Clear touchpad scaling
	kbd_write_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_REG, REG_TOUCHPAD_REG_ENGINE);
	kbd_read_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_VAL, &reg);
	reg &= ~REG_TOUCHPAD_ENGINE_XY_SCALE;
	kbd_write_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_VAL, reg);
}

int input_touch_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	ctx->touch.x = 0;
	ctx->touch.dx = 0;
	ctx->touch.y = 0;
	ctx->touch.dy = 0;

	ctx->touch.enable_while_shift_held = 1;
	ctx->touch.entry_while_shift_held = 0;
	ctx->touch.threshold = 8;

	// Default touch settings
	input_touch_set_activation(ctx, TOUCH_ACT_CLICK);
	input_touch_set_input_as(ctx, TOUCH_INPUT_AS_KEYS);

	return 0;
}

void input_touch_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{}

void input_touch_report_event(struct kbd_ctx *ctx)
{
	uint8_t x_threshold, y_threshold;
#if (DEBUG_LEVEL & DEBUG_LEVEL_FE)
	uint8_t qual;
#endif

	if (!ctx || !ctx->touch.enabled
	 || ((ctx->touch.dx == 0) && (ctx->touch.dy == 0))) {
		return;
	}

	// Set minimum touch thresholds
	x_threshold = ctx->touch.threshold;
	y_threshold = ctx->touch.threshold;

	// Log touchpad surface quality
#if (DEBUG_LEVEL & DEBUG_LEVEL_FE)
	kbd_write_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_REG, 0x05);
	kbd_read_i2c_u8(ctx->i2c_client, REG_TOUCHPAD_VAL, &qual);

	dev_info_fe(&ctx->i2c_client->dev,
		"Touch (%d, %d) Qual %d\n",
		ctx->touch.dx, ctx->touch.dy, qual);
#endif

	// Report mouse movement
	if (ctx->touch.input_as == TOUCH_INPUT_AS_MOUSE) {

		// Report mouse movement
		input_report_rel(ctx->input_dev, REL_X, (int8_t)ctx->touch.dx);
		input_report_rel(ctx->input_dev, REL_Y, (int8_t)ctx->touch.dy);
		ctx->touch.dx = 0;
		ctx->touch.dy = 0;

		// Reset shift sticky state if touch entry was sent while held
		ctx->touch.entry_while_shift_held = 1;

	// Report arrow key movement
	} else if (ctx->touch.input_as == TOUCH_INPUT_AS_KEYS) {

		// Accumulate X / Y
		ctx->touch.x += ctx->touch.dx;
		ctx->touch.y += ctx->touch.dy;

		// Snap movement to arrow keys directions
		if ((ctx->touch.dx == 0) && (abs(ctx->touch.x) < x_threshold)) {
			ctx->touch.x = 0;
		}
		if ((ctx->touch.dy == 0) && (abs(ctx->touch.y) < y_threshold)) {
			ctx->touch.y = 0;
		}
		ctx->touch.dx = 0;
		ctx->touch.dy = 0;

		// Reset shift sticky state if touch entry was sent while held
		ctx->touch.entry_while_shift_held = 1;

		// Negative X: left arrow key
		if (ctx->touch.x <= -x_threshold) {

			do {
				input_report_key(ctx->input_dev, KEY_LEFT, TRUE);
				input_report_key(ctx->input_dev, KEY_LEFT, FALSE);
				ctx->touch.x += x_threshold;
			} while (ctx->touch.x <= -x_threshold);

		// Positive X: right arrow key
		} else if (ctx->touch.x > x_threshold) {

			do {
				input_report_key(ctx->input_dev, KEY_RIGHT, TRUE);
				input_report_key(ctx->input_dev, KEY_RIGHT, FALSE);
				ctx->touch.x -= x_threshold;
			} while (ctx->touch.x > x_threshold);
		}

		// Negative Y: up arrow key
		if (ctx->touch.y <= -y_threshold) {

			do {
				input_report_key(ctx->input_dev, KEY_UP, TRUE);
				input_report_key(ctx->input_dev, KEY_UP, FALSE);
				ctx->touch.y += y_threshold;
			} while (ctx->touch.y <= -y_threshold);

		// Positive Y: down arrow key
		} else if (ctx->touch.y > y_threshold) {

			do {
				input_report_key(ctx->input_dev, KEY_DOWN, TRUE);
				input_report_key(ctx->input_dev, KEY_DOWN, FALSE);
				ctx->touch.y -= y_threshold;
			} while (ctx->touch.y > y_threshold);
		}
	}
}

// Touch enabled: touchpad click sends enter / mouse click
// Touch disabled: touchpad click enables touch mode
int input_touch_consumes_keycode(struct kbd_ctx* ctx,
	uint8_t *remapped_keycode, uint8_t keycode, uint8_t state)
{
	// Touchpad click
	// Touch off: enable touch
	// Touch on: enter or mouse click
	if (keycode == KEY_COMPOSE) {

		if (ctx->touch.enabled) {

			// Keys mode, send enter
			if ((ctx->touch.input_as == TOUCH_INPUT_AS_KEYS)
			 && (state == KEY_STATE_RELEASED)) {
				input_report_key(ctx->input_dev, KEY_ENTER, TRUE);
				input_report_key(ctx->input_dev, KEY_ENTER, FALSE);

			// Mouse mode, send mouse click
			} else if (ctx->touch.input_as == TOUCH_INPUT_AS_MOUSE) {
				input_report_key(ctx->input_dev, BTN_LEFT,
					(state == KEY_STATE_PRESSED));
			}

			return 1;

		// If touch off, touchpad click will turn touch on
		} else if (state == KEY_STATE_RELEASED) {
			input_touch_enable(ctx);

			// Don't show indicator in mouse mode
			if (ctx->touch.input_as == TOUCH_INPUT_AS_KEYS) {
				input_touch_set_indicator(ctx);
			}
		}

	// Back key disables touch mode if touch enabled
	} else if (ctx->touch.enabled && (keycode == KEY_ESC)) {

		if (state == KEY_STATE_RELEASED) {
			input_touch_disable(ctx);
		}

		return 1;

	// Enable touch while shift is held
	} else if ((keycode == KEY_LEFTSHIFT) || (keycode == KEY_RIGHTSHIFT)) {
		if ((ctx->touch.activation == TOUCH_ACT_CLICK)
		 && ctx->touch.enable_while_shift_held) {

			if (!ctx->touch.enabled && (state == KEY_STATE_PRESSED)) {
				ctx->touch.entry_while_shift_held = 0;
				input_touch_enable(ctx);

			} else if (ctx->touch.enabled && (state == KEY_STATE_RELEASED)) {
				input_touch_disable(ctx);
				if (ctx->touch.entry_while_shift_held) {
					ctx->touch.entry_while_shift_held = 0;
					input_modifiers_reset_shift(ctx);
				}
			}
		}
	}

	return 0;
}

void input_touch_enable(struct kbd_ctx *ctx)
{
	ctx->touch.enabled = 1;
	input_fw_enable_touch_interrupts(ctx);
}

void input_touch_disable(struct kbd_ctx *ctx)
{
	ctx->touch.enabled = 0;
	input_fw_disable_touch_interrupts(ctx);

	if (g_touch_indicator) {
		g_touch_indicator = 0;
		input_display_clear_indicator(6);
	}
}

void input_touch_set_activation(struct kbd_ctx *ctx, uint8_t activation)
{
	if (activation == TOUCH_ACT_ALWAYS) {
		ctx->touch.activation = TOUCH_ACT_ALWAYS;
		input_touch_enable(ctx);

	} else if (activation == TOUCH_ACT_CLICK) {
		ctx->touch.activation = TOUCH_ACT_CLICK;
		input_touch_disable(ctx);
	}
}

void input_touch_set_input_as(struct kbd_ctx *ctx, uint8_t input_as)
{
	ctx->touch.input_as = input_as;

	// Scale setting for touch input as keys
	if (input_as == TOUCH_INPUT_AS_KEYS) {
		enable_scale_2x(ctx);

	// Mouse does not apply scaling
	} else if (input_as == TOUCH_INPUT_AS_MOUSE) {
		disable_scale_2x(ctx);
	}
}

void input_touch_set_threshold(struct kbd_ctx *ctx, uint8_t threshold)
{
	ctx->touch.threshold = threshold;
}

void input_touch_set_indicator(struct kbd_ctx *ctx)
{
	g_touch_indicator = 1;
	input_display_set_indicator(6, ind_touch);
}
