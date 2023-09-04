// SPDX-License-Identifier: GPL-2.0-only
// Input firmware subsystem

#include "config.h"
#include "i2c_helper.h"
#include "input_iface.h"

// Power helpers

void input_fw_run_poweroff(struct kbd_ctx* ctx)
{
	// Set LED to red
	kbd_write_i2c_u8(ctx->i2c_client, REG_LED_R, 0xff);
	kbd_write_i2c_u8(ctx->i2c_client, REG_LED_G, 0x0);
	kbd_write_i2c_u8(ctx->i2c_client, REG_LED_B, 0x0);
	kbd_write_i2c_u8(ctx->i2c_client, REG_LED, 0x1);

	// Run poweroff
	static const char * const poweroff_argv[] = {
		"/sbin/poweroff", "now", NULL };
	call_usermodehelper(poweroff_argv[0], (char**)poweroff_argv, NULL, UMH_NO_WAIT);
}

// Brightness helpers

void input_fw_decrease_brightness(struct kbd_ctx* ctx)
{
	// Decrease by delta, min at 0x0 brightness
	ctx->fw_brightness = (ctx->fw_brightness < BBQ10_BRIGHTNESS_DELTA)
		? 0x0
		: ctx->fw_brightness - BBQ10_BRIGHTNESS_DELTA;

	// Set backlight using I2C
	(void)kbd_write_i2c_u8(ctx->i2c_client, REG_BKL, ctx->fw_brightness);
}

void input_fw_increase_brightness(struct kbd_ctx* ctx)
{
	// Increase by delta, max at 0xff brightness
	ctx->fw_brightness = (ctx->fw_brightness > (0xff - BBQ10_BRIGHTNESS_DELTA))
		? 0xff
		: ctx->fw_brightness + BBQ10_BRIGHTNESS_DELTA;

	// Set backlight using I2C
	(void)kbd_write_i2c_u8(ctx->i2c_client, REG_BKL, ctx->fw_brightness);
}

void input_fw_toggle_brightness(struct kbd_ctx* ctx)
{
	// Toggle, save last brightness in context
	if (ctx->fw_last_brightness) {
		ctx->fw_brightness = ctx->fw_last_brightness;
		ctx->fw_last_brightness = 0;
	} else {
		ctx->fw_last_brightness = ctx->fw_brightness;
		ctx->fw_brightness = 0;
	}

	// Set backlight using I2C
	(void)kbd_write_i2c_u8(ctx->i2c_client, REG_BKL, ctx->fw_brightness);
}

// I2C helpers

int input_fw_enable_touch_interrupts(struct kbd_ctx* ctx)
{
	int rc;
	uint8_t reg_value;

	// Get old CF2 value
	if ((rc = kbd_read_i2c_u8(ctx->i2c_client, REG_CF2, &reg_value))) {
		return rc;
	}

	// Set touch interrupt bit
	reg_value |= REG_CF2_TOUCH_INT;

	// Write new CF2 value
	if ((rc = kbd_write_i2c_u8(ctx->i2c_client, REG_CF2, reg_value))) {
		return rc;
	}

	return 0;
}

int input_fw_disable_touch_interrupts(struct kbd_ctx* ctx)
{
	int rc;
	uint8_t reg_value;

	// Get old CF2 value
	if ((rc = kbd_read_i2c_u8(ctx->i2c_client, REG_CF2, &reg_value))) {
		return rc;
	}

	// Clear touch interrupt bit
	reg_value &= ~REG_CF2_TOUCH_INT;

	// Write new CF2 value
	if ((rc = kbd_write_i2c_u8(ctx->i2c_client, REG_CF2, reg_value))) {
		return rc;
	}

	return 0;
}

// Transfer from I2C FIFO to internal context FIFO
void input_fw_read_fifo(struct kbd_ctx* ctx)
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
			"%s %02d: 0x%02x%02x State %d Scancode %d\n",
			__func__, fifo_idx,
			((uint8_t*)&ctx->fifo_data[fifo_idx])[0],
			((uint8_t*)&ctx->fifo_data[fifo_idx])[1],
			ctx->fifo_data[fifo_idx].state,
			ctx->fifo_data[fifo_idx].scancode);
	}
}

// RTC helpers

