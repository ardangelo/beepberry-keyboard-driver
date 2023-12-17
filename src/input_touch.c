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

int input_touch_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	ctx->touch.activation = TOUCH_ACT_CLICK;
	ctx->touch.input_as = TOUCH_INPUT_AS_KEYS;
	ctx->touch.enabled = 0;
	ctx->touch.dx = 0;
	ctx->touch.dy = 0;

	// High power led
	uint8_t reg;
	(void)kbd_write_i2c_u8(ctx->i2c_client, 0x40, 0x1a);
	(void)kbd_read_i2c_u8(ctx->i2c_client, 0x41, &reg);
	reg = (reg & ~7) | 0x3;
	(void)kbd_write_i2c_u8(ctx->i2c_client, 0x41, reg);

	// Finger dectection
	(void)kbd_write_i2c_u8(ctx->i2c_client, 0x40, 0x60);
	(void)kbd_read_i2c_u8(ctx->i2c_client, 0x41, &reg);
	reg |= 1 << 2;
	(void)kbd_write_i2c_u8(ctx->i2c_client, 0x41, reg);

	return 0;
}

void input_touch_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{}

#define XCS 4
#define YCS 4
#define CSMAX 30

#define LAST_DIRS_LEN 4
static uint16_t last_dirs = 0x0000;
static uint8_t last_dirs_idx = 0;
static int any_in_last_dirs(uint8_t is_x)
{
	int result;
	int dir = (is_x) ? 1 : 2;
	uint16_t pattern = (is_x) ? 0x1111 : 0x2222;

	result = (last_dirs & pattern) ? 1 : 0;

	last_dirs &= (0xffff0fff >> (4 * (last_dirs_idx % LAST_DIRS_LEN)));
	last_dirs |= dir << (4 * ((LAST_DIRS_LEN - 1) - (last_dirs_idx % LAST_DIRS_LEN)));
	last_dirs_idx++;

	return result;
}

void input_touch_report_event(struct kbd_ctx *ctx)
{
	if (!ctx || !ctx->touch.enabled) {
		return;
	}

	// Read quality
	uint8_t qual = 0xcc;
	(void)kbd_write_i2c_u8(ctx->i2c_client, 0x40, 0x5);
	(void)kbd_read_i2c_u8(ctx->i2c_client, 0x41, &qual);
	if (qual < 16) {
		return;
	}

	// Read finger
	uint8_t finger = 0xcc;
	(void)kbd_write_i2c_u8(ctx->i2c_client, 0x40, 0x75);
	(void)kbd_read_i2c_u8(ctx->i2c_client, 0x41, &finger);

	dev_info_fe(&ctx->i2c_client->dev,
		"%s X Reg: %d Y Reg: %d.\nQual %3d Finger 0x%02x\n",
		/*__func__*/"", ctx->touch.dx, ctx->touch.dy, qual, finger);

	// Report mouse movement
	if (ctx->touch.input_as == TOUCH_INPUT_AS_MOUSE) {
#if 0
		input_report_mouse(ctx->input_dev, ctx->touch.dx, ctx->touch.dy);
#endif

	// Report arrow key movement
	} else if (ctx->touch.input_as == TOUCH_INPUT_AS_KEYS) {

		// TODO: configure sensor firmware w/ register
		ctx->touch.dx = -ctx->touch.dx;

		// X or Y smoothing
		if (abs(ctx->touch.dx) > abs(ctx->touch.dy * 2)) {
			ctx->touch.dy = 0;
			if (!any_in_last_dirs(1)) {
				ctx->touch.dx = 0;
			}
		} else if (abs(ctx->touch.dy) > abs(ctx->touch.dx * 2)) {
			ctx->touch.dx = 0;
			if (!any_in_last_dirs(0)) {
				ctx->touch.dy = 0;
			}
		}

		// Negative X: left arrow key
		if (ctx->touch.dx <= -XCS) {

			do {
				input_report_key(ctx->input_dev, KEY_LEFT, TRUE);
				input_report_key(ctx->input_dev, KEY_LEFT, FALSE);
				ctx->touch.dx += XCS;
			} while (ctx->touch.dx <= -XCS);

		// Positive X: right arrow key
		} else if (ctx->touch.dx > XCS) {

			do {
				input_report_key(ctx->input_dev, KEY_RIGHT, TRUE);
				input_report_key(ctx->input_dev, KEY_RIGHT, FALSE);
				ctx->touch.dx -= XCS;
			} while (ctx->touch.dx > XCS);
		}

		// Negative Y: up arrow key
		if (ctx->touch.dy <= -YCS) {

			do {
				input_report_key(ctx->input_dev, KEY_UP, TRUE);
				input_report_key(ctx->input_dev, KEY_UP, FALSE);
				ctx->touch.dy += YCS;
			} while (ctx->touch.dy <= -YCS);

		// Positive Y: down arrow key
		} else if (ctx->touch.dy > YCS) {

			do {
				input_report_key(ctx->input_dev, KEY_DOWN, TRUE);
				input_report_key(ctx->input_dev, KEY_DOWN, FALSE);
				ctx->touch.dy -= YCS;
			} while (ctx->touch.dy > YCS);
		}
	}

	ctx->touch.dx = 0;
	ctx->touch.dy = 0;
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
				// TODO: mouse click
			}

			return 1;

		// If touch off, touchpad click will turn touch on
		} else if (state == KEY_STATE_RELEASED) {
			input_touch_enable(ctx);
		}

	// Back key disables touch mode if touch enabled
	} else if (ctx->touch.enabled && (keycode == KEY_ESC)) {

		if  (state == KEY_STATE_RELEASED) {
			input_touch_disable(ctx);
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

	} else if (activation == TOUCH_ACT_CLICK) {
		ctx->touch.activation = TOUCH_ACT_CLICK;
		input_touch_disable(ctx);
	}
}

void input_touch_set_input_as(struct kbd_ctx *ctx, uint8_t input_as)
{
	ctx->touch.input_as = input_as;
}
