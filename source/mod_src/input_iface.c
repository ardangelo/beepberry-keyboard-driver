// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * input_iface.c: Key handler implementation
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/kdev_t.h>

#include "config.h"
#include "debug_levels.h"

#include "i2c_helper.h"
#include "params_iface.h"
#include "input_iface.h"

#include "bbq20kbd_pmod_codes.h"

// Global keyboard context and sysfs data
struct kbd_ctx *g_ctx = NULL;

// Brightness helpers

static void kbd_decrease_brightness(struct kbd_ctx* ctx)
{
	// Decrease by delta, min at 0x0 brightness
	ctx->brightness = (ctx->brightness < BBQ10_BRIGHTNESS_DELTA)
		? 0x0
		: ctx->brightness - BBQ10_BRIGHTNESS_DELTA;

	// Set backlight using I2C
	(void)kbd_write_i2c_u8(ctx->i2c_client, REG_BKL, ctx->brightness);
}

static void kbd_increase_brightness(struct kbd_ctx* ctx)
{
	// Increase by delta, max at 0xff brightness
	ctx->brightness = (ctx->brightness > (0xff - BBQ10_BRIGHTNESS_DELTA))
		? 0xff
		: ctx->brightness + BBQ10_BRIGHTNESS_DELTA;

	// Set backlight using I2C
	(void)kbd_write_i2c_u8(ctx->i2c_client, REG_BKL, ctx->brightness);
}

static void kbd_toggle_brightness(struct kbd_ctx* ctx)
{
	// Toggle, save last brightness in context
	if (ctx->last_brightness) {
		ctx->brightness = ctx->last_brightness;
		ctx->last_brightness = 0;
	} else {
		ctx->last_brightness = ctx->brightness;
		ctx->brightness = 0;
	}

	// Set backlight using I2C
	(void)kbd_write_i2c_u8(ctx->i2c_client, REG_BKL, ctx->brightness);
}

// I2C FIFO helpers

// Transfer from I2C FIFO to internal context FIFO
static void kbd_read_fifo(struct kbd_ctx* ctx)
{
	uint8_t fifo_idx;
	int rc;

	// Read number of FIFO items
	if (kbd_read_i2c_u8(ctx->i2c_client, REG_KEY, &ctx->fifo_count)) {
		return;
	}
	ctx->fifo_count &= REG_KEY_KEYCOUNT_MASK;

	// Read and transfer all FIFO items
	for (fifo_idx = 0; fifo_idx < ctx->fifo_count; fifo_idx++) {

		// Read 2 fifo items
		if ((rc = kbd_read_i2c_2u8(ctx->i2c_client, REG_FIF,
			(uint8_t*)&ctx->fifo_data[fifo_idx]))) {

			dev_err(&ctx->i2c_client->dev,
				"%s Could not read REG_FIF, Error: %d\n", __func__, rc);
			return;
		}

		// Advance FIFO position
		dev_info_fe(&ctx->i2c_client->dev,
			"%s %02d: 0x%02x%02x Shift %d Ctrl %d AltGr %d State %d Scancode %d\n",
			__func__, fifo_idx,
			((uint8_t*)&ctx->fifo_data[fifo_idx])[0],
			((uint8_t*)&ctx->fifo_data[fifo_idx])[1],
			ctx->fifo_data[fifo_idx].shift_modifier,
			ctx->fifo_data[fifo_idx].ctrl_modifier,
			ctx->fifo_data[fifo_idx].altgr_modifier,
			ctx->fifo_data[fifo_idx].state,
			ctx->fifo_data[fifo_idx].scancode);
	}
}

// Meta mode helpers

static void send_one_key_with_control(struct kbd_ctx* ctx, uint8_t keycode)
{
	input_report_key(ctx->input_dev, KEY_LEFTCTRL, TRUE);
	input_report_key(ctx->input_dev, keycode, TRUE);
	input_report_key(ctx->input_dev, keycode, FALSE);
	input_report_key(ctx->input_dev, KEY_LEFTCTRL, FALSE);
}

static void send_one_key_with_alt(struct kbd_ctx* ctx, uint8_t keycode)
{
	input_report_key(ctx->input_dev, KEY_LEFTALT, TRUE);
	input_report_key(ctx->input_dev, keycode, TRUE);
	input_report_key(ctx->input_dev, keycode, FALSE);
	input_report_key(ctx->input_dev, KEY_LEFTALT, FALSE);
}

