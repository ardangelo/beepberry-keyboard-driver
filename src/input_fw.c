// SPDX-License-Identifier: GPL-2.0-only
// Input firmware subsystem

#include <linux/input.h>

#include "config.h"
#include "i2c_helper.h"
#include "input_iface.h"

#include "bbq20kbd_pmod_codes.h"

int input_fw_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	int rc;
	uint8_t reg_value;

	// Get firmware version
	if (kbd_read_i2c_u8(i2c_client, REG_VER, &ctx->version_number)) {
		return -ENODEV;
	}
	dev_info(&i2c_client->dev,
		"%s BBQX0KBD Software version: 0x%02X\n", __func__,
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

	// Write configuration 2
	if (kbd_write_i2c_u8(i2c_client, REG_CF2, REG_CF2_USB_KEYB_ON)) {
		return -ENODEV;
	}

	// Read back configuration 2 setting
	if (kbd_read_i2c_u8(i2c_client, REG_CF2, &reg_value)) {
		return rc;
	}
	dev_info_ld(&i2c_client->dev,
		"%s Configuration 2 Register Value: 0x%02X\n",
		__func__, reg_value);

	// Notify firmware that driver has initialized
	(void)kbd_write_i2c_u8(i2c_client, REG_DRIVER_STATE, 1);

	return 0;
}

void input_fw_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	uint8_t reg_value;

	dev_info_fe(&i2c_client->dev,
		"%s Shutting Down Keyboard.\n", __func__);

	// Notify firmware that driver has shut down
	(void)kbd_write_i2c_u8(i2c_client, REG_DRIVER_STATE, 0);

	// Read back version
	(void)kbd_read_i2c_u8(i2c_client, REG_VER, &reg_value);
}
