// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * bbqX0kbd_main.c: Main C File.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>

#include "config.h"
#include "bbqX0kbd_i2cHelper.h"
#include "bbqX0kbd_registers.h"
#include "debug_levels.h"
#include "bbq20kbd_pmod_codes.h"

#define BBQX0KBD_BUS_TYPE		BUS_I2C
#define BBQX0KBD_VENDOR_ID		0x0001
#define BBQX0KBD_PRODUCT_ID		0x0001
#define BBQX0KBD_VERSION_ID		0x0001

#if (BBQX0KBD_INT != BBQX0KBD_USE_INT)
#error "Only supporting interrupts mode right now"
#endif

#if (BBQX0KBD_TYPE != BBQ20KBD_PMOD)
#error "Only supporting BBQ20 keyboard right now"
#endif

#if (DEBUG_LEVEL & DEBUG_LEVEL_FE)
#define dev_info_fe(...) dev_info(__VA_ARGS__)
#else
#define dev_info_fe(...)
#endif

#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
#define dev_info_ld(...) dev_info(__VA_ARGS__)
#else
#define dev_info_ld(...)
#endif

// From keyboard firmware source
struct fifo_item
{
	uint8_t scancode;

	uint8_t _ : 1;
	uint8_t shift_modifier : 1;
	uint8_t ctrl_modifier : 1;
	uint8_t altgr_modifier : 1;
	enum key_state state : 4;
};

// It's just hard to keep typing bbqX0kbd_data...
struct kbd_ctx
{
	struct work_struct work_struct;
	uint8_t version_number;

	// Map from input HID scancodes to Linux keycodes
	unsigned short keycode_map[NUM_KEYCODES];

	// Key state FIFO queue
	uint8_t fifo_count;
	struct fifo_item fifo_data[BBQX0KBD_FIFO_SIZE];

	// Touch and mouse flags
	uint8_t touch_event_flag;
	int8_t touch_rel_x;
	int8_t touch_rel_y;

	// Meta mode enabled flag
	uint8_t meta_mode;
	uint8_t touch_keys_mode;

	// Apply modifiers to the next alpha keypress
	uint8_t apply_control;
	uint8_t apply_alt;

	// Keyboard brightness
	uint8_t brightness;
	uint8_t last_brightness;

	struct i2c_client *i2c_client;
	struct input_dev *input_dev;
};

// Helper functions

// Read a single uint8_t value from I2C register
static int kbd_read_i2c_u8(struct i2c_client* i2c_client, uint8_t reg_addr, uint8_t* dst)
{
	return bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, reg_addr, dst, sizeof(uint8_t));
}

// Write a single uint8_t value to I2C register
static int kbd_write_i2c_u8(struct i2c_client* i2c_client, uint8_t reg_addr, uint8_t src)
{
	return bbqX0kbd_write(i2c_client, BBQX0KBD_I2C_ADDRESS, reg_addr, &src,
		sizeof(uint8_t));
}

// Read two uint8_t values from a 16-bit I2C register
static int kbd_read_i2c_2u8(struct i2c_client* i2c_client, uint8_t reg_addr, uint8_t* dst)
{
	return bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, reg_addr, dst,
		2 * sizeof(uint8_t));
}