int input_fw_get_rtc(uint8_t* year, uint8_t* mon, uint8_t* day,
	uint8_t* hour, uint8_t* min, uint8_t* sec)
{
	int rc;

	if (!g_ctx || !g_ctx->i2c_client) {
		return -EAGAIN;
	}

	if ((rc = kbd_read_i2c_u8(g_ctx->i2c_client, REG_RTC_YEAR, year))) {
		return rc;
	}
	if ((rc = kbd_read_i2c_u8(g_ctx->i2c_client, REG_RTC_MON, mon))) {
		return rc;
	}
	if ((rc = kbd_read_i2c_u8(g_ctx->i2c_client, REG_RTC_MDAY, day))) {
		return rc;
	}
	if ((rc = kbd_read_i2c_u8(g_ctx->i2c_client, REG_RTC_HOUR, hour))) {
		return rc;
	}
	if ((rc = kbd_read_i2c_u8(g_ctx->i2c_client, REG_RTC_MIN, min))) {
		return rc;
	}
	if ((rc = kbd_read_i2c_u8(g_ctx->i2c_client, REG_RTC_SEC, sec))) {
		return rc;
	}

	return 0;
}

int input_fw_set_rtc(uint8_t year, uint8_t mon, uint8_t day,
	uint8_t hour, uint8_t min, uint8_t sec)
{
	int rc;

	if (!g_ctx || !g_ctx->i2c_client) {
		return -EAGAIN;
	}

	if ((rc = kbd_write_i2c_u8(g_ctx->i2c_client, REG_RTC_YEAR, year))) {
		return rc;
	}
	if ((rc = kbd_write_i2c_u8(g_ctx->i2c_client, REG_RTC_MON, mon))) {
		return rc;
	}
	if ((rc = kbd_write_i2c_u8(g_ctx->i2c_client, REG_RTC_MDAY, day))) {
		return rc;
	}
	if ((rc = kbd_write_i2c_u8(g_ctx->i2c_client, REG_RTC_HOUR, hour))) {
		return rc;
	}
	if ((rc = kbd_write_i2c_u8(g_ctx->i2c_client, REG_RTC_MIN, min))) {
		return rc;
	}
	if ((rc = kbd_write_i2c_u8(g_ctx->i2c_client, REG_RTC_SEC, sec))) {
		return rc;
	}
	if ((rc = kbd_write_i2c_u8(g_ctx->i2c_client, REG_RTC_COMMIT, 0x1))) {
		return rc;
	}

	return 0;
}

int input_fw_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	int rc;
	uint8_t reg_value;

	// Initialize keyboard context
	ctx->fw_brightness = 0xFF;
	ctx->fw_last_brightness = 0xFF;
	ctx->fw_handle_poweroff = 0;

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

	// Read back configuration 2 setting
	if (kbd_read_i2c_u8(i2c_client, REG_CF2, &reg_value)) {
		return rc;
	}
	dev_info_ld(&i2c_client->dev,
		"%s Configuration 2 Register Value: 0x%02X\n",
		__func__, reg_value);

	// Update keyboard brightness
	(void)kbd_write_i2c_u8(i2c_client, REG_BKL, ctx->fw_brightness);

	// Notify firmware that driver has initialized
	// Clear boot indicator LED
	(void)kbd_write_i2c_u8(i2c_client, REG_LED, 0);
	(void)kbd_write_i2c_u8(i2c_client, REG_DRIVER_STATE, 1);

	return 0;
}

void input_fw_shutdown(struct i2c_client* i2c_client)
{
	uint8_t reg_value;

	dev_info_fe(&i2c_client->dev,
		"%s Shutting Down Keyboard And Screen Backlight.\n", __func__);

	// Turn off LED and notify firmware that driver has shut down
	(void)kbd_write_i2c_u8(i2c_client, REG_LED, 0);
	(void)kbd_write_i2c_u8(i2c_client, REG_DRIVER_STATE, 0);

	// Turn off backlight
	(void)kbd_write_i2c_u8(i2c_client, REG_BKL, 0);

	// Reenable touch events
	(void)kbd_write_i2c_u8(i2c_client, REG_CF2, REG_CFG2_DEFAULT_SETTING);

	// Read back version
	(void)kbd_read_i2c_u8(i2c_client, REG_VER, &reg_value);
}

