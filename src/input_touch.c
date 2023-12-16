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
	dev_info_ld(&ctx->i2c_client->dev,
		"%s X Reg: %d Y Reg: %d.\n",
		__func__, ctx->touch.rel_x, ctx->touch.rel_y);

	#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)

		// Report mouse movement
		input_report_rel(input_dev, REL_X, ctx->touch.rel_x);
		input_report_rel(input_dev, REL_Y, ctx->touch.rel_y);

	#endif

	#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_KEYS)

	// Touchpad-as-keys mode will always use the touchpad as arrow keys
	if (ctx->touch.activation == TOUCH_ACT_ALWAYS) {

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

	// Meta touch keys will only send up/down keys when enabled
	} else if ((ctx->touch.activation == TOUCH_ACT_META)
	        && ctx->touch.enabled) {

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
	#endif
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
