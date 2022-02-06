// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * bbqX0kbd_main.c: Main C File.
 */

#include "bbqX0kbd_main.h"


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
		returnValue = KEY_PAGEUP;
		break;
	case KEY_R:
		returnValue = KEY_PAGEDOWN;
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
	case KEY_Z:
		returnValue = KEY_VOLUMEUP;
		break;
	case KEY_X:
		returnValue = KEY_VOLUMEDOWN;
		break;
	case KEY_0:
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
	case KEY_K:
		*reportKey = 0;
		if (bbqX0kbd_data->keyboardBrightness > 0xFF - BBQ10_BRIGHTNESS_DELTA)
			bbqX0kbd_data->keyboardBrightness = 0xFF;
		else
			bbqX0kbd_data->keyboardBrightness = bbqX0kbd_data->keyboardBrightness + BBQ10_BRIGHTNESS_DELTA;
		break;
	case KEY_L:
		*reportKey = 0;
		if (bbqX0kbd_data->keyboardBrightness < BBQ10_BRIGHTNESS_DELTA)
			bbqX0kbd_data->keyboardBrightness = 0;
		else
			bbqX0kbd_data->keyboardBrightness = bbqX0kbd_data->keyboardBrightness - BBQ10_BRIGHTNESS_DELTA;
		break;
	case KEY_GRAVE:
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

	if (*reportKey == 1 && bbqX0kbd_data->version_number == BBQ10_I2C_SW_VERSION)
		bbqX0kbd_write(bbqX0kbd_data->i2c_client, BBQX0KBD_I2C_ADDRESS, REG_BK2, &bbqX0kbd_data->screenBrightness, sizeof(uint8_t));

}

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
				else if (bbqX0kbd_data->modifier_keys_status & RIGHT_ALT_BIT) {
					keycode = bbqX0kbd_get_altgr_keycode(keycode);
					if (bbqX0kbd_data->fifoData[pos][0] == KEY_PRESSED_STATE)
						bbqX0kbd_set_brightness(bbqX0kbd_data, keycode, &reportKey);
				}
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
				dev_info(&i2c_client->dev, "%s MODKEYS: 0x%02X LOCKKEYS: 0x%02X keycode: %d State: %d reportKey: %d\n", __func__, bbqX0kbd_data->modifier_keys_status, bbqX0kbd_data->lockStatus, keycode, bbqX0kbd_data->fifoData[pos][0], reportKey);
#endif
				if (reportKey == 2)
					input_report_key(input_dev, keycode, bbqX0kbd_data->fifoData[pos][0] == KEY_PRESSED_STATE);
				break;
			}
		}
		++pos;
		--bbqX0kbd_data->fifoCount;
	}
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
	return IRQ_HANDLED;
}

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

	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_RST, &registerValue, sizeof(uint8_t));
	if (returnValue) {
		dev_err(&client->dev, "%s Could not Reset BBQX0KBD. Error: %d\n", __func__, returnValue);
		return -ENODEV;
	}
	msleep(100);

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

	bbqX0kbd_data->modifier_keys_status = 0x00; // Serendipitously coincides with idle state of all keys.
	bbqX0kbd_data->lockStatus = 0x00;
	bbqX0kbd_data->screenBrightness = 0xFF;
	bbqX0kbd_data->keyboardBrightness = 0xFF;
	bbqX0kbd_data->lastScreenBrightness = bbqX0kbd_data->screenBrightness;
	bbqX0kbd_data->lastKeyboardBrightness = bbqX0kbd_data->keyboardBrightness;
	bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_BKL, &bbqX0kbd_data->keyboardBrightness, sizeof(uint8_t));
	bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_BK2, &bbqX0kbd_data->screenBrightness, sizeof(uint8_t));

	returnValue = devm_request_threaded_irq(dev, client->irq,
										NULL, bbqX0kbd_irq_handler,
										IRQF_SHARED | IRQF_ONESHOT,
										client->name, bbqX0kbd_data);

	// bbqX0kbd_data->work_struct_ptr = kmalloc(sizeof(struct work_struct), GFP_KERNEL);
	INIT_WORK(&bbqX0kbd_data->work_struct, bbqX0kbd_work_fnc);
	if (returnValue != 0) {
		dev_err(&client->dev, "Coudl not claim IRQ %d; error %d\n", client->irq, returnValue);
		return returnValue;
	}

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
	if (registerValue == BBQ10_I2C_SW_VERSION) {
		registerValue = 0x00;
		returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_BK2, &registerValue, sizeof(uint8_t));
		if (returnValue != 0) {
			dev_err(&client->dev, "%s Could not write to BBQX0KBD Screen Backlight. Error: %d\n", __func__, returnValue);
			return;
		}
	}
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
#if (DEBUG_LEVEL & DEBUG_LEVEL_FE)
	pr_info("%s Initalised BBQX0KBD.\n", __func__);
#endif
	return returnValue;
}
module_init(bbqX0kbd_init);

static void __exit bbqX0kbd_exit(void)
{
#if (DEBUG_LEVEL & DEBUG_LEVEL_FE)
	pr_info("%s Exiting BBQX0KBD.\n", __func__);
#endif
	i2c_del_driver(&bbqX0kbd_driver);
}
module_exit(bbqX0kbd_exit);


