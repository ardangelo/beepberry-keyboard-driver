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
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h> 
#include <linux/kobject.h> 
#include <linux/err.h>

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

// Kernel module parameters
static char* touchpad_setting = "meta"; // Use meta mode by default

// Internal structs

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
	uint8_t touchpad_always_keys; // Set by parameter `touchpad_setting`

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
	uint8_t meta_touch_keys_mode;

	// Apply modifiers to the next alpha keypress
	uint8_t apply_control;
	uint8_t apply_alt;

	// Keyboard brightness
	uint8_t brightness;
	uint8_t last_brightness;

	struct i2c_client *i2c_client;
	struct input_dev *input_dev;
};

// Global keyboard context and sysfs data
static struct kbd_ctx *g_ctx = NULL;
static struct kobject *g_beepberry_kobj = NULL;

// Helper functions

// Update touchpad setting in global context, if available
static bool set_touchpad_setting(struct kbd_ctx *ctx, char const* val)
{
	if (strncmp(val, "keys", 4) == 0) {
		if (ctx) { ctx->touchpad_always_keys = 1; }
		return TRUE;

	} else if (strncmp(val, "meta", 4) == 0) {
		if (ctx) { ctx->touchpad_always_keys = 0; }
		return TRUE;
	}

	return FALSE;
}

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

// Sysfs implementations

// Read battery level over I2C
static int read_raw_battery_level(void)
{
	int rc;
	uint8_t battery_level[2];

	// Make sure I2C client was initialized
	if ((g_ctx == NULL) || (g_ctx->i2c_client == NULL)) {
		return -EINVAL;
	}

	// Read battery level
	if ((rc = kbd_read_i2c_2u8(g_ctx->i2c_client, REG_ADC, battery_level)) < 0) {
		return rc;
	}

	// Calculate raw battery level
	return (battery_level[1] << 8) | battery_level[0];
}

// Raw battery level
static ssize_t battery_raw_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	int total_level;

	// Read raw level
	if ((total_level = read_raw_battery_level()) < 0) {
		return total_level;
	}

	// Format into buffer
	return sprintf(buf, "%d", total_level);
}
struct kobj_attribute battery_raw_attr
	= __ATTR(battery_raw, 0440, battery_raw_show, NULL);

// Battery volts level
static ssize_t battery_volts_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	int volts_fp;

	// Read raw level
	if ((volts_fp = read_raw_battery_level()) < 0) {
		return volts_fp;
	}

	// Calculate voltage in fixed point
	volts_fp *= 330 * 2;
	volts_fp /= 4095;

	// Format into buffer
	return sprintf(buf, "%d.%d", volts_fp / 100, volts_fp % 100);
}
struct kobj_attribute battery_volts_attr
	= __ATTR(battery_volts, 0440, battery_volts_show, NULL);


// Red, green, and blue LED settings, max of 0xff
static uint8_t parse_u8(char const* buf)
{
	int rc, result;

	// Parse string value
	if ((rc = kstrtoint(buf, 10, &result)) || (result < 0) || (result > 0xff)) {
		return -EINVAL;
	}
	return (uint8_t)result;
}

static int parse_and_write_i2c_u8(char const* buf, size_t count, uint8_t reg)
{
	uint8_t parsed;
	
	// Parse string entry
	if ((parsed = parse_u8(buf)) < 0) {
		return -EINVAL;
	}

	// Write value to LED register if available
	if (g_ctx && g_ctx->i2c_client) {
		kbd_write_i2c_u8(g_ctx->i2c_client, reg, parsed);
	}

	return count;
}

// LED on or off
static ssize_t led_store(struct kobject *kobj, struct kobj_attribute *attr,
	char const *buf, size_t count)
{
	return parse_and_write_i2c_u8(buf, count, REG_LED);
}
struct kobj_attribute led_attr = __ATTR(led, 0220, NULL, led_store);

// LED red value
static ssize_t led_red_store(struct kobject *kobj, struct kobj_attribute *attr,
	char const *buf, size_t count)
{
	return parse_and_write_i2c_u8(buf, count, REG_LED_R);
}
struct kobj_attribute led_red_attr = __ATTR(led_red, 0220, NULL, led_red_store);

// LED green value
static ssize_t led_green_store(struct kobject *kobj, struct kobj_attribute *attr,
	char const *buf, size_t count)
{
	return parse_and_write_i2c_u8(buf, count, REG_LED_G);
}
struct kobj_attribute led_green_attr = __ATTR(led_green, 0220, NULL, led_green_store);

// LED blue value
static ssize_t __used led_blue_store(struct kobject *kobj, struct kobj_attribute *attr,
	char const *buf, size_t count)
{
	return parse_and_write_i2c_u8(buf, count, REG_LED_B);
}
struct kobj_attribute led_blue_attr = __ATTR(led_blue, 0220, NULL, led_blue_store);

// Keyboard backlight value
static ssize_t __used keyboard_backlight_store(struct kobject *kobj,
	struct kobj_attribute *attr, char const *buf, size_t count)
{
	return parse_and_write_i2c_u8(buf, count, REG_BKL);
}
struct kobj_attribute keyboard_backlight_attr
	= __ATTR(keyboard_backlight, 0220, NULL, keyboard_backlight_store);

// Sysfs attributes (entries)
static struct attribute *beepberry_attrs[] = {
    &battery_raw_attr.attr,
	&battery_volts_attr.attr,
	&led_attr.attr,
	&led_red_attr.attr,
	&led_green_attr.attr,
	&led_blue_attr.attr,
	&keyboard_backlight_attr.attr,	
    NULL,
};
static struct attribute_group beepberry_attr_group = {
    .attrs = beepberry_attrs
};

