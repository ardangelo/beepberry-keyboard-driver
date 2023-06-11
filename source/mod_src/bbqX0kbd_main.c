// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * bbqX0kbd_main.c: Main C File.
 */

#include "bbqX0kbd_main.h"

#if (BBQX0KBD_INT == BBQX0KBD_NO_INT)
static atomic_t keepWorking = ATOMIC_INIT(1);
static struct workqueue_struct *workqueue_struct;
#endif

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

static void bbqX0kbd_set_brightness(struct bbqX0kbd_data *bbqX0kbd_data, unsigned short keycode, uint8_t *reportKey)
{
	uint8_t swapVar;

	switch (keycode) {
#if (BBQX0KBD_TYPE == BBQ10KBD_FEATHERWING)
	case KEY_Q:
		*reportKey = 1;
		if (bbqX0kbd_data->screenBrightness > 0xFF - BBQ10_BRIGHTNESS_DELTA)
			bbqX0kbd_data->screenBrightness = 0xFF;
		else
			bbqX0kbd_data->screenBrightness = bbqX0kbd_data->screenBrightness + BBQ10_BRIGHTNESS_DELTA;
		break;
	case KEY_W:
		*reportKey = 1;
		if (bbqX0kbd_data->screenBrightness < BBQ10_BRIGHTNESS_DELTA)
			bbqX0kbd_data->screenBrightness = 0;
		else
			bbqX0kbd_data->screenBrightness = bbqX0kbd_data->screenBrightness - BBQ10_BRIGHTNESS_DELTA;
		break;
	case KEY_S:
		*reportKey = 1;
		swapVar = bbqX0kbd_data->screenBrightness;
		bbqX0kbd_data->screenBrightness = (bbqX0kbd_data->screenBrightness == 0) ? bbqX0kbd_data->lastScreenBrightness : 0;
		bbqX0kbd_data->lastScreenBrightness = swapVar;
		break;
#endif
	case KEY_Z:
		*reportKey = 0;
		if (bbqX0kbd_data->keyboardBrightness > 0xFF - BBQ10_BRIGHTNESS_DELTA)
			bbqX0kbd_data->keyboardBrightness = 0xFF;
		else
			bbqX0kbd_data->keyboardBrightness = bbqX0kbd_data->keyboardBrightness + BBQ10_BRIGHTNESS_DELTA;
		break;
	case KEY_X:
		*reportKey = 0;
		if (bbqX0kbd_data->keyboardBrightness < BBQ10_BRIGHTNESS_DELTA)
			bbqX0kbd_data->keyboardBrightness = 0;
		else
			bbqX0kbd_data->keyboardBrightness = bbqX0kbd_data->keyboardBrightness - BBQ10_BRIGHTNESS_DELTA;
		break;
	case KEY_0:
		*reportKey = 0;
		swapVar = bbqX0kbd_data->keyboardBrightness;
		bbqX0kbd_data->keyboardBrightness = (bbqX0kbd_data->keyboardBrightness == 0) ? bbqX0kbd_data->lastKeyboardBrightness : 0;
		bbqX0kbd_data->lastKeyboardBrightness = swapVar;
		break;
	default:
		*reportKey = 2;
		break;
	}
	if (*reportKey == 0)
		bbqX0kbd_write(bbqX0kbd_data->i2c_client, BBQX0KBD_I2C_ADDRESS, REG_BKL, &bbqX0kbd_data->keyboardBrightness, sizeof(uint8_t));

#if (BBQX0KBD_TYPE == BBQ10KBD_FEATHERWING)
	if (*reportKey == 1)
		bbqX0kbd_write(bbqX0kbd_data->i2c_client, BBQX0KBD_I2C_ADDRESS, REG_BK2, &bbqX0kbd_data->screenBrightness, sizeof(uint8_t));
#endif
}

