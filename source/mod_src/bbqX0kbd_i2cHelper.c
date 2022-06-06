// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * bbqX0kbd_i2cHelper.c: I2C Functions C file.
 */

#include "bbqX0kbd_i2cHelper.h"

extern int bbqX0kbd_write(struct i2c_client *i2c_client, uint8_t deviceAddress, uint8_t registerAddress, const uint8_t *buffer, uint8_t bufferSize)
{
	int returnValue;

	switch (deviceAddress) {
	case BBQX0KBD_I2C_ADDRESS:
		returnValue = i2c_smbus_write_byte_data(i2c_client, registerAddress | BBQX0KBD_WRITE_MASK, *buffer);
		if (returnValue != 0) {
			dev_err(&i2c_client->dev, "%s Could not write to register 0x%02X, Error: %d\n", __func__, registerAddress, returnValue);
			return returnValue;
		}
#if (DEBUG_LEVEL & DEBUG_LEVEL_RW)
		if (bufferSize == sizeof(uint8_t))
			dev_err(&i2c_client->dev, "%s Wrote data: [0x%02X] to register 0x%02X\n", __func__, *buffer, registerAddress);
		else
			dev_err(&i2c_client->dev, "%s Wrote data: [0x%02X%02X] to register 0x%02X\n", __func__, *buffer, *(buffer+1), registerAddress);
#endif
		break;
	// TODO: Add code for other i2c devices, BBQ20, etc.
	default:
		return 0;
	}
	return 0;
}

extern int bbqX0kbd_read(struct i2c_client *i2c_client, uint8_t deviceAddress, uint8_t registerAddress, uint8_t *buffer, uint8_t bufferSize)
{
	int returnValue;

	switch (deviceAddress) {
	case BBQX0KBD_I2C_ADDRESS:
	{
		if (bufferSize == 2*sizeof(uint8_t)) {
			returnValue = i2c_smbus_read_word_data(i2c_client, registerAddress);
			if (returnValue < 0) {
				dev_err(&i2c_client->dev, "%s Could not read from register 0x%02X, error: %d\n", __func__, registerAddress, returnValue);
				return returnValue;
			}
			*buffer = (uint8_t)(returnValue & 0xFF);
			*(buffer+1) = (uint8_t)((returnValue & 0xFF00) >> 8);
		} else {
			returnValue = i2c_smbus_read_byte_data(i2c_client, registerAddress);
			if (returnValue < 0) {
				dev_err(&i2c_client->dev, "%s Could not read from register 0x%02X, error: %d\n", __func__, registerAddress, returnValue);
				return returnValue;
			}
			*buffer = returnValue & 0xFF;
		}
#if (DEBUG_LEVEL & DEBUG_LEVEL_RW)
	if (bufferSize == sizeof(uint8_t))
		dev_err(&i2c_client->dev, "%s Read data: [0x%02X] from register 0x%02X\n", __func__, *buffer, registerAddress);
	else
		dev_err(&i2c_client->dev, "%s Read data: [0x%02X%02X]/%c  from register 0x%02X\n", __func__, *buffer, *(buffer+1), *(buffer+1), registerAddress);
#endif
	}
	break;
	// TODO: Add code for other i2c devices, BBQ20, etc.
	default:
		return 0;
	}
	return 0;
}