static void enable_meta_touch_keys_mode(struct kbd_ctx* ctx)
{
	// Enable touch interrupts on I2C
	if (kbd_write_i2c_u8(ctx->i2c_client, REG_CF2, REG_CF2_TOUCH_INT)) {
		return;
	}

	ctx->meta_touch_keys_mode = 1;
}

static void disable_meta_touch_keys_mode(struct kbd_ctx* ctx)
{
	// Disable touch interrupts on I2C
	if (kbd_write_i2c_u8(ctx->i2c_client, REG_CF2, 0)) {
		return;
	}

	ctx->meta_touch_keys_mode = 0;
}

// Called before checking "repeatable" meta mode keys,
// These keys map to an internal driver function rather than another key
// They will not be sent to the input system
// The check is separate from the run so that key-up events can be ignored
static bool is_single_function_meta_mode_key(struct kbd_ctx* ctx, uint8_t keycode)
{
	switch (keycode) {
	case KEY_Q: return TRUE; // Word left
	case KEY_A: return TRUE; // Word right
	case KEY_X: return TRUE; // Control
	case KEY_C: return TRUE; // Alt
	case KEY_N: return TRUE; // Decrease brightness
	case KEY_M: return TRUE; // Increase brightness
	case KEY_MUTE: return TRUE; // Toggle brightness
	case KEY_COMPOSE: return TRUE; // Turn on touch keys mode
	}

	return FALSE;
}
static void run_single_function_meta_mode_key(struct kbd_ctx* ctx,
	uint8_t keycode)
{
	switch (keycode) {

	case KEY_Q: send_one_key_with_alt(ctx, KEY_LEFT); return;
	case KEY_A: send_one_key_with_alt(ctx, KEY_RIGHT); return;

	case KEY_X:
		ctx->apply_control = 1;
		ctx->meta_mode = 0;
		disable_meta_touch_keys_mode(ctx);
		return;

	case KEY_C:
		ctx->apply_alt = 1;
		ctx->meta_mode = 0;
		disable_meta_touch_keys_mode(ctx);
		return;

	case KEY_COMPOSE:
		// First click of Compose enters meta mode (already here)
		// Second click of Compose enters touch keys mode.
		// Subsequent clicks are Enter.
		if (ctx->meta_touch_keys_mode) {
			input_report_key(ctx->input_dev, KEY_ENTER, 1);
			input_report_key(ctx->input_dev, KEY_ENTER, 0);

		} else {
			enable_meta_touch_keys_mode(ctx);
		}
		return;

	case KEY_N: kbd_decrease_brightness(ctx); return;
	case KEY_M: kbd_increase_brightness(ctx); return;
	case KEY_MUTE:
		kbd_toggle_brightness(ctx);
		ctx->meta_mode = 0;
		return;
	}
}

// Called after checking "single function" meta mode keys,
// These keys, both press and release events, will be sent to the input system
static uint8_t map_repeatable_meta_mode_key(struct kbd_ctx* ctx, uint8_t keycode)
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

	case KEY_T:
		ctx->meta_mode = 0;
		disable_meta_touch_keys_mode(ctx);
		return KEY_TAB;

	case KEY_ESC:
		ctx->meta_mode = 0;
		disable_meta_touch_keys_mode(ctx);
		return 0;
	}

	// No meta mode match, disable and return original key
	ctx->meta_mode = 0;
	disable_meta_touch_keys_mode(ctx);
	return keycode;
}

// Main key event handler 
static void report_key_input_event(struct kbd_ctx* ctx,
	struct fifo_item const* ev)
{
	uint8_t keycode, remapped_keycode;

	// Only handle key pressed or released events
	if ((ev->state != KEY_PRESSED_STATE) && (ev->state != KEY_RELEASED_STATE)) {
		return;
	}

	// Post key scan event
	input_event(ctx->input_dev, EV_MSC, MSC_SCAN, ev->scancode);

	// Map input scancode to Linux input keycode
	keycode = ctx->keycode_map[ev->scancode];
	dev_info_fe(&ctx->i2c_client->dev,
		"%s state %d, scancode %d mapped to keycode %d\n",
		__func__, ev->state, ev->scancode, keycode);