#if (BBQX0KBD_INT == BBQX0KBD_NO_INT)
static void bbqX0kbd_work_handler(struct work_struct *work_struct)
{
	struct bbqX0kbd_data *bbqX0kbd_data;
	struct i2c_client *i2c_client;
	uint8_t fifoData[2];
	uint8_t registerValue;
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
	uint8_t registerX, registerY;
#endif
	uint8_t count;
	uint8_t reportKey = 2;
	int returnValue;
	unsigned short keycode;

	if (atomic_read(&keepWorking) == 1) {
#if (DEBUG_LEVEL & DEBUG_LEVEL_FE)
		pr_info("%s Done with Queue.\n", __func__);
#endif
		return;
	}

#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
		pr_info("%s Doing Queue now.\n", __func__);
#endif
	bbqX0kbd_data = container_of(work_struct, struct bbqX0kbd_data, delayed_work.work);
	i2c_client = bbqX0kbd_data->i2c_client;

	returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_INT, &registerValue, sizeof(uint8_t));
	if (returnValue < 0) {
		dev_err(&i2c_client->dev, "%s : Could not read REG_INT. Error: %d\n", __func__, returnValue);
		return;
	}
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
	dev_info(&i2c_client->dev, "%s Interrupt: 0x%02x\n", __func__, registerValue);
#endif
	if (registerValue == 0x00) {
		queue_delayed_work(bbqX0kbd_data->workqueue_struct, &bbqX0kbd_data->delayed_work, msecs_to_jiffies(bbqX0kbd_data->work_rate_ms));
		return;
	}

	if (registerValue & REG_INT_OVERFLOW)
		dev_warn(&i2c_client->dev, "%s overflow occurred.\n", __func__);

	if (registerValue & REG_INT_KEY) {
		returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_KEY, &count, sizeof(uint8_t));

		if (returnValue != 0) {
			dev_err(&i2c_client->dev, "%s Could not read REG_KEY, Error: %d\n", __func__, returnValue);
			queue_delayed_work(bbqX0kbd_data->workqueue_struct, &bbqX0kbd_data->delayed_work, msecs_to_jiffies(bbqX0kbd_data->work_rate_ms));
			return;
		}

		count = count & REG_KEY_KEYCOUNT_MASK;
		while (count) {
			returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_FIF, fifoData, 2*sizeof(uint8_t));
			if (returnValue != 0) {
				dev_err(&i2c_client->dev, "%s Could not read REG_FIF, Error: %d\n", __func__, returnValue);
				queue_delayed_work(bbqX0kbd_data->workqueue_struct, &bbqX0kbd_data->delayed_work, msecs_to_jiffies(bbqX0kbd_data->work_rate_ms));
				return;
			}
			if (fifoData[0] == KEY_PRESSED_STATE || fifoData[0] == KEY_RELEASED_STATE) {
				input_event(bbqX0kbd_data->input_dev, EV_MSC, MSC_SCAN, fifoData[1]);

				keycode = bbqX0kbd_data->keycode[fifoData[1]];
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
				dev_info(&i2c_client->dev, "%s BEFORE: MODKEYS: 0x%02X LOCKKEYS: 0x%02X scancode: %d(%c) keycode: %d State: %d reportKey: %d\n", __func__, bbqX0kbd_data->modifier_keys_status, bbqX0kbd_data->lockStatus, fifoData[1], fifoData[1], keycode, fifoData[0], reportKey);
#endif
				switch (keycode) {
				case KEY_UNKNOWN:
					dev_warn(&i2c_client->dev, "%s Could not get Keycode for Scancode: [0x%02X]\n", __func__, fifoData[1]);
					break;
				case KEY_RIGHTSHIFT:
					if (bbqX0kbd_data->modifier_keys_status & LEFT_ALT_BIT && fifoData[0] == KEY_PRESSED_STATE)
						bbqX0kbd_data->lockStatus ^= NUMS_LOCK_BIT;
					fallthrough;
				case KEY_LEFTSHIFT:
				case KEY_RIGHTALT:
				case KEY_LEFTALT:
				case KEY_LEFTCTRL:
				case KEY_RIGHTCTRL:
					if (fifoData[0] == KEY_PRESSED_STATE)
						bbqX0kbd_data->modifier_keys_status |= bbqX0kbd_modkeys_to_bits(keycode);
					else
						bbqX0kbd_data->modifier_keys_status &= ~bbqX0kbd_modkeys_to_bits(keycode);
					fallthrough;
				default:
					if (bbqX0kbd_data->lockStatus & NUMS_LOCK_BIT)
						keycode = bbqX0kbd_get_num_lock_keycode(keycode);
					else if (bbqX0kbd_data->modifier_keys_status & RIGHT_ALT_BIT)
						keycode = bbqX0kbd_get_altgr_keycode(keycode);
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)
					else if (bbqX0kbd_data->modifier_keys_status & LEFT_ALT_BIT && fifoData[1] == 0x05)
						keycode = BTN_RIGHT;
#endif
#endif
					if (bbqX0kbd_data->modifier_keys_status & RIGHT_ALT_BIT && fifoData[0] == KEY_PRESSED_STATE)
						bbqX0kbd_set_brightness(bbqX0kbd_data, keycode, &reportKey);
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
					dev_info(&i2c_client->dev, "%s AFTER : MODKEYS: 0x%02X LOCKKEYS: 0x%02X scancode: %d(%c) keycode: %d State: %d reportKey: %d\n", __func__, bbqX0kbd_data->modifier_keys_status, bbqX0kbd_data->lockStatus, fifoData[1], fifoData[1],  keycode, fifoData[0], reportKey);
#endif
					if (reportKey == 2)
						input_report_key(bbqX0kbd_data->input_dev, keycode, fifoData[0] == KEY_PRESSED_STATE);
					break;
				}
			}
			--count;
		}
		input_sync(bbqX0kbd_data->input_dev);
		registerValue = 0x00;
		returnValue = bbqX0kbd_write(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_INT, &registerValue, sizeof(uint8_t));
		if (returnValue < 0)
			dev_err(&i2c_client->dev, "%s Could not clear REG_INT. Error: %d\n", __func__, returnValue);
	}

