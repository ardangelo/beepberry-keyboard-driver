// SPDX-License-Identifier: GPL-2.0-only
// Input display subsystem

#include <drm/drm.h>

#include "config.h"
#include "input_iface.h"

#include "indicators.h"

// Display ioctl

#define DRM_SHARP_REDRAW 0x00

#define SHARP_DEVICE_PATH "/dev/dri/card0"

// Globals

static uint32_t g_mono_invert;

// Loaded from display driver

extern void sharp_memory_set_invert(int setting);
void (*__sharp_memory_set_invert)(int setting);

extern void* sharp_memory_add_overlay(int x, int y, int width, int height,
	unsigned char const* pixels);
void* (*__sharp_memory_add_overlay)(int x, int y, int width, int height,
	unsigned char const* pixels);
extern void sharp_memory_remove_overlay(void* entry);
void (*__sharp_memory_remove_overlay)(void* entry);
extern void* sharp_memory_show_overlay(void* storage);
void* (*__sharp_memory_show_overlay)(void* storage);
extern void sharp_memory_hide_overlay(void* display);
void (*__sharp_memory_hide_overlay)(void* display);
extern void sharp_memory_clear_overlays(void);
void (*__sharp_memory_clear_overlays)(void);

struct mod_overlay_t
{
	void *storage;
	void *display;
};

struct mod_overlay_t g_mod_overlays[MAX_INDICATORS];

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

// Set display indicator
void input_display_set_indicator(int idx, unsigned char const* pixels)
{
	int x;

	if ((idx >= MAX_INDICATORS) || (pixels == NULL)) {
		return;
	}

	if (g_mod_overlays[idx].storage == NULL) {
		x = - ((idx + 1) * INDICATOR_WIDTH);

		if ((__sharp_memory_add_overlay = symbol_get(sharp_memory_add_overlay))) {
			g_mod_overlays[idx].storage = __sharp_memory_add_overlay(
				x, 0, INDICATOR_WIDTH, INDICATOR_HEIGHT, pixels);
		} else {
			return;
		}
	}

	if (g_mod_overlays[idx].display == NULL) {
		if ((__sharp_memory_show_overlay = symbol_get(sharp_memory_show_overlay))) {
			g_mod_overlays[idx].display = __sharp_memory_show_overlay(
				g_mod_overlays[idx].storage);
		} else {
			return;
		}
	}

	// Refresh display
	(void)ioctl_call_uint32(SHARP_DEVICE_PATH, 
		DRM_IO(DRM_COMMAND_BASE + DRM_SHARP_REDRAW), 0);
}

// Clear display indicator
void input_display_clear_indicator(int idx)
{
	if (g_mod_overlays[idx].display != NULL) {
		if ((__sharp_memory_hide_overlay = symbol_get(sharp_memory_hide_overlay))) {
			__sharp_memory_hide_overlay(g_mod_overlays[idx].display);
			g_mod_overlays[idx].display = NULL;
		} else {
			return;
		}
	}

	// Refresh display
	(void)ioctl_call_uint32(SHARP_DEVICE_PATH, 
		DRM_IO(DRM_COMMAND_BASE + DRM_SHARP_REDRAW), 0);
}

// Clear all overlays
void input_display_clear_overlays(void)
{
	int i;

	if ((__sharp_memory_clear_overlays = symbol_get(sharp_memory_clear_overlays))) {
		__sharp_memory_clear_overlays();

		// Invalidate all overlays
		for (i = 0; i < MAX_INDICATORS; i++) {
			g_mod_overlays[i].storage = NULL;
			g_mod_overlays[i].display = NULL;
		}
		
		// Refresh display
		(void)ioctl_call_uint32(SHARP_DEVICE_PATH, 
			DRM_IO(DRM_COMMAND_BASE + DRM_SHARP_REDRAW), 0);
	}
}

int input_display_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	int i;

	g_mono_invert = 0;

	// Clear all indicator overlays
	if ((__sharp_memory_clear_overlays = symbol_get(sharp_memory_clear_overlays))) {
		__sharp_memory_clear_overlays();
	}
	for (i = 0; i < MAX_INDICATORS; i++) {
		g_mod_overlays[i].storage = NULL;
		g_mod_overlays[i].display = NULL;
	}

	return 0;
}

void input_display_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	int i;

	// Clear all indicator overlays
	if ((__sharp_memory_clear_overlays = symbol_get(sharp_memory_clear_overlays))) {
		__sharp_memory_clear_overlays();
	}
	for (i = 0; i < MAX_INDICATORS; i++) {
		g_mod_overlays[i].storage = NULL;
		g_mod_overlays[i].display = NULL;
	}
}
