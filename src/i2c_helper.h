/* SPDX-License-Identifier: GPL-2.0 */

#ifndef I2C_HELPER_H_
#define I2C_HELPER_H_

#include <linux/i2c.h>

#include "config.h"
#include "registers.h"
#include "debug_levels.h"

// Parse 0 to 255 from string
static inline int parse_u8(char const* buf)
{
	int rc, result;

    // Parse string value
	if ((rc = kstrtoint(buf, 10, &result)) || (result < 0) || (result > 0xff)) {
		return -EINVAL;
	}
	return result;
}

// Read a single uint8_t value from I2C register
static inline int kbd_read_i2c_u8(struct i2c_client* i2c_client, uint8_t reg_addr,
	uint8_t* dst)
{
	int reg_value;

	// Read value over I2C
	if ((reg_value = i2c_smbus_read_byte_data(i2c_client, reg_addr)) < 0) {
		dev_err(&i2c_client->dev,
			"%s Could not read from register 0x%02X, error: %d\n",
			__func__, reg_addr, reg_value);
		return reg_value;
	}

	// Assign result to buffer
	*dst = reg_value & 0xFF;

	return 0;
}

// Write a single uint8_t value to I2C register
static inline int kbd_write_i2c_u8(struct i2c_client* i2c_client, uint8_t reg_addr,
	uint8_t src)
{
	int rc;

	// Write value over I2C
	if ((rc = i2c_smbus_write_byte_data(i2c_client,
		reg_addr | BBQX0KBD_WRITE_MASK, src))) {

		dev_err(&i2c_client->dev,
			"%s Could not write to register 0x%02X, Error: %d\n",
			__func__, reg_addr, rc);
		return rc;
	}

	return 0;
}

// Read a pair of uint8_t values from I2C register
static inline int kbd_read_i2c_2u8(struct i2c_client* i2c_client, uint8_t reg_addr,
	uint8_t* dst)
{
	int word_value;

	// Read value over I2C
	if ((word_value = i2c_smbus_read_word_data(i2c_client, reg_addr)) < 0) {
		dev_err(&i2c_client->dev,
			"%s Could not read from register 0x%02X, error: %d\n",
			__func__, reg_addr, word_value);
		return word_value;
	}

	// Assign result to buffer
	*dst = (uint8_t)(word_value & 0xFF);
	*(dst + 1) = (uint8_t)((word_value & 0xFF00) >> 8);

	return 0;
}

#endif