#if (BBQX0KBD_TYPE ==  BBQ20KBD_PMOD)
	if (registerValue & REG_INT_TOUCH) {

		returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_TOX, &registerX, sizeof(uint8_t));
		if (returnValue < 0) {
			dev_err(&i2c_client->dev, "%s : Could not read REG_TOX. Error: %d\n", __func__, returnValue);
			return;
		}
		returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_TOY, &registerY, sizeof(uint8_t));
		if (returnValue < 0) {
			dev_err(&i2c_client->dev, "%s : Could not read REG_TOY. Error: %d\n", __func__, returnValue);
			return;
		}
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
		dev_info(&i2c_client->dev, "%s X Reg: %d Y Reg: %d.\n", __func__, (int8_t)registerX, (int8_t)registerY);
#endif
#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)
		input_report_rel(bbqX0kbd_data->input_dev, REL_X, (int8_t)registerX);
		input_report_rel(bbqX0kbd_data->input_dev, REL_Y, (int8_t)registerY);
		input_sync(bbqX0kbd_data->input_dev);
		registerValue = 0x00;
		returnValue = bbqX0kbd_write(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_INT, &registerValue, sizeof(uint8_t));
		if (returnValue < 0)
			dev_err(&i2c_client->dev, "%s Could not clear REG_INT. Error: %d\n", __func__, returnValue);
#endif
	}

#endif

	queue_delayed_work(bbqX0kbd_data->workqueue_struct, &bbqX0kbd_data->delayed_work, msecs_to_jiffies(bbqX0kbd_data->work_rate_ms));
}

static int bbqX0kbd_queue_work(struct bbqX0kbd_data *bbqX0kbd_data)
{
	INIT_DELAYED_WORK(&bbqX0kbd_data->delayed_work, bbqX0kbd_work_handler);
	atomic_set(&keepWorking, 0);
	return queue_delayed_work(bbqX0kbd_data->workqueue_struct, &bbqX0kbd_data->delayed_work, msecs_to_jiffies(bbqX0kbd_data->work_rate_ms));
}
#endif


#if (BBQX0KBD_INT == BBQX0KBD_USE_INT)
static void bbqX0kbd_read_fifo(struct bbqX0kbd_data *bbqX0kbd_data)
{
	struct i2c_client *i2c_client = bbqX0kbd_data->i2c_client;
	uint8_t fifo_data[2];
	uint8_t count;
	uint8_t pos;
	int returnValue;

	returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_KEY, &count, sizeof(uint8_t));
	if (returnValue != 0) {
		dev_err(&i2c_client->dev, "%s Could not read REG_KEY, Error: %d\n", __func__, returnValue);
		return;
	}
	count = count & REG_KEY_KEYCOUNT_MASK;
	bbqX0kbd_data->fifoCount = count;
	pos = 0;
	while (count) {
		returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_FIF, fifo_data, 2*sizeof(uint8_t));
		if (returnValue != 0) {
			dev_err(&i2c_client->dev, "%s Could not read REG_FIF, Error: %d\n", __func__, returnValue);
			return;
		}
		bbqX0kbd_data->fifoData[pos][0] = fifo_data[0];
		bbqX0kbd_data->fifoData[pos][1] = fifo_data[1];
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
		dev_info(&i2c_client->dev, "%s Filled Data: KeyState:%d SCANCODE:%d at Pos: %d Count: %d\n",
			__func__, bbqX0kbd_data->fifoData[pos][0], bbqX0kbd_data->fifoData[pos][1], pos, count);