// Helper to read a single uint8_t value and handle error reporting
#define READ_I2C_U8_OR_DEV_ERR(rc, client, reg, dst, on_err) \
	if ((rc = kbd_read_i2c_u8(client, reg, dst)) < 0) { \
		dev_err(&client->dev, "%s: Could not read " #reg ". Error: %d\n", __func__, rc); \
		on_err; \
	}

// Helper to write a single uint8_t value and handle error reporting
#define WRITE_I2C_U8_OR_DEV_ERR(rc, client, reg, src, on_err) \
	if ((rc = kbd_write_i2c_u8(client, reg, src)) < 0) { \
		dev_err(&client->dev, "%s: Could not write " #reg ". Error: %d\n", __func__, rc); \
		on_err; \
	}

static void bbqX0kbd_set_brightness(struct kbd_ctx* ctx,
	unsigned short keycode, uint8_t* should_report_key)
{
	if (keycode == KEY_Z) {
		*should_report_key = 0;

		// Increase by delta, max at 0xff brightness
		ctx->brightness
			= (ctx->brightness > (0xff - BBQ10_BRIGHTNESS_DELTA))
				? 0xff
				: ctx->brightness + BBQ10_BRIGHTNESS_DELTA;

	} else if (keycode == KEY_X) {
		*should_report_key = 0;

		// Decrease by delta, min at 0x0 brightness
		ctx->brightness
			= (ctx->brightness < BBQ10_BRIGHTNESS_DELTA)
				? 0x0
				: ctx->brightness - BBQ10_BRIGHTNESS_DELTA;

	} else if (keycode == KEY_0) {
		*should_report_key = 0;

		// Toggle off, save last brightness in context
		ctx->last_brightness = ctx->brightness;
		ctx->brightness = 0;

	// Not a brightness control key, don't consume it
	} else {
		*should_report_key = 2;
	}

	// If it was a brightness control key, set backlight
	if (*should_report_key == 0) {
		(void)kbd_write_i2c_u8(ctx->i2c_client, REG_BKL, ctx->brightness);
	}
}

// Transfer from I2C FIFO to internal context FIFO
static void bbqX0kbd_read_fifo(struct kbd_ctx* ctx)
{
	uint8_t fifo_idx;
	int rc;

	// Read number of FIFO items
	READ_I2C_U8_OR_DEV_ERR(rc, ctx->i2c_client, REG_KEY, &ctx->fifo_count, return);
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

// Called before checking "repeatable" meta mode keys,
// These keys map to an internal driver function rather than another key
// They will not be sent to the input system
// The check is separate from the run so that key-up events can be ignored
static bool is_single_function_meta_mode_key(struct kbd_ctx* ctx, uint8_t keycode)
{
	switch (keycode) {
	case KEY_Q: return TRUE;
	case KEY_A: return TRUE;
	case KEY_X: return TRUE;
	case KEY_C: return TRUE;
	case KEY_COMPOSE: return TRUE;
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
		ctx->touch_keys_mode = 0;
		return;

	case KEY_C:
		ctx->apply_alt = 1;
		ctx->meta_mode = 0;
		ctx->touch_keys_mode = 0;
		return;

	case KEY_COMPOSE:
		// First click of Compose enters meta mode (already here)
		// Second click of Compose enters touch keys mode.
		// Subsequent clicks are Enter.
		if (ctx->touch_keys_mode) {
			input_report_key(ctx->input_dev, KEY_ENTER, 1);
			input_report_key(ctx->input_dev, KEY_ENTER, 0);

		} else {
			ctx->touch_keys_mode = 1;
		}
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
		ctx->touch_keys_mode = 0;
		return KEY_TAB;

	case KEY_ESC:
		ctx->meta_mode = 0;
		ctx->touch_keys_mode = 0;
		return 0;
	}

	// No meta mode match, disable and return original key
	ctx->meta_mode = 0;
	ctx->touch_keys_mode = 0;
	return keycode;
}

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

	// Compose key enters meta mode
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
	if (ctx->meta_mode) {

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

static void bbqX0kbd_work_fnc(struct work_struct *work_struct_ptr)
{
	struct kbd_ctx *ctx;
	uint8_t fifo_idx;
	int rc;

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
		if (ctx->touch_keys_mode) {

			#if 0
			// Negative X: left arrow key
			if (ctx->touch_rel_x < -4) {
				input_report_key(ctx->input_dev, KEY_LEFT, TRUE);
				input_report_key(ctx->input_dev, KEY_LEFT, FALSE);

			// Positive X: right arrow key
			} else if (ctx->touch_rel_x > 4) {
				input_report_key(ctx->input_dev, KEY_RIGHT, TRUE);
				input_report_key(ctx->input_dev, KEY_RIGHT, FALSE);
			}
			#endif

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
	WRITE_I2C_U8_OR_DEV_ERR(rc, ctx->i2c_client, REG_INT, 0, return);
}

static irqreturn_t bbqX0kbd_irq_handler(int irq, void *param)
{
	struct kbd_ctx *ctx;
	int rc;
	uint8_t irq_type;

	// `param` is current keyboard context as started in _probe
	ctx = (struct kbd_ctx *)param;

	dev_info_ld(&ctx->i2c_client->dev,
		"%s Interrupt Fired. IRQ: %d\n", __func__, irq);

	// Read interrupt type from client
	READ_I2C_U8_OR_DEV_ERR(rc, ctx->i2c_client, REG_INT, &irq_type, return IRQ_NONE);
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
		bbqX0kbd_read_fifo(ctx);
		schedule_work(&ctx->work_struct);
	}

	// Client reported a touch event
	if (irq_type & REG_INT_TOUCH) {

		// Read touch X-coordinate
		READ_I2C_U8_OR_DEV_ERR(rc, ctx->i2c_client, REG_TOX,
			&ctx->touch_rel_x, return IRQ_NONE);

		// Read touch Y-coordinate
		READ_I2C_U8_OR_DEV_ERR(rc, ctx->i2c_client, REG_TOY,
			&ctx->touch_rel_y, return IRQ_NONE);

		// Set touch event flag and schedule touch work
		ctx->touch_event_flag = 1;
		schedule_work(&ctx->work_struct);

	} else {

		// Clear touch event flag
		ctx->touch_event_flag = 0;
	}

	return IRQ_HANDLED;
}

static int bbqX0kbd_probe(struct i2c_client* i2c_client, struct i2c_device_id const* i2c_id)
{
	struct kbd_ctx *ctx;
	int rc, i;
	uint8_t reg_value;

	dev_info_fe(&i2c_client->dev,
		"%s Probing BBQX0KBD.\n", __func__);

	// Allocate keyboard context (managed by device lifetime)
	ctx = devm_kzalloc(&i2c_client->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		return -ENOMEM;
	}

	// Initialize keyboard context
	ctx->i2c_client = i2c_client;
	memcpy(ctx->keycode_map, keycodes, sizeof(ctx->keycode_map));
	ctx->meta_mode = 0;
	ctx->touch_keys_mode = 0;
	ctx->apply_control = 0;
	ctx->apply_alt = 0;
	ctx->brightness = 0xFF;
	ctx->last_brightness = 0xFF;

	// Get firmware version
	READ_I2C_U8_OR_DEV_ERR(rc, i2c_client, REG_VER, &ctx->version_number,
		return -ENODEV);
	dev_info(&i2c_client->dev,
		"%s BBQX0KBD indev Software version: 0x%02X\n", __func__, ctx->version_number);

	// Write configuration 1
	WRITE_I2C_U8_OR_DEV_ERR(rc, i2c_client, REG_CFG, REG_CFG_DEFAULT_SETTING,
		return -ENODEV);

	// Read back configuration 1 setting
	READ_I2C_U8_OR_DEV_ERR(rc, i2c_client, REG_CFG, &reg_value, return rc);
	dev_info_ld(&i2c_client->dev,
		"%s Configuration Register Value: 0x%02X\n", __func__, reg_value);

	// Write configuration 2
	WRITE_I2C_U8_OR_DEV_ERR(rc, i2c_client, REG_CF2, REG_CFG2_DEFAULT_SETTING,
		return -ENODEV);

	// Read back configuration 2 setting
	READ_I2C_U8_OR_DEV_ERR(rc, i2c_client, REG_CF2, &reg_value, return rc);
	dev_info_ld(&i2c_client->dev,
		"%s Configuration 2 Register Value: 0x%02X\n",
		__func__, reg_value);

	// Update keyboard brightness
	(void)kbd_write_i2c_u8(i2c_client, REG_BKL, ctx->brightness);

	// Allocate input device
	if ((ctx->input_dev = devm_input_allocate_device(&i2c_client->dev)) == NULL) {
		dev_err(&i2c_client->dev,
			"%s Could not devm_input_allocate_device BBQX0KBD.\n", __func__);
		return -ENOMEM;
	}

	// Initialize input device
	ctx->input_dev->name = i2c_client->name;
	ctx->input_dev->id.bustype = BBQX0KBD_BUS_TYPE;
	ctx->input_dev->id.vendor  = BBQX0KBD_VENDOR_ID;
	ctx->input_dev->id.product = BBQX0KBD_PRODUCT_ID;
	ctx->input_dev->id.version = BBQX0KBD_VERSION_ID;

	// Initialize input device keycodes
	ctx->input_dev->keycode = ctx->keycode_map;
	ctx->input_dev->keycodesize = sizeof(ctx->keycode_map[0]);
	ctx->input_dev->keycodemax = ARRAY_SIZE(ctx->keycode_map);

	// Set input device keycode bits
	for (i = 0; i < NUM_KEYCODES; i++) {
		__set_bit(ctx->keycode_map[i], ctx->input_dev->keybit);
	}
	__clear_bit(KEY_RESERVED, ctx->input_dev->keybit);
	__set_bit(EV_REP, ctx->input_dev->evbit);
	__set_bit(EV_KEY, ctx->input_dev->evbit);

	// Set input device capabilities
	input_set_capability(ctx->input_dev, EV_MSC, MSC_SCAN);
	#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)
		input_set_capability(ctx->input_dev, EV_REL, REL_X);
		input_set_capability(ctx->input_dev, EV_REL, REL_Y);
	#endif

	// Request IRQ handler for I2C client and initialize workqueue
	if ((rc = devm_request_threaded_irq(&i2c_client->dev,
		i2c_client->irq, NULL, bbqX0kbd_irq_handler, IRQF_SHARED | IRQF_ONESHOT,
		i2c_client->name, ctx))) {

		dev_err(&i2c_client->dev,
			"Could not claim IRQ %d; error %d\n", i2c_client->irq, rc);
		return rc;
	}
	INIT_WORK(&ctx->work_struct, bbqX0kbd_work_fnc);

	// Register input device with input subsystem
	dev_info(&i2c_client->dev,
		"%s registering input device", __func__);
	if ((rc = input_register_device(ctx->input_dev))) {
		dev_err(&i2c_client->dev,
			"Failed to register input device, error: %d\n", rc);
		return rc;
	}

	return 0;
}

static void bbqX0kbd_shutdown(struct i2c_client* i2c_client)
{
	int rc;
	uint8_t reg_value;

	dev_info_fe(&i2c_client->dev,
		"%s Shutting Down Keyboard And Screen Backlight.\n", __func__);
	
	// Turn off backlight
	WRITE_I2C_U8_OR_DEV_ERR(rc, i2c_client, REG_BKL, 0, return);

	// Read back version
	READ_I2C_U8_OR_DEV_ERR(rc, i2c_client, REG_VER, &reg_value, return);
}

static void bbqX0kbd_remove(struct i2c_client* i2c_client)
{
	dev_info_fe(&i2c_client->dev,
		"%s Removing BBQX0KBD.\n", __func__);

	bbqX0kbd_shutdown(i2c_client);
}

// Driver definitions

// Device IDs
static const struct i2c_device_id bbqX0kbd_i2c_device_id[] = {
	{ "bbqX0kbd", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bbqX0kbd_i2c_device_id);
static const struct of_device_id bbqX0kbd_of_device_id[] = {
	{ .compatible = "wallComputer,bbqX0kbd", },
	{ }
};
MODULE_DEVICE_TABLE(of, bbqX0kbd_of_device_id);

// Callbacks
static struct i2c_driver bbqX0kbd_driver = {
	.driver = {
		.name = "bbqX0kbd",
		.of_match_table = bbqX0kbd_of_device_id,
	},
	.probe    = bbqX0kbd_probe,
	.shutdown = bbqX0kbd_shutdown,
	.remove   = bbqX0kbd_remove,
	.id_table = bbqX0kbd_i2c_device_id,
};

// Module constructor and destructor
static int __init bbqX0kbd_init(void)
{
	int rc;

	// Adding the I2C driver will call the _probe function to continue setup
	if ((rc = i2c_add_driver(&bbqX0kbd_driver))) {
		pr_err("%s Could not initialise BBQX0KBD driver! Error: %d\n", __func__, rc);
		return rc;
	}
	pr_info("%s Initalised BBQX0KBD.\n", __func__);

	return rc;
}
module_init(bbqX0kbd_init);

static void __exit bbqX0kbd_exit(void)
{
	pr_info("%s Exiting BBQX0KBD.\n", __func__);
	i2c_del_driver(&bbqX0kbd_driver);
}
module_exit(bbqX0kbd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wallComputer and Andrew D'Angelo <dangeloandrew@outlook.com>");
MODULE_DESCRIPTION("BB Classic keyboard driver for Beepberry");
MODULE_VERSION("0.5");