	// Scancode mapped to ignored keycode
	if (keycode == 0) {
		return;

	// Scancode converted to keycode not in map
	} else if (keycode == KEY_UNKNOWN) {
		dev_warn(&ctx->i2c_client->dev,
			"%s Could not get Keycode for Scancode: [0x%02X]\n",
			__func__, ev->scancode);
		return;

	// Compose key sends enter if touchpad always sends arrow keys
	} else if (ctx->touchpad_always_keys && (keycode == KEY_COMPOSE)) {
		keycode = KEY_ENTER;
		// Continue to normal input handling

	// Compose key enters meta mode if touchpad not in arrow key mode
	} else if (!ctx->meta_mode && (keycode == KEY_COMPOSE)) {
		if (ev->state == KEY_RELEASED_STATE) {
			ctx->meta_mode = 1;
		}
		return;

	// Berry key sends Tmux prefix (Control + code 162 in keymap)
	} else if (keycode == KEY_PROPS) {
		if (ev->state == KEY_PRESSED_STATE) {
			send_one_key_with_control(ctx, 162);
		}
		return;
	}

	// Handle keys without modifiers in meta mode
	if (!ctx->touchpad_always_keys && ctx->meta_mode) {

		// Handle function dispatch meta mode keys
		if (is_single_function_meta_mode_key(ctx, keycode)) {
			if (ev->state == KEY_RELEASED_STATE) {
		 		run_single_function_meta_mode_key(ctx, keycode);
			}
			return;
		}

		// Remap to meta mode key
		remapped_keycode = map_repeatable_meta_mode_key(ctx, keycode);

		// Input consumed by meta mode with no remmaped key
		if (remapped_keycode == 0) {
			return;
		}

		// Report key to input system
		input_report_key(ctx->input_dev, remapped_keycode, ev->state == KEY_PRESSED_STATE);

		// If exited meta mode, simulate key up event. Otherwise, input system
		// will have remapped key as in the down state
		if (!ctx->meta_mode && (remapped_keycode != keycode)) {
			input_report_key(ctx->input_dev, remapped_keycode, FALSE);
		}

	// Apply modifier keys
	} else {
		if (ev->shift_modifier) { input_report_key(ctx->input_dev, KEY_LEFTSHIFT, 1); }
		if (ev->ctrl_modifier || ctx->apply_control) {
			input_report_key(ctx->input_dev,  KEY_LEFTCTRL, 1);
		}
		if (ctx->apply_alt) { input_report_key(ctx->input_dev, KEY_LEFTALT, 1); }
		if (ev->altgr_modifier) { input_report_key(ctx->input_dev, KEY_RIGHTALT, 1); }

		// Report key to input system
		input_report_key(ctx->input_dev, keycode, ev->state == KEY_PRESSED_STATE);

		// Release modifier keys
		if (ev->shift_modifier) { input_report_key(ctx->input_dev, KEY_LEFTSHIFT, 0); }
		if (ev->ctrl_modifier || ctx->apply_control) {
			input_report_key(ctx->input_dev,  KEY_LEFTCTRL, 0);
			ctx->apply_control = 0;
		}
		if (ctx->apply_alt) {
			input_report_key(ctx->input_dev, KEY_LEFTALT, 0);
			ctx->apply_alt = 0;
		}
		if (ev->altgr_modifier) { input_report_key(ctx->input_dev,  KEY_RIGHTALT, 0); }
	}
}