#endif
		++pos;
		--count;
	}
}

static void bbqX0kbd_work_fnc(struct work_struct *work_struct_ptr)
{
	struct bbqX0kbd_data *bbqX0kbd_data;
	struct input_dev *input_dev;
	struct i2c_client *i2c_client;
	unsigned short keycode;

	uint8_t pos = 0;
	uint8_t reportKey = 2;
	uint8_t registerValue = 0x00;
	int returnValue = 0;

	bbqX0kbd_data = container_of(work_struct_ptr, struct bbqX0kbd_data, work_struct);
	input_dev = bbqX0kbd_data->input_dev;
	i2c_client = bbqX0kbd_data->i2c_client;

	while (bbqX0kbd_data->fifoCount > 0) {
		if (bbqX0kbd_data->fifoData[pos][0] == KEY_PRESSED_STATE || bbqX0kbd_data->fifoData[pos][0] == KEY_RELEASED_STATE) {
			input_event(input_dev, EV_MSC, MSC_SCAN, bbqX0kbd_data->fifoData[pos][1]);

			keycode = bbqX0kbd_data->keycode[bbqX0kbd_data->fifoData[pos][1]];
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
				dev_info(&i2c_client->dev, "%s BEFORE: MODKEYS: 0x%02X LOCKKEYS: 0x%02X scancode: %d(%c) keycode: %d State: %d reportKey: %d\n", __func__, bbqX0kbd_data->modifier_keys_status, bbqX0kbd_data->lockStatus, bbqX0kbd_data->fifoData[pos][1], bbqX0kbd_data->fifoData[pos][1], keycode, bbqX0kbd_data->fifoData[pos][0], reportKey);
#endif
			switch (keycode) {
			case KEY_UNKNOWN:
				dev_warn(&i2c_client->dev, "%s Could not get Keycode for Scancode: [0x%02X]\n", __func__, bbqX0kbd_data->fifoData[pos][1]);
				break;
			case KEY_RIGHTSHIFT:
				if (bbqX0kbd_data->modifier_keys_status & LEFT_ALT_BIT && bbqX0kbd_data->fifoData[pos][0] == KEY_PRESSED_STATE)
					bbqX0kbd_data->lockStatus ^= NUMS_LOCK_BIT;
				fallthrough;
			case KEY_LEFTSHIFT:
			case KEY_RIGHTALT:
			case KEY_LEFTALT:
			case KEY_LEFTCTRL:
			case KEY_RIGHTCTRL:
				if (bbqX0kbd_data->fifoData[pos][0] == KEY_PRESSED_STATE)
					bbqX0kbd_data->modifier_keys_status |= bbqX0kbd_modkeys_to_bits(keycode);
				else
					bbqX0kbd_data->modifier_keys_status &= ~bbqX0kbd_modkeys_to_bits(keycode);
				fallthrough;
			default:
				if (bbqX0kbd_data->lockStatus & NUMS_LOCK_BIT)
					keycode = bbqX0kbd_get_num_lock_keycode(keycode);
				else if (bbqX0kbd_data->modifier_keys_status & RIGHT_ALT_BIT)
					keycode = bbqX0kbd_get_altgr_keycode(keycode);
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)
				else if (bbqX0kbd_data->modifier_keys_status & LEFT_ALT_BIT && bbqX0kbd_data->fifoData[pos][1] == 0x05)
					keycode = BTN_RIGHT;
#endif
#endif
				if (bbqX0kbd_data->modifier_keys_status & RIGHT_ALT_BIT && bbqX0kbd_data->fifoData[pos][0] == KEY_PRESSED_STATE)
					bbqX0kbd_set_brightness(bbqX0kbd_data, keycode, &reportKey);
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
				dev_info(&i2c_client->dev, "%s BEFORE: MODKEYS: 0x%02X LOCKKEYS: 0x%02X scancode: %d(%c) keycode: %d State: %d reportKey: %d\n", __func__, bbqX0kbd_data->modifier_keys_status, bbqX0kbd_data->lockStatus, bbqX0kbd_data->fifoData[pos][1], bbqX0kbd_data->fifoData[pos][1], keycode, bbqX0kbd_data->fifoData[pos][0], reportKey);
#endif
				if (reportKey == 2)
					input_report_key(input_dev, keycode, bbqX0kbd_data->fifoData[pos][0] == KEY_PRESSED_STATE);
				break;
			}
		}
		++pos;
		--bbqX0kbd_data->fifoCount;
	}

