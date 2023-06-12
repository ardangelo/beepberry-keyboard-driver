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

#define LEFT_CTRL_BIT			BIT(0)
#define LEFT_SHIFT_BIT			BIT(1)
#define LEFT_ALT_BIT			BIT(2)
#define LEFT_GUI_BIT			BIT(3)
#define RIGHT_CTRL_BIT			BIT(4)
#define RIGHT_SHIFT_BIT			BIT(5)
#define RIGHT_ALT_BIT			BIT(6)
#define RIGHT_GUI_BIT			BIT(7)

#define CAPS_LOCK_BIT			BIT(0)
#define NUMS_LOCK_BIT			BIT(1)

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

struct bbqX0kbd_data {
    struct work_struct work_struct;
    uint8_t fifoCount;
    uint8_t fifoData[BBQX0KBD_FIFO_SIZE][2];

    uint8_t touchInt;
    int8_t rel_x;
    int8_t rel_y;

    uint8_t version_number;
    uint8_t modifier_keys_status;
    uint8_t lockStatus;
    uint8_t keyboardBrightness;
    uint8_t lastKeyboardBrightness;

	unsigned short keycode[NUM_KEYCODES];

    struct i2c_client *i2c_client;
    struct input_dev *input_dev;
};

// Helper functions
static int kbd_read_i2c_u8(struct i2c_client* i2c_client, uint8_t reg_addr, uint8_t* dst)
{
	return bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, reg_addr, dst, sizeof(uint8_t));
}

static int kbd_write_i2c_u8(struct i2c_client* i2c_client, uint8_t reg_addr, uint8_t src)
{
	return bbqX0kbd_write(i2c_client, BBQX0KBD_I2C_ADDRESS, reg_addr, &src,
		sizeof(uint8_t));
}

static int kbd_read_i2c_2u8(struct i2c_client* i2c_client, uint8_t reg_addr, uint8_t* dst)
{
	return bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, reg_addr, dst,
		2 * sizeof(uint8_t));
}

static uint8_t bbqX0kbd_modkeys_to_bits(unsigned short mod_keycode)
{
	uint8_t returnValue;

	switch (mod_keycode) {
	case (KEY_LEFTCTRL):
		returnValue = LEFT_CTRL_BIT;
		break;
	case KEY_RIGHTCTRL:
		returnValue = RIGHT_CTRL_BIT;
		break;
	case KEY_LEFTSHIFT:
		returnValue = LEFT_SHIFT_BIT;
		break;
	case KEY_RIGHTSHIFT:
		returnValue = RIGHT_SHIFT_BIT;
		break;
	case KEY_LEFTALT:
		returnValue = LEFT_ALT_BIT;
		break;
	case KEY_RIGHTALT:
		returnValue = RIGHT_ALT_BIT;
		break;
	//TODO: Add for GUI Key if needed.
	}
	return returnValue;
}

static unsigned short bbqX0kbd_get_num_lock_keycode(unsigned short keycode)
{
	unsigned short returnValue;

	switch (keycode) {
	case KEY_W:
		returnValue = KEY_1;
		break;
	case KEY_E:
		returnValue = KEY_2;
		break;
	case KEY_R:
		returnValue = KEY_3;
		break;
	case KEY_S:
		returnValue = KEY_4;
		break;
	case KEY_D:
		returnValue = KEY_5;
		break;
	case KEY_F:
		returnValue = KEY_6;
		break;
	case KEY_Z:
		returnValue = KEY_7;
		break;
	case KEY_X:
		returnValue = KEY_8;
		break;
	case KEY_C:
		returnValue = KEY_9;
		break;
	case KEY_GRAVE:
		returnValue = KEY_0;
		break;
	default:
		returnValue = keycode;
		break;
	}
	return returnValue;
}

static unsigned short bbqX0kbd_get_altgr_keycode(unsigned short keycode)
{
	unsigned short returnValue;

	switch (keycode) {
	case KEY_E:
		returnValue = KEY_PAGEDOWN;
		break;
	case KEY_R:
		returnValue = KEY_PAGEUP;
		break;
	case KEY_Y:
		returnValue = KEY_UP;
		break;
	case KEY_G:
		returnValue = KEY_LEFT;
		break;
	case KEY_H:
		returnValue = KEY_HOME;
		break;
	case KEY_J:
		returnValue = KEY_RIGHT;
		break;
	case KEY_B:
		returnValue = KEY_DOWN;
		break;
	case KEY_M:
		returnValue = KEY_MENU;
		break;
	case KEY_K:
		returnValue = KEY_VOLUMEUP;
		break;
	case KEY_L:
		returnValue = KEY_VOLUMEDOWN;
		break;
	case KEY_GRAVE:
		returnValue = KEY_MUTE;
		break;
	case KEY_BACKSPACE:
		returnValue = KEY_DELETE;
		break;
	default:
		returnValue = keycode;
		break;
	}
	return returnValue;
}

