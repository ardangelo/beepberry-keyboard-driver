// SPDX-License-Identifier: GPL-2.0-only
// Input display subsystem

#include <drm/drm.h>

#include "config.h"
#include "input_iface.h"

// Display ioctl

#define DRM_SHARP_REDRAW 0x00

#define SHARP_DEVICE_PATH "/dev/dri/card0"

// Globals

static uint32_t g_mono_invert;

// Loaded from display driver

extern void sharp_memory_set_invert(int setting);
void (*__sharp_memory_set_invert)(int setting);

extern void sharp_memory_set_indicator(int idx, char c);
void (*__sharp_memory_set_indicator)(int idx, char c);

// Display helpers

static int ioctl_call_uint32(char const* path, unsigned int cmd, uint32_t value)
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
	if ((__sharp_memory_set_invert = symbol_get(sharp_memory_set_invert))) {
		__sharp_memory_set_invert(g_mono_invert);

		(void)ioctl_call_uint32(SHARP_DEVICE_PATH, 
			DRM_IO(DRM_COMMAND_BASE + DRM_SHARP_REDRAW), 0);
	}
}

// Clear display indicator
void input_display_set_indicator(int idx, char c)
{
	// Write to parameter
	if ((__sharp_memory_set_indicator = symbol_get(sharp_memory_set_indicator))) {
		__sharp_memory_set_indicator(idx, c);

		(void)ioctl_call_uint32(SHARP_DEVICE_PATH, 
			DRM_IO(DRM_COMMAND_BASE + DRM_SHARP_REDRAW), 0);
	}
}

void input_display_clear_indicator(int idx)
{
	// Write to parameter
	if ((__sharp_memory_set_indicator = symbol_get(sharp_memory_set_indicator))) {
		__sharp_memory_set_indicator(idx, '\0');

		(void)ioctl_call_uint32(SHARP_DEVICE_PATH, 
			DRM_IO(DRM_COMMAND_BASE + DRM_SHARP_REDRAW), 0);
	}
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