#if (BBQX0KBD_TYPE ==  BBQ20KBD_PMOD)
	if (bbqX0kbd_data->touchInt) {
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
		dev_info(&i2c_client->dev, "%s X Reg: %d Y Reg: %d.\n", __func__, bbqX0kbd_data->rel_x, bbqX0kbd_data->rel_y);
#endif
#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)
		input_report_rel(input_dev, REL_X, bbqX0kbd_data->rel_x);
		input_report_rel(input_dev, REL_Y, bbqX0kbd_data->rel_y);
		bbqX0kbd_data->touchInt = 0;
#endif
#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_KEYS)
		if(bbqX0kbd_data->rel_x < -4){
			input_report_key(input_dev, KEY_LEFT, TRUE);
			input_report_key(input_dev, KEY_LEFT, FALSE);
		}
		if(bbqX0kbd_data->rel_x > 4){
			input_report_key(input_dev, KEY_RIGHT, TRUE);
			input_report_key(input_dev, KEY_RIGHT, FALSE);
		}
		if(bbqX0kbd_data->rel_y < -4){
			input_report_key(input_dev, KEY_UP, TRUE);
			input_report_key(input_dev, KEY_UP, FALSE);
		}
		if(bbqX0kbd_data->rel_y > 4){
			input_report_key(input_dev, KEY_DOWN, TRUE);
			input_report_key(input_dev, KEY_DOWN, FALSE);
		}						
		bbqX0kbd_data->touchInt = 0;
#endif
	}
#endif

	input_sync(input_dev);
	registerValue = 0x00;
	returnValue = bbqX0kbd_write(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_INT, &registerValue, sizeof(uint8_t));
	if (returnValue < 0)
		dev_err(&i2c_client->dev, "%s Could not clear REG_INT. Error: %d\n", __func__, returnValue);
}

static irqreturn_t bbqX0kbd_irq_handler(int irq, void *dev_id)
{
	struct bbqX0kbd_data *bbqX0kbd_data = dev_id;
	struct i2c_client *client = bbqX0kbd_data->i2c_client;
	int returnValue;
	uint8_t registerValue;

#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
	dev_info(&client->dev, "%s Interrupt Fired. IRQ: %d\n", __func__, irq);
#endif

	returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_INT, &registerValue, sizeof(uint8_t));
	if (returnValue < 0) {
		dev_err(&client->dev, "%s: Could not read REG_INT. Error: %d\n", __func__, returnValue);
		return IRQ_NONE;
	}

#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
	dev_info(&client->dev, "%s Interrupt: 0x%02x\n", __func__, registerValue);
#endif

	if (registerValue == 0x00)
		return IRQ_NONE;

	if (registerValue & REG_INT_OVERFLOW)
		dev_warn(&client->dev, "%s overflow occurred.\n", __func__);

	if (registerValue & REG_INT_KEY) {
		bbqX0kbd_read_fifo(bbqX0kbd_data);
		schedule_work(&bbqX0kbd_data->work_struct);
	}
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
	if (registerValue & REG_INT_TOUCH) {
		returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_TOX, &registerValue, sizeof(uint8_t));
		if (returnValue < 0) {
			dev_err(&client->dev, "%s : Could not read REG_TOX. Error: %d\n", __func__, returnValue);
			return IRQ_NONE;
		}
		bbqX0kbd_data->rel_x = (int8_t)registerValue;
		returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_TOY, &registerValue, sizeof(uint8_t));
		if (returnValue < 0) {
			dev_err(&client->dev, "%s : Could not read REG_TOY. Error: %d\n", __func__, returnValue);
			return IRQ_NONE;
		}
		bbqX0kbd_data->rel_y = (int8_t)registerValue;
		bbqX0kbd_data->touchInt = 1;
		schedule_work(&bbqX0kbd_data->work_struct);
	} else
		bbqX0kbd_data->touchInt = 0;
#endif

	return IRQ_HANDLED;
}
#endif