static void bbqX0kbd_set_brightness(struct bbqX0kbd_data* kbd_ctx,
	unsigned short keycode, uint8_t* should_report_key)
{
	if (keycode == KEY_Z) {
		*should_report_key = 0;

		// Increase by delta, max at 0xff brightness
		kbd_ctx->keyboardBrightness
			= (kbd_ctx->keyboardBrightness > (0xff - BBQ10_BRIGHTNESS_DELTA))
				? 0xff
				: kbd_ctx->keyboardBrightness + BBQ10_BRIGHTNESS_DELTA;

	} else if (keycode == KEY_X) {
		*should_report_key = 0;

		// Decrease by delta, min at 0x0 brightness
		kbd_ctx->keyboardBrightness
			= (kbd_ctx->keyboardBrightness < BBQ10_BRIGHTNESS_DELTA)
				? 0x0
				: kbd_ctx->keyboardBrightness - BBQ10_BRIGHTNESS_DELTA;

	} else if (keycode == KEY_0) {
		*should_report_key = 0;

		// Toggle off, save last brightness in context
		kbd_ctx->lastKeyboardBrightness = kbd_ctx->keyboardBrightness;
		kbd_ctx->keyboardBrightness = 0;

	// Not a brightness control key, don't consume it
	} else {
		*should_report_key = 2;
	}

	// If it was a brightness control key, set backlight
	if (*should_report_key == 0) {
		(void)kbd_write_i2c_u8(kbd_ctx->i2c_client, REG_BKL, kbd_ctx->keyboardBrightness);
	}
}

// Transfer from I2C FIFO to internal context FIFO
static void bbqX0kbd_read_fifo(struct bbqX0kbd_data* kbd_ctx)
{
	uint8_t fifo_count, fifo_idx;
	int rc;

	// Read number of FIFO items
	if ((rc = kbd_read_i2c_u8(kbd_ctx->i2c_client, REG_KEY, &fifo_count))) {
		dev_err(&kbd_ctx->i2c_client->dev,
			"%s Could not read REG_KEY, Error: %d\n", __func__, rc);
		return;
	}
	fifo_count &= REG_KEY_KEYCOUNT_MASK;
	kbd_ctx->fifoCount = fifo_count;

	// Read and transfer all FIFO items
	fifo_idx = 0;
	while (fifo_count > 0) {

		// Read 2 fifo items
		if ((rc = kbd_read_i2c_2u8(kbd_ctx->i2c_client, REG_FIF,
			kbd_ctx->fifoData[fifo_idx]))) {

			dev_err(&kbd_ctx->i2c_client->dev,
				"%s Could not read REG_FIF, Error: %d\n", __func__, rc);
			return;
		}

		// Advance FIFO position
		dev_info_ld(&kbd_ctx->i2c_client->dev,
			"%s Filled Data: KeyState:%d SCANCODE:%d at Pos: %d Count: %d\n",
			__func__, kbd_ctx->fifoData[fifo_idx][0], kbd_ctx->fifoData[fifo_idx][1],
			fifo_idx, fifo_count);
		fifo_idx++;
		fifo_count--;
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
	keycode = kbd_ctx->keycode[key_scancode];
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

	case KEY_RIGHTSHIFT:
		if ((kbd_ctx->modifier_keys_status & LEFT_ALT_BIT)
		 && (key_state == KEY_PRESSED_STATE)) {

			// Set numlock (AltGr) mode
			kbd_ctx->lockStatus ^= NUMS_LOCK_BIT;
		}
		fallthrough;

	case KEY_LEFTSHIFT:
	case KEY_RIGHTALT:
	case KEY_LEFTALT:
	case KEY_LEFTCTRL:
	case KEY_RIGHTCTRL:
		if (key_state == KEY_PRESSED_STATE) {
			kbd_ctx->modifier_keys_status |= bbqX0kbd_modkeys_to_bits(keycode);
		} else {
			kbd_ctx->modifier_keys_status &= ~bbqX0kbd_modkeys_to_bits(keycode);
		}
		fallthrough;

	default:
		if (kbd_ctx->lockStatus & NUMS_LOCK_BIT) {
			keycode = bbqX0kbd_get_num_lock_keycode(keycode);

		} else if (kbd_ctx->modifier_keys_status & RIGHT_ALT_BIT) {
			keycode = bbqX0kbd_get_altgr_keycode(keycode);

		#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)
		} else if ((kbd_ctx->modifier_keys_status & LEFT_ALT_BIT)
				&& (keycode == KEY_ENTER)) {
			keycode = BTN_RIGHT;
		#endif
		}

		if ((kbd_ctx->modifier_keys_status & RIGHT_ALT_BIT)
			&& (key_state == KEY_PRESSED_STATE)) {

			// Set brightness if proper key is pressed
			// If so, don't report the key to input
			bbqX0kbd_set_brightness(kbd_ctx, keycode, &should_report_key);
		}

		// If input was not consumed by brightness setting, report key to input
		if (should_report_key == 2) {
			input_report_key(kbd_ctx->input_dev, keycode,
				key_state == KEY_PRESSED_STATE);
			break;
		}
	}
}

