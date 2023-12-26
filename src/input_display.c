// SPDX-License-Identifier: GPL-2.0-only
// Input display subsystem

#include "config.h"
#include "input_iface.h"

#define SHARP_DEVICE_PATH "/dev/sharp"

// Display ioctl

#define SHARP_IOC_MAGIC 0xd5
#define SHARP_IOCTQ_SET_INVERT _IOW(SHARP_IOC_MAGIC, 1, uint32_t)
#define SHARP_IOCTQ_SET_INDICATOR _IOW(SHARP_IOC_MAGIC, 2, uint32_t)

// Globals

static uint8_t g_mono_invert;

// Display helpers

static int ioctl_call_int(char const* path, unsigned int cmd, int value)
{
	struct file *filp;

	// Open file
	if (IS_ERR((filp = filp_open(path, O_WRONLY, 0)))) {
		// Silently return if display driver was not loaded
		return 0;
	}

	filp->f_op->unlocked_ioctl(filp, cmd, value);

	// Close file
	filp_close(filp, NULL);

	return 0;
}

// Invert display colors by writing to display driver parameter
void input_display_invert(struct kbd_ctx* ctx)
{
	// Update saved invert value
	g_mono_invert = (g_mono_invert) ? 0 : 1;

	// Write to parameter
	(void)ioctl_call_int(SHARP_DEVICE_PATH, SHARP_IOCTQ_SET_INVERT,
		(g_mono_invert) ? 1 : 0);
}

// Clear display indicator
void input_display_set_indicator(uint8_t idx, char c)
{
	(void)ioctl_call_int(SHARP_DEVICE_PATH, SHARP_IOCTQ_SET_INDICATOR,
		(idx << 8) | (int)c);
}

void input_display_clear_indicator(uint8_t idx)
{
	(void)ioctl_call_int(SHARP_DEVICE_PATH, SHARP_IOCTQ_SET_INDICATOR,
		(idx << 8) | (int)'\0');
}

int input_display_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	int i;

	g_mono_invert = 0;

	// Clear all indicators
	for (i = 0; i < 6; i++) {
		input_display_clear_indicator(i);
	}

	return 0;
}

void input_display_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	int i;

	// Clear all indicators
	for (i = 0; i < 6; i++) {
		input_display_clear_indicator(i);
	}
}