// Device settings helpers

static void bbqX0kbd_decrease_brightness(struct kbd_ctx* ctx)
{
	// Decrease by delta, min at 0x0 brightness
	ctx->brightness = (ctx->brightness < BBQ10_BRIGHTNESS_DELTA)
		? 0x0
		: ctx->brightness - BBQ10_BRIGHTNESS_DELTA;

	// Set backlight using I2C
	(void)kbd_write_i2c_u8(ctx->i2c_client, REG_BKL, ctx->brightness);
}

static void bbqX0kbd_increase_brightness(struct kbd_ctx* ctx)
{
	// Increase by delta, max at 0xff brightness
	ctx->brightness = (ctx->brightness > (0xff - BBQ10_BRIGHTNESS_DELTA))
		? 0xff
		: ctx->brightness + BBQ10_BRIGHTNESS_DELTA;

	// Set backlight using I2C
	(void)kbd_write_i2c_u8(ctx->i2c_client, REG_BKL, ctx->brightness);
}

static void bbqX0kbd_toggle_brightness(struct kbd_ctx* ctx)
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

static void enable_meta_touch_keys_mode(struct kbd_ctx* ctx)
{
	int rc;

	// Enable touch interrupts on I2C
	WRITE_I2C_U8_OR_DEV_ERR(rc, ctx->i2c_client, REG_CF2, REG_CF2_TOUCH_INT,
		return);

	ctx->meta_touch_keys_mode = 1;
}

static void disable_meta_touch_keys_mode(struct kbd_ctx* ctx)
{
	int rc;

	// Disable touch interrupts on I2C
	WRITE_I2C_U8_OR_DEV_ERR(rc, ctx->i2c_client, REG_CF2, 0,
		return);

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

	case KEY_N: bbqX0kbd_decrease_brightness(ctx); return;
	case KEY_M: bbqX0kbd_increase_brightness(ctx); return;
	case KEY_MUTE:
		bbqX0kbd_toggle_brightness(ctx);
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

	// Save pointer in global state to allow parameters to update settings
	g_ctx = ctx;

	// Initialize keyboard context
	ctx->i2c_client = i2c_client;
	memcpy(ctx->keycode_map, keycodes, sizeof(ctx->keycode_map));
	ctx->meta_mode = 0;
	ctx->meta_touch_keys_mode = 0;
	ctx->apply_control = 0;
	ctx->apply_alt = 0;
	ctx->brightness = 0xFF;
	ctx->last_brightness = 0xFF;
	set_touchpad_setting(ctx, touchpad_setting);

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

	// Write configuration 2: touch disabled if not in keys mode
	WRITE_I2C_U8_OR_DEV_ERR(rc, i2c_client, REG_CF2,
		(ctx->touchpad_always_keys) ? REG_CF2_TOUCH_INT : 0,
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

	// Create sysfs entry for beepberry
	if ((g_beepberry_kobj = kobject_create_and_add("beepberry", firmware_kobj)) == NULL) {
    	return -ENOMEM;
	}

	// Create sysfs attributes
	if (sysfs_create_group(g_beepberry_kobj, &beepberry_attr_group)){
		kobject_put(g_beepberry_kobj);
		return -ENOMEM;
	}

	return 0;
}

static void bbqX0kbd_shutdown(struct i2c_client* i2c_client)
{
	int rc;
	uint8_t reg_value;

	dev_info_fe(&i2c_client->dev,
		"%s Shutting Down Keyboard And Screen Backlight.\n", __func__);

	// Remove sysfs entry
	if (g_beepberry_kobj) {
		kobject_put(g_beepberry_kobj);
		g_beepberry_kobj = NULL;
	}
	
	// Turn off backlight
	WRITE_I2C_U8_OR_DEV_ERR(rc, i2c_client, REG_BKL, 0, return);

	// Read back version
	READ_I2C_U8_OR_DEV_ERR(rc, i2c_client, REG_VER, &reg_value, return);

	// Reenable touch events
	WRITE_I2C_U8_OR_DEV_ERR(rc, i2c_client, REG_CF2, REG_CFG2_DEFAULT_SETTING, return);

	// Remove context from global state
	// (It is freed by the device-specific memory mananger)
	g_ctx = NULL;
}

static void bbqX0kbd_remove(struct i2c_client* i2c_client)
{
	dev_info_fe(&i2c_client->dev,
		"%s Removing BBQX0KBD.\n", __func__);

	bbqX0kbd_shutdown(i2c_client);
}

// Kernel module parameters

// Use touchpad for meta mode, or use it as arrow keys
static int touchpad_setting_param_set(const char *val, const struct kernel_param *kp)
{
	char *stripped_val;
	char stripped_val_buf[5];

	// Copy provided value to buffer and strip it of newlines
	strncpy(stripped_val_buf, val, 5);
	stripped_val_buf[4] = '\0';
	stripped_val = strstrip(stripped_val_buf);

	return (set_touchpad_setting(g_ctx, stripped_val))
		? param_set_charp(stripped_val, kp)
		: -EINVAL;
}
static const struct kernel_param_ops touchpad_setting_param_ops = {
	.set = touchpad_setting_param_set,
	.get = param_get_charp,
};
module_param_cb(touchpad, &touchpad_setting_param_ops, &touchpad_setting, 0664);
MODULE_PARM_DESC(touchpad_setting, "Touchpad as arrow keys (\"keys\") or click for meta mode (\"meta\")");

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