int input_probe(struct i2c_client* i2c_client)
{
	int rc, i;
	uint8_t reg_value;

	// Allocate keyboard context (managed by device lifetime)
	g_ctx = devm_kzalloc(&i2c_client->dev, sizeof(*g_ctx), GFP_KERNEL);
	if (!g_ctx) {
		return -ENOMEM;
	}

	// Allocate and copy keycode array
	g_ctx->keycode_map = devm_kzalloc(&i2c_client->dev, sizeof(keycodes),
		GFP_KERNEL);
	memcpy(g_ctx->keycode_map, keycodes, ARRAY_SIZE(keycodes));

	// Initialize keyboard context
	g_ctx->i2c_client = i2c_client;
	g_ctx->meta_mode = 0;
	g_ctx->meta_touch_keys_mode = 0;
	g_ctx->apply_control = 0;
	g_ctx->apply_alt = 0;
	g_ctx->brightness = 0xFF;
	g_ctx->last_brightness = 0xFF;

	// Get firmware version
	if (kbd_read_i2c_u8(i2c_client, REG_VER, &g_ctx->version_number)) {
		return -ENODEV;
	}
	dev_info(&i2c_client->dev,
		"%s BBQX0KBD indev Software version: 0x%02X\n", __func__,
		g_ctx->version_number);

	// Write configuration 1
	if (kbd_write_i2c_u8(i2c_client, REG_CFG, REG_CFG_DEFAULT_SETTING)) {
		return -ENODEV;
	}

	// Read back configuration 1 setting
	if (kbd_read_i2c_u8(i2c_client, REG_CFG, &reg_value)) {
		return -ENODEV;
	}
	dev_info_ld(&i2c_client->dev,
		"%s Configuration Register Value: 0x%02X\n", __func__, reg_value);

	// Read back configuration 2 setting
	if (kbd_read_i2c_u8(i2c_client, REG_CF2, &reg_value)) {
		return rc;
	}
	dev_info_ld(&i2c_client->dev,
		"%s Configuration 2 Register Value: 0x%02X\n",
		__func__, reg_value);

	// Update keyboard brightness
	(void)kbd_write_i2c_u8(i2c_client, REG_BKL, g_ctx->brightness);

	// Allocate input device
	if ((g_ctx->input_dev = devm_input_allocate_device(&i2c_client->dev)) == NULL) {
		dev_err(&i2c_client->dev,
			"%s Could not devm_input_allocate_device BBQX0KBD.\n", __func__);
		return -ENOMEM;
	}

	// Initialize input device
	g_ctx->input_dev->name = i2c_client->name;
	g_ctx->input_dev->id.bustype = BBQX0KBD_BUS_TYPE;
	g_ctx->input_dev->id.vendor  = BBQX0KBD_VENDOR_ID;
	g_ctx->input_dev->id.product = BBQX0KBD_PRODUCT_ID;
	g_ctx->input_dev->id.version = BBQX0KBD_VERSION_ID;

	// Initialize input device keycodes
	g_ctx->input_dev->keycode = g_ctx->keycode_map;
	g_ctx->input_dev->keycodesize = sizeof(g_ctx->keycode_map[0]);
	g_ctx->input_dev->keycodemax = ARRAY_SIZE(keycodes);

	// Set input device keycode bits
	for (i = 0; i < NUM_KEYCODES; i++) {
		__set_bit(g_ctx->keycode_map[i], g_ctx->input_dev->keybit);
	}
	__clear_bit(KEY_RESERVED, g_ctx->input_dev->keybit);
	__set_bit(EV_REP, g_ctx->input_dev->evbit);
	__set_bit(EV_KEY, g_ctx->input_dev->evbit);

	// Set input device capabilities
	input_set_capability(g_ctx->input_dev, EV_MSC, MSC_SCAN);
	#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)
		input_set_capability(g_ctx->input_dev, EV_REL, REL_X);
		input_set_capability(g_ctx->input_dev, EV_REL, REL_Y);
	#endif

	// Request IRQ handler for I2C client and initialize workqueue
	if ((rc = devm_request_threaded_irq(&i2c_client->dev,
		i2c_client->irq, NULL, input_irq_handler, IRQF_SHARED | IRQF_ONESHOT,
		i2c_client->name, g_ctx))) {

		dev_err(&i2c_client->dev,
			"Could not claim IRQ %d; error %d\n", i2c_client->irq, rc);
		return rc;
	}
	INIT_WORK(&g_ctx->work_struct, input_workqueue_handler);

	// Register input device with input subsystem
	dev_info(&i2c_client->dev,
		"%s registering input device", __func__);
	if ((rc = input_register_device(g_ctx->input_dev))) {
		dev_err(&i2c_client->dev,
			"Failed to register input device, error: %d\n", rc);
		return rc;
	}

	return 0;
}

void input_shutdown(struct i2c_client* i2c_client)
{
	uint8_t reg_value;

	dev_info_fe(&i2c_client->dev,
		"%s Shutting Down Keyboard And Screen Backlight.\n", __func__);
	
	// Turn off backlight
	(void)kbd_write_i2c_u8(i2c_client, REG_BKL, 0);

	// Reenable touch events
	(void)kbd_write_i2c_u8(i2c_client, REG_CF2, REG_CFG2_DEFAULT_SETTING);

	// Read back version
	(void)kbd_read_i2c_u8(i2c_client, REG_VER, &reg_value);

	// Remove context from global state
	// (It is freed by the device-specific memory mananger)
	g_ctx = NULL;
}