static void bbqX0kbd_work_fnc(struct work_struct *work_struct_ptr)
{
	struct bbqX0kbd_data *kbd_ctx;
	uint8_t fifo_idx = 0;
	int rc;

	// Get keyboard context from work struct
	kbd_ctx = container_of(work_struct_ptr, struct bbqX0kbd_data, work_struct);

	while (kbd_ctx->fifoCount > 0) {
		report_state_and_scancode(kbd_ctx,
			kbd_ctx->fifoData[fifo_idx][0],  // Key state
			kbd_ctx->fifoData[fifo_idx][1]); // Key scancode

		// Advance FIFO position
		fifo_idx++;
		kbd_ctx->fifoCount--;
	}

	// Handle touch interrupt flag
	if (kbd_ctx->touchInt) {

		dev_info_ld(&kbd_ctx->i2c_client->dev,
			"%s X Reg: %d Y Reg: %d.\n",
			__func__, kbd_ctx->rel_x, kbd_ctx->rel_y);

		#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)

			// Report mouse movement
			input_report_rel(input_dev, REL_X, kbd_ctx->rel_x);
			input_report_rel(input_dev, REL_Y, kbd_ctx->rel_y);

			// Clear touch interrupt flag
			kbd_ctx->touchInt = 0;
		#endif

		#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_KEYS)

			// Negative X: left arrow key
			if (kbd_ctx->rel_x < -4) {
				input_report_key(kbd_ctx->input_dev, KEY_LEFT, TRUE);
				input_report_key(kbd_ctx->input_dev, KEY_LEFT, FALSE);

			// Positive X: right arrow key
			} else if (kbd_ctx->rel_x > 4) {
				input_report_key(kbd_ctx->input_dev, KEY_RIGHT, TRUE);
				input_report_key(kbd_ctx->input_dev, KEY_RIGHT, FALSE);
			}

			// Negative Y: up arrow key
			if (kbd_ctx->rel_y < -4) {
				input_report_key(kbd_ctx->input_dev, KEY_UP, TRUE);
				input_report_key(kbd_ctx->input_dev, KEY_UP, FALSE);

			// Positive Y: down arrow key
			} else if (kbd_ctx->rel_y > 4) {
				input_report_key(kbd_ctx->input_dev, KEY_DOWN, TRUE);
				input_report_key(kbd_ctx->input_dev, KEY_DOWN, FALSE);
			}

			// Clear touch interrupt flag
			kbd_ctx->touchInt = 0;
		#endif
	}

	// Synchronize input system and clear client interrupt flag
	input_sync(kbd_ctx->input_dev);
	if ((rc = kbd_write_i2c_u8(kbd_ctx->i2c_client, REG_INT, 0)) < 0) {
		dev_err(&kbd_ctx->i2c_client->dev,
			"%s Could not clear REG_INT. Error: %d\n",
			__func__, rc);
	}
}