static int bbqX0kbd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct bbqX0kbd_data *bbqX0kbd_data;
	struct input_dev *input;
	int returnValue;
	int i;
	uint8_t registerValue = 0x00;

#if (DEBUG_LEVEL & DEBUG_LEVEL_FE)
	dev_info(&client->dev, "%s Probing BBQX0KBD.\n", __func__);
#endif
	bbqX0kbd_data = devm_kzalloc(dev, sizeof(*bbqX0kbd_data), GFP_KERNEL);
	if (!bbqX0kbd_data)
		return -ENOMEM;
	bbqX0kbd_data->i2c_client = client;
	memcpy(bbqX0kbd_data->keycode, keycodes, sizeof(bbqX0kbd_data->keycode));
	
	//don't reset the keyboard as it controls the pi's power
	returnValue = 0; //bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_RST, &registerValue, sizeof(uint8_t));
	if (returnValue) {
		dev_err(&client->dev, "%s Could not Reset BBQX0KBD. Error: %d\n", __func__, returnValue);
		return -ENODEV;
	}
	msleep(400);

	returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_VER, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		dev_err(&client->dev, "%s Could not Read Version BBQX0KBD. Error: %d\n", __func__, returnValue);
		return -ENODEV;
	}
	dev_info(&client->dev, "%s BBQX0KBD Software version: 0x%02X\n", __func__, registerValue);
	bbqX0kbd_data->version_number = registerValue;

	registerValue = REG_CFG_DEFAULT_SETTING;
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_CFG, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		dev_err(&client->dev, "%s Could not write configuration to BBQX0KBD. Error: %d\n", __func__, returnValue);
		return -ENODEV;
	}
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
	returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_CFG, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		dev_err(&client->dev, "%s Could not read REG_CFG. Error: %d\n", __func__, returnValue);
		return returnValue;
	}
	dev_info(&client->dev, "%s Configuration Register Value: 0x%02X\n", __func__, registerValue);
#endif

#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
	registerValue = REG_CFG2_DEFAULT_SETTING;
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_CF2, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		dev_err(&client->dev, "%s Could not write configuration 2 to BBQX0KBD. Error: %d\n", __func__, returnValue);
		return -ENODEV;
	}
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
	returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_CF2, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		dev_err(&client->dev, "%s Could not read REG_CF2. Error: %d\n", __func__, returnValue);
		return returnValue;
	}
	dev_info(&client->dev, "%s Configuration 2 Register Value: 0x%02X\n", __func__, registerValue);
#endif
#endif


#if (BBQX0KBD_INT == BBQX0KBD_NO_INT)
	if (BBQX0KBD_POLL_PERIOD < BBQX0KBD_MINIMUM_WORK_RATE || BBQX0KBD_POLL_PERIOD > BBQX0KBD_MAXIMUM_WORK_RATE)
		bbqX0kbd_data->work_rate_ms = BBQX0KBD_DEFAULT_WORK_RATE;
	else
		bbqX0kbd_data->work_rate_ms = BBQX0KBD_POLL_PERIOD;
#endif

	input = devm_input_allocate_device(dev);
	if (!input) {
		dev_err(&client->dev, "%s Could not devm_input_allocate_device BBQX0KBD. Error: %d\n", __func__, returnValue);
		return -ENOMEM;
	}
	bbqX0kbd_data->input_dev = input;

	input->name = client->name;
	input->id.bustype = BBQX0KBD_BUS_TYPE;
	input->id.vendor  = BBQX0KBD_VENDOR_ID;
	input->id.product = BBQX0KBD_PRODUCT_ID;
	input->id.version = BBQX0KBD_VERSION_ID;
	input->keycode = bbqX0kbd_data->keycode;
	input->keycodesize = sizeof(bbqX0kbd_data->keycode[0]);
	input->keycodemax = ARRAY_SIZE(bbqX0kbd_data->keycode);

	for (i = 0; i < NUM_KEYCODES; i++)
		__set_bit(bbqX0kbd_data->keycode[i], input->keybit);

	__clear_bit(KEY_RESERVED, input->keybit);

	__set_bit(EV_REP, input->evbit);
	__set_bit(EV_KEY, input->evbit);

	input_set_capability(input, EV_MSC, MSC_SCAN);
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD && BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)
	input_set_capability(input, EV_REL, REL_X);
	input_set_capability(input, EV_REL, REL_Y);
