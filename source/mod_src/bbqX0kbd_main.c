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

#define CTRL_BIT  BIT(0)
#define SHIFT_BIT BIT(1)
#define ALT_BIT   BIT(2)
#define META_BIT  BIT(3)

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

struct bbqX0kbd_data
{
	struct work_struct work_struct;
	uint8_t version_number;

	// Map from input HID scancodes to Linux keycodes
	unsigned short keycode_map[NUM_KEYCODES];

	// Key state FIFO queue
	uint8_t fifo_count;
	uint8_t fifo_data[BBQX0KBD_FIFO_SIZE][2];

	// Touch and mouse flags
	uint8_t touch_event_flag;
	int8_t touch_rel_x;
	int8_t touch_rel_y;

	// Modifier mode flags and lock status
	uint8_t held_modifier_keys;
	uint8_t pending_sticky_modifier_keys;
	uint8_t sticky_modifier_keys;

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

// Sticky keys will keep meta mode on after being pressed
static unsigned short bbqX0kbd_get_meta_keycode(uint8_t *sticky, unsigned short keycode)
{
	switch (keycode) {
	case KEY_E: *sticky = 1; return KEY_UP;
	case KEY_S: *sticky = 1; return KEY_DOWN;
	case KEY_W: *sticky = 1; return KEY_LEFT;
	case KEY_D: *sticky = 1; return KEY_RIGHT;
	case KEY_R: *sticky = 1; return KEY_HOME;
	case KEY_F: *sticky = 1; return KEY_END;
	case KEY_T: *sticky = 0; return KEY_TAB;
	case KEY_O: *sticky = 1; return KEY_PAGEUP;
	case KEY_P: *sticky = 1; return KEY_PAGEDOWN;
	case KEY_X: *sticky = 0; return KEY_LEFTCTRL;

	// Default: turn meta mode off
	default: *sticky = 0; return keycode;
	}
}

static void bbqX0kbd_set_brightness(struct bbqX0kbd_data* kbd_ctx,
	unsigned short keycode, uint8_t* should_report_key)
{
	if (keycode == KEY_Z) {
		*should_report_key = 0;

		// Increase by delta, max at 0xff brightness
		kbd_ctx->brightness
			= (kbd_ctx->brightness > (0xff - BBQ10_BRIGHTNESS_DELTA))
				? 0xff
				: kbd_ctx->brightness + BBQ10_BRIGHTNESS_DELTA;

	} else if (keycode == KEY_X) {
		*should_report_key = 0;

		// Decrease by delta, min at 0x0 brightness
		kbd_ctx->brightness
			= (kbd_ctx->brightness < BBQ10_BRIGHTNESS_DELTA)
				? 0x0
				: kbd_ctx->brightness - BBQ10_BRIGHTNESS_DELTA;

	} else if (keycode == KEY_0) {
		*should_report_key = 0;

		// Toggle off, save last brightness in context
		kbd_ctx->last_brightness = kbd_ctx->brightness;
		kbd_ctx->brightness = 0;

	// Not a brightness control key, don't consume it
	} else {
		*should_report_key = 2;
	}

	// If it was a brightness control key, set backlight
	if (*should_report_key == 0) {
		(void)kbd_write_i2c_u8(kbd_ctx->i2c_client, REG_BKL, kbd_ctx->brightness);
	}
}

// Transfer from I2C FIFO to internal context FIFO
static void bbqX0kbd_read_fifo(struct bbqX0kbd_data* kbd_ctx)
{
	uint8_t fifo_idx;
	int rc;

	// Read number of FIFO items
	READ_I2C_U8_OR_DEV_ERR(rc, kbd_ctx->i2c_client, REG_KEY, &kbd_ctx->fifo_count, return);
	kbd_ctx->fifo_count &= REG_KEY_KEYCOUNT_MASK;

	// Read and transfer all FIFO items
	for (fifo_idx = 0; fifo_idx < kbd_ctx->fifo_count; fifo_idx++) {

		// Read 2 fifo items
		if ((rc = kbd_read_i2c_2u8(kbd_ctx->i2c_client, REG_FIF,
			kbd_ctx->fifo_data[fifo_idx]))) {

			dev_err(&kbd_ctx->i2c_client->dev,
				"%s Could not read REG_FIF, Error: %d\n", __func__, rc);
			return;
		}

		// Advance FIFO position
		dev_info_ld(&kbd_ctx->i2c_client->dev,
			"%s Filled Data: KeyState:%d SCANCODE:%d at Pos: %d Count: %d\n",
			__func__, kbd_ctx->fifo_data[fifo_idx][0], kbd_ctx->fifo_data[fifo_idx][1],
			fifo_idx, fifo_count);
	}
}

// Sticky modifier keys follow BB Q10 convention
// Holding modifier while typing alpha keys will apply to all alpha keys
// until released.
// One press and release will enter sticky mode, apply modifier key to
// the next alpha key only. If the same modifier key is pressed and
// released again in sticky mode, it will be canceled.
static void transition_sticky_modifier_key(struct bbqX0kbd_data* kbd_ctx,
	uint8_t key_state, uint8_t key_bit, int keycode)
{
	if (key_state == KEY_PRESSED_STATE) {

		// Set "held" state and "pending sticky" state. Clear "sticky".
		kbd_ctx->held_modifier_keys |= key_bit;

		// If pressed again while sticky, clear sticky
		if (kbd_ctx->sticky_modifier_keys & key_bit) {
			kbd_ctx->sticky_modifier_keys &= ~key_bit;

		// Otherwise, set pending sticky to be applied on release
		} else {
			kbd_ctx->pending_sticky_modifier_keys |= key_bit;
		}

		// Report modifier to input system as held
		input_report_key(kbd_ctx->input_dev, keycode, TRUE);

	// Released
	} else {

		// Unset "held" state
		kbd_ctx->held_modifier_keys &= ~key_bit;

		// If any alpha key was typed during hold,
		// `apply_sticky_modifiers` will clear "pending sticky" state.
		// If still in "pending sticky", set "sticky" state.
		if (kbd_ctx->pending_sticky_modifier_keys & key_bit) {
			kbd_ctx->sticky_modifier_keys |= key_bit;
			kbd_ctx->pending_sticky_modifier_keys &= ~key_bit;
		}

		// Report modifier to input system as released
		input_report_key(kbd_ctx->input_dev, keycode, FALSE);
	}
}

// This function is called before sending the alpha key to apply
// any pending sticky modifiers
static void apply_sticky_modifier_keys(struct bbqX0kbd_data* kbd_ctx,
	uint8_t key_state, uint8_t key_bit, int keycode)
{
	if (kbd_ctx->held_modifier_keys & key_bit) {
		kbd_ctx->pending_sticky_modifier_keys &= ~key_bit;
	} else if (kbd_ctx->sticky_modifier_keys & key_bit) {

		// Modifier applied by reporting key down to input system
		input_report_key(kbd_ctx->input_dev, keycode, TRUE);
	}
}

// This function is called after sending the alpha key to reset
// any sticky modifiers
static void reset_sticky_modifier_keys(struct bbqX0kbd_data* kbd_ctx,
	uint8_t key_state, uint8_t key_bit, int keycode)
{
	if (kbd_ctx->sticky_modifier_keys & key_bit) {
		kbd_ctx->sticky_modifier_keys &= ~key_bit;

		// Modifier reset by reporting key up to input system
		input_report_key(kbd_ctx->input_dev, keycode, FALSE);
	}
}

static void report_state_and_scancode(struct bbqX0kbd_data* kbd_ctx,
	uint8_t key_state, uint8_t key_scancode)
{
	unsigned short keycode;
	uint8_t should_report_key = 2; // Report by default

	// Only handle key pressed or released events
	if ((key_state != KEY_PRESSED_STATE) && (key_state != KEY_RELEASED_STATE)) {
		return;
	}

	// Post key scan event
	input_event(kbd_ctx->input_dev, EV_MSC, MSC_SCAN, key_scancode);

	// Map input scancode to Linux input keycode
	keycode = kbd_ctx->keycode_map[key_scancode];
	dev_info(&kbd_ctx->i2c_client->dev,
		"%s state %d, scancode %d mapped to keycode %d\n",
		__func__, key_state, key_scancode, keycode);

	// Set / get modifiers, report key event
	switch (keycode) {

	case KEY_UNKNOWN:
		dev_warn(&kbd_ctx->i2c_client->dev,
			"%s Could not get Keycode for Scancode: [0x%02X]\n",
				__func__, key_scancode);
		break;

	case KEY_LEFTSHIFT:
		transition_sticky_modifier_key(kbd_ctx, key_state, SHIFT_BIT, KEY_LEFTSHIFT);
		break;

	default:
		apply_sticky_modifier_keys(kbd_ctx, key_state, SHIFT_BIT, KEY_LEFTSHIFT);
		input_report_key(kbd_ctx->input_dev, keycode, key_state == KEY_PRESSED_STATE);
		reset_sticky_modifier_keys(kbd_ctx, key_state, SHIFT_BIT, KEY_LEFTSHIFT);
	}
}

static void bbqX0kbd_work_fnc(struct work_struct *work_struct_ptr)
{
	struct bbqX0kbd_data *kbd_ctx;
	uint8_t fifo_idx;
	int rc;

	// Get keyboard context from work struct
	kbd_ctx = container_of(work_struct_ptr, struct bbqX0kbd_data, work_struct);

	// Process FIFO items
	for (fifo_idx = 0; fifo_idx < kbd_ctx->fifo_count; fifo_idx++) {
		report_state_and_scancode(kbd_ctx,
			kbd_ctx->fifo_data[fifo_idx][0],  // Key state
			kbd_ctx->fifo_data[fifo_idx][1]); // Key scancode
	}

	// Reset pending FIFO count
	kbd_ctx->fifo_count = 0;

	// Handle touch interrupt flag
	if (kbd_ctx->touch_event_flag) {

		dev_info_ld(&kbd_ctx->i2c_client->dev,
			"%s X Reg: %d Y Reg: %d.\n",
			__func__, kbd_ctx->rel_x, kbd_ctx->rel_y);

		#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)

			// Report mouse movement
			input_report_rel(input_dev, REL_X, kbd_ctx->touch_rel_x);
			input_report_rel(input_dev, REL_Y, kbd_ctx->touch_rel_y);

			// Clear touch interrupt flag
			kbd_ctx->touch_event_flag = 0;
		#endif

		#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_KEYS)

			// Negative X: left arrow key
			if (kbd_ctx->touch_rel_x < -4) {
				input_report_key(kbd_ctx->input_dev, KEY_LEFT, TRUE);
				input_report_key(kbd_ctx->input_dev, KEY_LEFT, FALSE);

			// Positive X: right arrow key
			} else if (kbd_ctx->touch_rel_x > 4) {
				input_report_key(kbd_ctx->input_dev, KEY_RIGHT, TRUE);
				input_report_key(kbd_ctx->input_dev, KEY_RIGHT, FALSE);
			}

			// Negative Y: up arrow key
			if (kbd_ctx->touch_rel_y < -4) {
				input_report_key(kbd_ctx->input_dev, KEY_UP, TRUE);
				input_report_key(kbd_ctx->input_dev, KEY_UP, FALSE);

			// Positive Y: down arrow key
			} else if (kbd_ctx->touch_rel_y > 4) {
				input_report_key(kbd_ctx->input_dev, KEY_DOWN, TRUE);
				input_report_key(kbd_ctx->input_dev, KEY_DOWN, FALSE);
			}

			// Clear touch interrupt flag
			kbd_ctx->touch_event_flag = 0;
		#endif
	}

	// Synchronize input system and clear client interrupt flag
	input_sync(kbd_ctx->input_dev);
	WRITE_I2C_U8_OR_DEV_ERR(rc, kbd_ctx->i2c_client, REG_INT, 0, return);
}

static irqreturn_t bbqX0kbd_irq_handler(int irq, void *param)
{
	struct bbqX0kbd_data *kbd_ctx;
	int rc;
	uint8_t irq_type;

	// `param` is current keyboard context as started in _probe
	kbd_ctx = (struct bbqX0kbd_data *)param;

	dev_info_ld(&kbd_ctx->i2c_client->dev,
		"%s Interrupt Fired. IRQ: %d\n", __func__, irq);

	// Read interrupt type from client
	READ_I2C_U8_OR_DEV_ERR(rc, kbd_ctx->i2c_client, REG_INT, &irq_type, return IRQ_NONE);
	dev_info_ld(&kbd_ctx->i2c_client->dev,
		"%s Interrupt type: 0x%02x\n", __func__, irq_type);

	// Reported no interrupt type
	if (irq_type == 0x00) {
		return IRQ_NONE;
	}

	// Client reported a key overflow
	if (irq_type & REG_INT_OVERFLOW) {
		dev_warn(&kbd_ctx->i2c_client->dev, "%s overflow occurred.\n", __func__);
	}

	// Client reported a key event
	if (irq_type & REG_INT_KEY) {
		bbqX0kbd_read_fifo(kbd_ctx);
		schedule_work(&kbd_ctx->work_struct);
	}

	// Client reported a touch event
	if (irq_type & REG_INT_TOUCH) {

		// Read touch X-coordinate
		READ_I2C_U8_OR_DEV_ERR(rc, kbd_ctx->i2c_client, REG_TOX,
			&kbd_ctx->touch_rel_x, return IRQ_NONE);

		// Read touch Y-coordinate
		READ_I2C_U8_OR_DEV_ERR(rc, kbd_ctx->i2c_client, REG_TOY,
			&kbd_ctx->touch_rel_y, return IRQ_NONE);

		// Set touch event flag and schedule touch work
		kbd_ctx->touch_event_flag = 1;
		schedule_work(&kbd_ctx->work_struct);

	} else {

		// Clear touch event flag
		kbd_ctx->touch_event_flag = 0;
	}

	return IRQ_HANDLED;
}

static int bbqX0kbd_probe(struct i2c_client* i2c_client, struct i2c_device_id const* i2c_id)
{
	struct bbqX0kbd_data *kbd_ctx;
	int rc, i;
	uint8_t reg_value;

	dev_info_fe(&i2c_client->dev,
		"%s Probing BBQX0KBD.\n", __func__);

	// Allocate keyboard context (managed by device lifetime)
	kbd_ctx = devm_kzalloc(&i2c_client->dev, sizeof(*kbd_ctx), GFP_KERNEL);
	if (!kbd_ctx) {
		return -ENOMEM;
	}

	// Initialize keyboard context
	kbd_ctx->i2c_client = i2c_client;
	memcpy(kbd_ctx->keycode_map, keycodes, sizeof(kbd_ctx->keycode_map));
	kbd_ctx->held_modifier_keys = 0x00;
	kbd_ctx->pending_sticky_modifier_keys = 0x00;
	kbd_ctx->sticky_modifier_keys = 0x00;
	kbd_ctx->brightness = 0xFF;
	kbd_ctx->last_brightness = 0xFF;

	// Get firmware version
	READ_I2C_U8_OR_DEV_ERR(rc, i2c_client, REG_VER, &kbd_ctx->version_number,
		return -ENODEV);
	dev_info(&i2c_client->dev,
		"%s BBQX0KBD indev Software version: 0x%02X\n", __func__, kbd_ctx->version_number);

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
	(void)kbd_write_i2c_u8(i2c_client, REG_BKL, kbd_ctx->brightness);

	// Allocate input device
	if ((kbd_ctx->input_dev = devm_input_allocate_device(&i2c_client->dev)) == NULL) {
		dev_err(&i2c_client->dev,
			"%s Could not devm_input_allocate_device BBQX0KBD.\n", __func__);
		return -ENOMEM;
	}

	// Initialize input device
	kbd_ctx->input_dev->name = i2c_client->name;
	kbd_ctx->input_dev->id.bustype = BBQX0KBD_BUS_TYPE;
	kbd_ctx->input_dev->id.vendor  = BBQX0KBD_VENDOR_ID;
	kbd_ctx->input_dev->id.product = BBQX0KBD_PRODUCT_ID;
	kbd_ctx->input_dev->id.version = BBQX0KBD_VERSION_ID;

	// Initialize input device keycodes
	kbd_ctx->input_dev->keycode = kbd_ctx->keycode_map;
	kbd_ctx->input_dev->keycodesize = sizeof(kbd_ctx->keycode_map[0]);
	kbd_ctx->input_dev->keycodemax = ARRAY_SIZE(kbd_ctx->keycode_map);

	// Set input device keycode bits
	for (i = 0; i < NUM_KEYCODES; i++) {
		__set_bit(kbd_ctx->keycode_map[i], kbd_ctx->input_dev->keybit);
	}
	__clear_bit(KEY_RESERVED, kbd_ctx->input_dev->keybit);
	__set_bit(EV_REP, kbd_ctx->input_dev->evbit);
	__set_bit(EV_KEY, kbd_ctx->input_dev->evbit);

	// Set input device capabilities
	input_set_capability(kbd_ctx->input_dev, EV_MSC, MSC_SCAN);
	#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)
		input_set_capability(kbd_ctx->input_dev, EV_REL, REL_X);
		input_set_capability(kbd_ctx->input_dev, EV_REL, REL_Y);
	#endif

	// Request IRQ handler for I2C client and initialize workqueue
	if ((rc = devm_request_threaded_irq(&i2c_client->dev,
		i2c_client->irq, NULL, bbqX0kbd_irq_handler, IRQF_SHARED | IRQF_ONESHOT,
		i2c_client->name, kbd_ctx))) {

		dev_err(&i2c_client->dev,
			"Could not claim IRQ %d; error %d\n", i2c_client->irq, rc);
		return rc;
	}
	INIT_WORK(&kbd_ctx->work_struct, bbqX0kbd_work_fnc);

	// Register input device with input subsystem
	dev_info(&i2c_client->dev,
		"%s registering input device", __func__);
	if ((rc = input_register_device(kbd_ctx->input_dev))) {
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

	dev_info_fe(&client->dev,
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
static int really_init=0;
module_param(really_init,int,0);
static int __init bbqX0kbd_init(void)
{
	int returnValue;

	if (really_init) {

	returnValue = i2c_add_driver(&bbqX0kbd_driver);
	if (returnValue != 0) {
		pr_err("%s Could not initialise BBQX0KBD driver! Error: %d\n", __func__, returnValue);
		return returnValue;
	}
	pr_info("%s Initalised BBQX0KBD.\n", __func__);

	} else {
		pr_info("%s Didn't really_init BBQX0KBD.\n", __func__);
		return 0;
	}
	return returnValue;
}
module_init(bbqX0kbd_init);

static void __exit bbqX0kbd_exit(void)
{
	pr_info("%s Exiting BBQX0KBD.\n", __func__);
	if (really_init) {
	i2c_del_driver(&bbqX0kbd_driver);
	}
}
module_exit(bbqX0kbd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wallComputer");
MODULE_DESCRIPTION("Keyboard driver for BBQ10, hardware by arturo182 <arturo182@tlen.pl>, software by wallComputer.");
MODULE_VERSION("0.4");