static irqreturn_t bbqX0kbd_irq_handler(int irq, void *param)
{
	struct bbqX0kbd_data *kbd_ctx;
	int rc;
	uint8_t irq_type, reg_value;

	// `param` is current keyboard context as started in _probe
	kbd_ctx = (struct bbqX0kbd_data *)param;

	dev_info_ld(&kbd_ctx->i2c_client->dev,
		"%s Interrupt Fired. IRQ: %d\n", __func__, irq);

	// Read interrupt type from client
	if ((rc = kbd_read_i2c_u8(kbd_ctx->i2c_client, REG_INT, &irq_type)) < 0) {
		dev_err(&kbd_ctx->i2c_client->dev,
			"%s: Could not read REG_INT. Error: %d\n", __func__, rc);
		return IRQ_NONE;
	}

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
		if ((rc = kbd_read_i2c_u8(kbd_ctx->i2c_client, REG_TOX, &reg_value)) < 0) {
			dev_err(&kbd_ctx->i2c_client->dev,
				"%s : Could not read REG_TOX. Error: %d\n", __func__, rc);
			return IRQ_NONE;
		}
		kbd_ctx->rel_x = reg_value;

		// Read touch Y-coordinate
		if ((rc = kbd_read_i2c_u8(kbd_ctx->i2c_client, REG_TOY, &reg_value)) < 0) {
			dev_err(&kbd_ctx->i2c_client->dev,
				"%s : Could not read REG_TOY. Error: %d\n", __func__, rc);
			return IRQ_NONE;
		}
		kbd_ctx->rel_y = reg_value;

		// Set touch event flag and schedule touch work
		kbd_ctx->touchInt = 1;
		schedule_work(&kbd_ctx->work_struct);

	} else {

		// Clear touch event flag
		kbd_ctx->touchInt = 0;
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
	memcpy(kbd_ctx->keycode, keycodes, sizeof(kbd_ctx->keycode));
	kbd_ctx->modifier_keys_status = 0x00; // Equal to idle state of all keys
	kbd_ctx->lockStatus = 0x00;
	kbd_ctx->keyboardBrightness = 0xFF;
	kbd_ctx->lastKeyboardBrightness = 0xFF;

	// Get firmware version
	if ((rc = kbd_read_i2c_u8(i2c_client, REG_VER, &reg_value))) {
		dev_err(&i2c_client->dev,
			"%s Could not Read Version BBQX0KBD. Error: %d\n", __func__, rc);
		return -ENODEV;
	}
	dev_info(&i2c_client->dev,
		"%s BBQX0KBD indev Software version: 0x%02X\n", __func__, reg_value);
	kbd_ctx->version_number = reg_value;

	// Write configuration 1
	if ((rc = kbd_write_i2c_u8(i2c_client, REG_CFG, REG_CFG_DEFAULT_SETTING))) {
		dev_err(&i2c_client->dev,
			"%s Could not write config to BBQX0KBD. Error: %d\n", __func__, rc);
		return -ENODEV;
	}

	// Read back configuration 1 setting
	if ((rc = kbd_read_i2c_u8(i2c_client, REG_CFG, &reg_value))) {
		dev_err(&i2c_client->dev,
			"%s Could not read REG_CFG. Error: %d\n", __func__, rc);
		return rc;
	}
	dev_info_ld(&i2c_client->dev,
		"%s Configuration Register Value: 0x%02X\n", __func__, reg_value);

	// Write configuration 2
	if ((rc = kbd_write_i2c_u8(i2c_client, REG_CF2, REG_CFG2_DEFAULT_SETTING))) {
		dev_err(&i2c_client->dev,
			"%s Could not write config 2 to BBQX0KBD. Error: %d\n", __func__, rc);
		return -ENODEV;
	}

	// Read back configuration 2 setting
	if ((rc = kbd_read_i2c_u8(i2c_client, REG_CF2, &reg_value))) {
		dev_err(&i2c_client->dev,
			"%s Could not read REG_CF2. Error: %d\n", __func__, rc);
		return rc;
	}
	dev_info_ld(&i2c_client->dev,
		"%s Configuration 2 Register Value: 0x%02X\n",
		__func__, reg_value);

	// Update keyboard brightness
	(void)kbd_write_i2c_u8(i2c_client, REG_BKL, kbd_ctx->keyboardBrightness);

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
	kbd_ctx->input_dev->keycode = kbd_ctx->keycode;
	kbd_ctx->input_dev->keycodesize = sizeof(kbd_ctx->keycode[0]);
	kbd_ctx->input_dev->keycodemax = ARRAY_SIZE(kbd_ctx->keycode);

	// Set input device keycode bits
	for (i = 0; i < NUM_KEYCODES; i++) {
		__set_bit(kbd_ctx->keycode[i], kbd_ctx->input_dev->keybit);
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
	if ((rc = kbd_write_i2c_u8(i2c_client, REG_BKL, 0))) {
		dev_err(&i2c_client->dev,
			"%s Could not write to BBQX0KBD Backlight. Error: %d\n", __func__, rc);
		return;
	}

	// Read back version
	if ((rc = kbd_read_i2c_u8(i2c_client, REG_VER, &reg_value))) {
		dev_err(&i2c_client->dev,
			"%s Could not read BBQX0KBD Software Version. Error: %d\n", __func__, rc);
		return;
	}
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