#endif

	bbqX0kbd_data->modifier_keys_status = 0x00; // Serendipitously coincides with idle state of all keys.
	bbqX0kbd_data->lockStatus = 0x00;
#if (BBQX0KBD_TYPE == BBQ10KBD_FEATHERWING)
	bbqX0kbd_data->screenBrightness = 0xFF;
	bbqX0kbd_data->lastScreenBrightness = bbqX0kbd_data->screenBrightness;
	bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_BK2, &bbqX0kbd_data->screenBrightness, sizeof(uint8_t));
#endif
	bbqX0kbd_data->keyboardBrightness = 0xFF;
	bbqX0kbd_data->lastKeyboardBrightness = bbqX0kbd_data->keyboardBrightness;
	bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_BKL, &bbqX0kbd_data->keyboardBrightness, sizeof(uint8_t));

#if (BBQX0KBD_INT == BBQX0KBD_USE_INT)
	returnValue = devm_request_threaded_irq(dev, client->irq,
										NULL, bbqX0kbd_irq_handler,
										IRQF_SHARED | IRQF_ONESHOT,
										client->name, bbqX0kbd_data);

	if (returnValue != 0) {
		dev_err(&client->dev, "Coudl not claim IRQ %d; error %d\n", client->irq, returnValue);
		return returnValue;
	}
	INIT_WORK(&bbqX0kbd_data->work_struct, bbqX0kbd_work_fnc);
#endif

#if (BBQX0KBD_INT == BBQX0KBD_NO_INT)
	bbqX0kbd_data->workqueue_struct = create_singlethread_workqueue("bbqX0kbd_workqueue");
	if (bbqX0kbd_data->workqueue_struct == NULL) {
		dev_err(dev, "%s Could not create_singlethreaded_workqueue.", __func__);
		return -ENOMEM;
	}
	workqueue_struct = bbqX0kbd_data->workqueue_struct;
	bbqX0kbd_queue_work(bbqX0kbd_data);
#endif
	returnValue = input_register_device(input);
	if (returnValue != 0) {
		dev_err(dev, "Failed to register input device, error: %d\n", returnValue);
		return returnValue;
	}

	return 0;
}

static void bbqX0kbd_shutdown(struct i2c_client *client)
{
	int returnValue;
	uint8_t registerValue = 0x00;
#if (DEBUG_LEVEL & DEBUG_LEVEL_FE)
	dev_info(&client->dev, "%s Shutting Down Keyboard And Screen Backlight.\n", __func__);
#endif
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_BKL, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		dev_err(&client->dev, "%s Could not write to BBQX0KBD Backlight. Error: %d\n", __func__, returnValue);
		return;
	}
	returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_VER, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		dev_err(&client->dev, "%s Could not read BBQX0KBD Software Version. Error: %d\n", __func__, returnValue);
		return;
	}
#if (BBQX0KBD_TYPE == BBQ10KBD_FEATHERWING)
	registerValue = 0x00;
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_BK2, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		dev_err(&client->dev, "%s Could not write to BBQX0KBD Screen Backlight. Error: %d\n", __func__, returnValue);
		return;
	}
#endif

}

static struct i2c_driver bbqX0kbd_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.of_match_table = bbqX0kbd_of_device_id,
	},
	.probe		= bbqX0kbd_probe,
	.shutdown	= bbqX0kbd_shutdown,
	.id_table	= bbqX0kbd_i2c_device_id,
};

static int __init bbqX0kbd_init(void)
{
	int returnValue;

	returnValue = i2c_add_driver(&bbqX0kbd_driver);
	if (returnValue != 0) {
		pr_err("%s Could not initialise BBQX0KBD driver! Error: %d\n", __func__, returnValue);
		return returnValue;
	}
	pr_info("%s Initalised BBQX0KBD.\n", __func__);
	return returnValue;
}
module_init(bbqX0kbd_init);

static void __exit bbqX0kbd_exit(void)
{
	pr_info("%s Exiting BBQX0KBD.\n", __func__);
#if (BBQX0KBD_INT == BBQX0KBD_NO_INT)
	atomic_set(&keepWorking, 1);
	flush_workqueue(workqueue_struct);
	msleep(500);
	destroy_workqueue(workqueue_struct);
#endif
	i2c_del_driver(&bbqX0kbd_driver);
}
module_exit(bbqX0kbd_exit);
