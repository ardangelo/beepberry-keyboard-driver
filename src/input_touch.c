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

int input_touch_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	ctx->touch.activation = TOUCH_ACT_META;
	ctx->touch.input_as = TOUCH_INPUT_AS_KEYS;
	ctx->touch.enabled = 0;
	ctx->touch.rel_x = 0;
	ctx->touch.rel_y = 0;

	return 0;
}

void input_touch_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{}

void input_touch_report_event(struct kbd_ctx *ctx)
{
	if (!ctx || !ctx->touch.enabled) {
		return;
	}

	dev_info_ld(&ctx->i2c_client->dev,
		"%s X Reg: %d Y Reg: %d.\n",
		__func__, ctx->touch.rel_x, ctx->touch.rel_y);

	// Report mouse movement
	if (ctx->touch.input_as == TOUCH_INPUT_AS_MOUSE) {
#if 0
		input_report_mouse(ctx->input_dev, ctx->touch.rel_x, ctx->touch.rel_y);
#endif

	// Report arrow key movement
	} else if (ctx->touch.input_as == TOUCH_INPUT_AS_KEYS) {

		// Negative X: left arrow key
		if (ctx->touch.rel_x < -4) {
			input_report_key(ctx->input_dev, KEY_LEFT, TRUE);
			input_report_key(ctx->input_dev, KEY_LEFT, FALSE);

		// Positive X: right arrow key
		} else if (ctx->touch.rel_x > 4) {
			input_report_key(ctx->input_dev, KEY_RIGHT, TRUE);
			input_report_key(ctx->input_dev, KEY_RIGHT, FALSE);
		}

		// Negative Y: up arrow key
		if (ctx->touch.rel_y < -4) {
			input_report_key(ctx->input_dev, KEY_UP, TRUE);
			input_report_key(ctx->input_dev, KEY_UP, FALSE);

		// Positive Y: down arrow key
		} else if (ctx->touch.rel_y > 4) {
			input_report_key(ctx->input_dev, KEY_DOWN, TRUE);
			input_report_key(ctx->input_dev, KEY_DOWN, FALSE);
		}
	}
}

// Touch enabled: touchpad click sends enter / mouse click
// Touch disabled: touchpad click enters meta mode
int input_touch_consumes_keycode(struct kbd_ctx* ctx,
	uint8_t *remapped_keycode, uint8_t keycode, uint8_t state)
{
	// Touchpad click
	// Touch off: enable meta mode
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
				// TODO: mouse click
			}

			return 1;
		}

		// If touch off, touchpad click will be handled by meta handler

	// Berry key
	// Touch off: sends Tmux prefix (Control + code 171 in keymap)
	// Touch on: enable meta mode
	} else if (keycode == KEY_PROPS) {

		if (state == KEY_STATE_RELEASED) {

			// Send Tmux prefix
			if (!ctx->touch.enabled) {
				input_report_key(ctx->input_dev, KEY_LEFTCTRL, TRUE);
				input_report_key(ctx->input_dev, 171, TRUE);
				input_report_key(ctx->input_dev, 171, FALSE);
				input_report_key(ctx->input_dev, KEY_LEFTCTRL, FALSE);

			// Enable meta mode
			} else {
				input_meta_enable(ctx);
			}
		}

		return 1;
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
}

void input_touch_set_activation(struct kbd_ctx *ctx, uint8_t activation)
{
	if (activation == TOUCH_ACT_ALWAYS) {
		ctx->touch.activation = TOUCH_ACT_ALWAYS;
		input_touch_enable(ctx);

	} else if (activation == TOUCH_ACT_META) {
		ctx->touch.activation = TOUCH_ACT_META;
		input_touch_disable(ctx);
	}
}

void input_touch_set_input_as(struct kbd_ctx *ctx, uint8_t input_as)
{
	ctx->touch.input_as = input_as;
}