irqreturn_t input_irq_handler(int irq, void *param)
{
	struct kbd_ctx *ctx;
	uint8_t irq_type;

	// `param` is current keyboard context as started in _probe
	ctx = (struct kbd_ctx *)param;

	dev_info_ld(&ctx->i2c_client->dev,
		"%s Interrupt Fired. IRQ: %d\n", __func__, irq);

	// Read interrupt type from client
	if (kbd_read_i2c_u8(ctx->i2c_client, REG_INT, &irq_type)) {
		return IRQ_NONE;
	}
	dev_info_ld(&ctx->i2c_client->dev,
		"%s Interrupt type: 0x%02x\n", __func__, irq_type);

	// Reported no interrupt type
	if (irq_type == 0x00) {
		return IRQ_NONE;
	}

	// Client reported a key overflow
	if (irq_type & REG_INT_OVERFLOW) {
		dev_warn(&ctx->i2c_client->dev, "%s overflow occurred.\n", __func__);
	}

	// Client reported a key event
	if (irq_type & REG_INT_KEY) {
		kbd_read_fifo(ctx);
		schedule_work(&ctx->work_struct);
	}

	// Client reported a touch event
	if (irq_type & REG_INT_TOUCH) {

		// Read touch X-coordinate
		if (kbd_read_i2c_u8(ctx->i2c_client, REG_TOX, &ctx->touch_rel_x)) {
			return IRQ_NONE;
		}

		// Read touch Y-coordinate
		if (kbd_read_i2c_u8(ctx->i2c_client, REG_TOY, &ctx->touch_rel_y)) {
			return IRQ_NONE;
		}

		// Set touch event flag and schedule touch work
		ctx->touch_event_flag = 1;
		schedule_work(&ctx->work_struct);

	} else {

		// Clear touch event flag
		ctx->touch_event_flag = 0;
	}

	return IRQ_HANDLED;
}

void input_workqueue_handler(struct work_struct *work_struct_ptr)
{
	struct kbd_ctx *ctx;
	uint8_t fifo_idx;

	// Get keyboard context from work struct
	ctx = container_of(work_struct_ptr, struct kbd_ctx, work_struct);

	// Process FIFO items
	for (fifo_idx = 0; fifo_idx < ctx->fifo_count; fifo_idx++) {
		report_key_input_event(ctx, &ctx->fifo_data[fifo_idx]);
	}

	// Reset pending FIFO count
	ctx->fifo_count = 0;

	// Handle touch interrupt flag
	if (ctx->touch_event_flag) {

		dev_info_ld(&ctx->i2c_client->dev,
			"%s X Reg: %d Y Reg: %d.\n",
			__func__, ctx->touch_rel_x, ctx->touch_rel_y);

		#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)

			// Report mouse movement
			input_report_rel(input_dev, REL_X, ctx->touch_rel_x);
			input_report_rel(input_dev, REL_Y, ctx->touch_rel_y);

			// Clear touch interrupt flag
			ctx->touch_event_flag = 0;
		#endif

		#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_KEYS)

		// Touchpad-as-keys mode will always use the touchpad as arrow keys
		if (ctx->touchpad_always_keys) {

			// Negative X: left arrow key
			if (ctx->touch_rel_x < -4) {
				input_report_key(ctx->input_dev, KEY_LEFT, TRUE);
				input_report_key(ctx->input_dev, KEY_LEFT, FALSE);

			// Positive X: right arrow key
			} else if (ctx->touch_rel_x > 4) {
				input_report_key(ctx->input_dev, KEY_RIGHT, TRUE);
				input_report_key(ctx->input_dev, KEY_RIGHT, FALSE);
			}

			// Negative Y: up arrow key
			if (ctx->touch_rel_y < -4) {
				input_report_key(ctx->input_dev, KEY_UP, TRUE);
				input_report_key(ctx->input_dev, KEY_UP, FALSE);

			// Positive Y: down arrow key
			} else if (ctx->touch_rel_y > 4) {
				input_report_key(ctx->input_dev, KEY_DOWN, TRUE);
				input_report_key(ctx->input_dev, KEY_DOWN, FALSE);
			}

		// Meta touch keys will only send up/down keys when enabled
		} else if (ctx->meta_touch_keys_mode) {

			// Negative Y: up arrow key
			if (ctx->touch_rel_y < -4) {
				input_report_key(ctx->input_dev, KEY_UP, TRUE);
				input_report_key(ctx->input_dev, KEY_UP, FALSE);

			// Positive Y: down arrow key
			} else if (ctx->touch_rel_y > 4) {
				input_report_key(ctx->input_dev, KEY_DOWN, TRUE);
				input_report_key(ctx->input_dev, KEY_DOWN, FALSE);
			}
		}

		// Clear touch interrupt flag
		ctx->touch_event_flag = 0;
		#endif
	}

	// Synchronize input system and clear client interrupt flag
	input_sync(ctx->input_dev);
	if (kbd_write_i2c_u8(ctx->i2c_client, REG_INT, 0)) {
		return;
	}
}

