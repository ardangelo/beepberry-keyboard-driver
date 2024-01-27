// SPDX-License-Identifier: GPL-2.0-only
// Input display subsystem

#include <drm/drm.h>

#include "config.h"
#include "input_iface.h"
#include "params_iface.h"

#include "indicators.h"

// Display ioctl

#define DRM_SHARP_REDRAW 0x00

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

static int ioctl_sharp_redraw(void)
{
	return ioctl_call_uint32(params_get_sharp_path(),
		DRM_IO(DRM_COMMAND_BASE + DRM_SHARP_REDRAW), 0);
}

// Whether this is a path to a valid Sharp display device
int input_display_valid_sharp_path(char const* path)
{
	// Try to refresh screen
	return ioctl_call_uint32(path,
		DRM_IO(DRM_COMMAND_BASE + DRM_SHARP_REDRAW), 0);
}

// Invert display colors by writing to display driver parameter
void input_display_invert(struct kbd_ctx* ctx)
{
	// Update saved invert value
	g_mono_invert = (g_mono_invert) ? 0 : 1;

	if (__sharp_memory_set_invert == NULL) {
		__sharp_memory_set_invert = symbol_get(sharp_memory_set_invert);
	}
	if (__sharp_memory_set_invert == NULL) {
		return;
	}

	// Apply invert value
	__sharp_memory_set_invert(g_mono_invert);
	(void)ioctl_sharp_redraw();
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

		// Get overlay add function
		if (__sharp_memory_add_overlay == NULL) {
			__sharp_memory_add_overlay = symbol_get(sharp_memory_add_overlay);
		}
		if (__sharp_memory_add_overlay == NULL) {
			return;
		}

		// Add indicator overlay
		g_mod_overlays[idx].storage = __sharp_memory_add_overlay(
			x, 0, INDICATOR_WIDTH, INDICATOR_HEIGHT, pixels);
	}

	if (g_mod_overlays[idx].display == NULL) {

		if (__sharp_memory_show_overlay == NULL) {
			__sharp_memory_show_overlay = symbol_get(sharp_memory_show_overlay);
		}
		if (__sharp_memory_show_overlay == NULL) {
			return;
		}

		// Display indicator overlay and set display handle
		g_mod_overlays[idx].display = __sharp_memory_show_overlay(
			g_mod_overlays[idx].storage);
	}

	// Refresh display
	(void)ioctl_sharp_redraw();
}

// Clear display indicator
void input_display_clear_indicator(int idx)
{
	if (g_mod_overlays[idx].display != NULL) {

		// Get overlay hide function
		if (__sharp_memory_hide_overlay == NULL) {
			__sharp_memory_hide_overlay = symbol_get(sharp_memory_hide_overlay);
		}
		if (__sharp_memory_hide_overlay == NULL) {
			return;
		}

		// Hide overlay and reset display handle
		__sharp_memory_hide_overlay(g_mod_overlays[idx].display);
		g_mod_overlays[idx].display = NULL;
		(void)ioctl_sharp_redraw();
	}
}

// Clear all overlays
void input_display_clear_overlays(void)
{
	int i;

	// Get overlay clear function
	if (__sharp_memory_clear_overlays == NULL) {
		__sharp_memory_clear_overlays = symbol_get(sharp_memory_clear_overlays);
	}
	if (__sharp_memory_clear_overlays == NULL) {
		return;
	}

	// Invalidate all overlays
	for (i = 0; i < MAX_INDICATORS; i++) {
		g_mod_overlays[i].storage = NULL;
		g_mod_overlays[i].display = NULL;
	}

	// Clear all overlays
	__sharp_memory_clear_overlays();

	// Refresh display
	(void)ioctl_sharp_redraw();
}

int input_display_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	g_mono_invert = 0;

	// Clear all overlays
	input_display_clear_overlays();

	return 0;
}

void input_display_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	// Clear all overlays
	input_display_clear_overlays();
}
