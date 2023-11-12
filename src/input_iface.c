// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * input_iface.c: Key handler implementation
 */

#include <linux/input.h>
#include <linux/module.h>

#include "config.h"
#include "debug_levels.h"

#include "params_iface.h"
#include "input_iface.h"

#include "i2c_helper.h"

#include "bbq20kbd_pmod_codes.h"

// Global keyboard context and sysfs data
struct kbd_ctx *g_ctx = NULL;

// Sticky modifier structs
static struct sticky_modifier sticky_ctrl;
static struct sticky_modifier sticky_shift;
static struct sticky_modifier sticky_phys_alt;
static struct sticky_modifier sticky_alt;
static struct sticky_modifier sticky_altgr;

// Sticky modifier helpers

static void press_sticky_modifier(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier)
{
	input_report_key(ctx->input_dev, sticky_modifier->keycode, TRUE);
}

static void release_sticky_modifier(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier)
{
	input_report_key(ctx->input_dev, sticky_modifier->keycode, FALSE);
}

static void enable_phys_alt(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier)
{
	ctx->apply_phys_alt = 1;
}

static void disable_phys_alt(struct kbd_ctx* ctx, struct sticky_modifier const* sticky_modifier)
{
	// Send key up event if there is a current phys. alt key being held
	if (ctx->current_phys_alt_keycode) {
		input_report_key(ctx->input_dev, ctx->current_phys_alt_keycode, FALSE);
		ctx->current_phys_alt_keycode = 0;
	}

	ctx->apply_phys_alt = 0;
}

static uint8_t map_phys_alt_keycode(struct kbd_ctx* ctx, uint8_t keycode)
{
	if (!ctx->apply_phys_alt) {
		return keycode;
	}

	keycode += 119; // See map file for result keys
	ctx->current_phys_alt_keycode = keycode;
	return keycode;
}

// Sticky modifier keys follow BB Q10 convention
// Holding modifier while typing alpha keys will apply to all alpha keys
// until released.
// One press and release will enter sticky mode, apply modifier key to
// the next alpha key only. If the same modifier key is pressed and
// released again in sticky mode, it will be canceled.
static void transition_sticky_modifier(struct kbd_ctx* ctx,
	struct sticky_modifier const* sticky_modifier, enum rp2040_key_state state)
{
	if (state == KEY_STATE_PRESSED) {

		// Set "held" state
		ctx->held_modifier_keys |= sticky_modifier->bit;

		// If pressed again while sticky, clear sticky
		if (ctx->sticky_modifier_keys & sticky_modifier->bit) {
			ctx->sticky_modifier_keys &= ~sticky_modifier->bit;

		// Otherwise, set pending sticky to be applied on release
		} else {
			ctx->pending_sticky_modifier_keys |= sticky_modifier->bit;
		}

		// In locked mode
		if (ctx->locked_modifier_keys & sticky_modifier->bit) {

			// Clear lock mode
			ctx->locked_modifier_keys &= ~sticky_modifier->bit;

			// Clear pending sticky so that it is not applied to next key
			ctx->pending_sticky_modifier_keys &= ~sticky_modifier->bit;
		}

		// Report modifier to input system as pressed
		sticky_modifier->set_callback(ctx, sticky_modifier);

		// Set display indicator
		input_display_set_indicator(sticky_modifier->indicator_idx,
			sticky_modifier->indicator_char);

	// Released
	} else if (state == KEY_STATE_RELEASED) {

		// Unset "held" state
		ctx->held_modifier_keys &= ~sticky_modifier->bit;

		// Not in locked mode
		if (!(ctx->locked_modifier_keys & sticky_modifier->bit)) {

			// If any alpha key was typed during hold,
			// `apply_sticky_modifiers` will clear "pending sticky" state.
			// If still in "pending sticky", set "sticky" state.
			if (ctx->pending_sticky_modifier_keys & sticky_modifier->bit) {

				ctx->sticky_modifier_keys |= sticky_modifier->bit;
				ctx->pending_sticky_modifier_keys &= ~sticky_modifier->bit;

			} else {
				// Clear display indicator
				input_display_clear_indicator(sticky_modifier->indicator_idx);
			}

			// Report modifier to input system as released
			sticky_modifier->unset_callback(ctx, sticky_modifier);
		}

	// Held
	} else if (state == KEY_STATE_HOLD) {

		// If any alpha key was typed during hold,
		// `apply_sticky_modifiers` will clear "pending sticky" state.
		// If still in "pending sticky", set locked mode
		if (ctx->pending_sticky_modifier_keys & sticky_modifier->bit) {
			ctx->locked_modifier_keys |= sticky_modifier->bit;

			// Report modifier to input system as pressed
			sticky_modifier->set_callback(ctx, sticky_modifier);
		}
	}
}

// Called before sending an alpha key to apply any pending sticky modifiers
static void apply_sticky_modifier(struct kbd_ctx* ctx,
	struct sticky_modifier const* sticky_modifier)
{
	if (ctx->held_modifier_keys & sticky_modifier->bit) {
		ctx->pending_sticky_modifier_keys &= ~sticky_modifier->bit;

	} else if (ctx->sticky_modifier_keys & sticky_modifier->bit) {
		sticky_modifier->set_callback(ctx, sticky_modifier);
	}
}

// Called after sending the alpha key to reset
// any sticky modifiers
static void reset_sticky_modifier(struct kbd_ctx* ctx,
	struct sticky_modifier const* sticky_modifier)
{
	if (ctx->sticky_modifier_keys & sticky_modifier->bit) {
		ctx->sticky_modifier_keys &= ~sticky_modifier->bit;

		sticky_modifier->unset_callback(ctx, sticky_modifier);

		// Clear display indicator
		input_display_clear_indicator(sticky_modifier->indicator_idx);
	}
}

// Main key event handler
static void report_key_input_event(struct kbd_ctx* ctx,
	struct fifo_item const* ev)
{
	uint8_t keycode, remapped_keycode;

	// Only handle key pressed, held, or released events
	if ((ev->state != KEY_STATE_PRESSED) && (ev->state != KEY_STATE_RELEASED)
	 && (ev->state != KEY_STATE_HOLD)) {
		return;
	}

	// Post key scan event
	input_event(ctx->input_dev, EV_MSC, MSC_SCAN, ev->scancode);

	// Map input scancode to Linux input keycode
	keycode = ctx->keycode_map[ev->scancode];
	dev_info_fe(&ctx->i2c_client->dev,
		"%s state %d, scancode %d mapped to keycode %d\n",
		__func__, ev->state, ev->scancode, keycode);

	// Scancode mapped to ignored keycode
	if (keycode == 0) {
		return;

	// Scancode converted to keycode not in map
	} else if (keycode == KEY_UNKNOWN) {
		dev_warn(&ctx->i2c_client->dev,
			"%s Could not get Keycode for Scancode: [0x%02X]\n",
			__func__, ev->scancode);
		return;

	// Compose key sends enter if touchpad always sends arrow keys
	} else if (ctx->touchpad_always_keys && (keycode == KEY_COMPOSE)) {
		keycode = KEY_ENTER;
		// Continue to normal input handling

	// Compose key enters meta mode if touchpad not in arrow key mode
	} else if (!ctx->meta_enabled && (keycode == KEY_COMPOSE)) {
		if (ev->state == KEY_STATE_RELEASED) {
			input_meta_enable(ctx);
		}
		return;

	// Berry key sends Tmux prefix (Control + code 171 in keymap)
	} else if (keycode == KEY_PROPS) {
		if (ev->state == KEY_STATE_PRESSED) {
			input_report_key(ctx->input_dev, KEY_LEFTCTRL, TRUE);
			input_report_key(ctx->input_dev, 171, TRUE);
			input_report_key(ctx->input_dev, 171, FALSE);
			input_report_key(ctx->input_dev, KEY_LEFTCTRL, FALSE);
		}
		return;

	// Power key runs /sbin/poweroff if `handle_poweroff` is set
	} else if (ctx->fw_handle_poweroff && (keycode == KEY_POWER)) {
		if (ev->state == KEY_STATE_RELEASED) {
			input_fw_run_poweroff(ctx);
		}
		return;
	}

	// Handle keys without modifiers in meta mode
	if (!ctx->touchpad_always_keys && ctx->meta_enabled) {

		// Ignore modifier keys in meta mode
		if ((keycode == KEY_LEFTSHIFT) || (keycode == KEY_RIGHTSHIFT)
		 || (keycode == KEY_LEFTALT) || (keycode == KEY_RIGHTALT)
		 || (keycode == KEY_LEFTCTRL) || (keycode == KEY_RIGHTCTRL)) {
			return;
		}

		// Escape key exits meta mode
		if (keycode == KEY_ESC) {
			if (ev->state == KEY_STATE_RELEASED) {
				input_meta_disable(ctx);
			}
			return;
		}

		// Handle function dispatch meta mode keys
		if (input_meta_is_single_function_key(ctx, keycode)) {
			if (ev->state == KEY_STATE_RELEASED) {
		 		input_meta_run_single_function_key(ctx, keycode);
			}
			return;
		}

		// Remap to meta mode key
		remapped_keycode = input_meta_map_repeatable_key(ctx, keycode);

		// Input consumed by meta mode with no remmaped key
		if (remapped_keycode == 0) {
			return;
		}

		// Report key to input system
		input_report_key(ctx->input_dev, remapped_keycode, ev->state == KEY_STATE_PRESSED);

		// If exited meta mode, simulate key up event. Otherwise, input system
		// will have remapped key as in the down state
		if (!ctx->meta_enabled && (remapped_keycode != keycode)) {
			input_report_key(ctx->input_dev, remapped_keycode, FALSE);
		}

	} else if ((keycode == KEY_LEFTSHIFT) || (keycode == KEY_RIGHTSHIFT)) {
		transition_sticky_modifier(ctx, &sticky_shift, ev->state);

	} else if (keycode == KEY_LEFTALT) {
		transition_sticky_modifier(ctx, &sticky_phys_alt, ev->state);

	} else if (keycode == KEY_RIGHTALT) {
		transition_sticky_modifier(ctx, &sticky_altgr, ev->state);

	} else if (keycode == KEY_OPEN) {
		transition_sticky_modifier(ctx, &sticky_ctrl, ev->state);

	// Apply modifier keys
	} else {

		// Apply pending sticky modifiers
		apply_sticky_modifier(ctx, &sticky_shift);
		apply_sticky_modifier(ctx, &sticky_ctrl);
		apply_sticky_modifier(ctx, &sticky_phys_alt);
		apply_sticky_modifier(ctx, &sticky_alt);
		apply_sticky_modifier(ctx, &sticky_altgr);

		// Map phys. alt
		keycode = sticky_phys_alt.map_callback(ctx, keycode);

		// Report key to input system
		input_report_key(ctx->input_dev, keycode, ev->state == KEY_STATE_PRESSED);

		// Reset sticky modifiers
		reset_sticky_modifier(ctx, &sticky_shift);
		reset_sticky_modifier(ctx, &sticky_ctrl);
		reset_sticky_modifier(ctx, &sticky_phys_alt);
		reset_sticky_modifier(ctx, &sticky_alt);
		reset_sticky_modifier(ctx, &sticky_altgr);
	}
}

void input_send_control(struct kbd_ctx* ctx)
{
	transition_sticky_modifier(ctx, &sticky_ctrl, KEY_STATE_PRESSED);
	transition_sticky_modifier(ctx, &sticky_ctrl, KEY_STATE_RELEASED);
}

void input_send_alt(struct kbd_ctx* ctx)
{
	transition_sticky_modifier(ctx, &sticky_alt, KEY_STATE_PRESSED);
	transition_sticky_modifier(ctx, &sticky_alt, KEY_STATE_RELEASED);
}

int input_probe(struct i2c_client* i2c_client)
{
	int rc, i;

	// Allocate keyboard context (managed by device lifetime)
	g_ctx = devm_kzalloc(&i2c_client->dev, sizeof(*g_ctx), GFP_KERNEL);
	if (!g_ctx) {
		return -ENOMEM;
	}

	// Allocate and copy keycode array
	g_ctx->keycode_map = devm_kmemdup(&i2c_client->dev, keycodes, NUM_KEYCODES,
		GFP_KERNEL);
	if (!g_ctx->keycode_map) {
		return -ENOMEM;
	}

	// Initialize keyboard context
	g_ctx->i2c_client = i2c_client;
	g_ctx->touchpad_always_keys = 0;

	g_ctx->held_modifier_keys = 0;
	g_ctx->pending_sticky_modifier_keys = 0;
	g_ctx->sticky_modifier_keys = 0;
	g_ctx->locked_modifier_keys = 0;
	g_ctx->apply_phys_alt = 0;
	g_ctx->current_phys_alt_keycode = 0;

	// Initialize sticky modifiers
	sticky_ctrl.bit = (1 << 0);
	sticky_ctrl.keycode = KEY_LEFTCTRL;
	sticky_ctrl.set_callback = press_sticky_modifier;
	sticky_ctrl.unset_callback = release_sticky_modifier;
	sticky_ctrl.map_callback = NULL;
	sticky_ctrl.indicator_idx = 2;
	sticky_ctrl.indicator_char = 'c';

	sticky_shift.bit = (1 << 1);
	sticky_shift.keycode = KEY_LEFTSHIFT;
	sticky_shift.set_callback = press_sticky_modifier;
	sticky_shift.unset_callback = release_sticky_modifier;
	sticky_shift.map_callback = NULL;
	sticky_shift.indicator_idx = 0;
	sticky_shift.indicator_char = 's';

	sticky_phys_alt.bit = (1 << 2);
	sticky_phys_alt.keycode = KEY_RIGHTCTRL;
	sticky_phys_alt.set_callback = enable_phys_alt;
	sticky_phys_alt.unset_callback = disable_phys_alt;
	sticky_phys_alt.map_callback = map_phys_alt_keycode;
	sticky_phys_alt.indicator_idx = 1;
	sticky_phys_alt.indicator_char = 'p';

	sticky_alt.bit = (1 << 3);
	sticky_alt.keycode = KEY_LEFTALT;
	sticky_alt.set_callback = press_sticky_modifier;
	sticky_alt.unset_callback = release_sticky_modifier;
	sticky_alt.map_callback = NULL;
	sticky_alt.indicator_idx = 3;
	sticky_alt.indicator_char = 'a';

	sticky_altgr.bit = (1 << 4);
	sticky_altgr.keycode = KEY_RIGHTALT;
	sticky_altgr.set_callback = press_sticky_modifier;
	sticky_altgr.unset_callback = release_sticky_modifier;
	sticky_altgr.map_callback = NULL;
	sticky_altgr.indicator_idx = 4;
	sticky_altgr.indicator_char = 'g';

	// Run subsystem probes
	if ((rc = input_fw_probe(i2c_client, g_ctx))) {
		dev_err(&i2c_client->dev, "beepy-kbd: input_fw_probe failed\n");
		return rc;
	}
	if ((rc = input_rtc_probe(i2c_client, g_ctx))) {
		dev_err(&i2c_client->dev, "beepy-kbd: input_rtc_probe failed\n");
		return rc;
	}
	if ((rc = input_display_probe(i2c_client, g_ctx))) {
		dev_err(&i2c_client->dev, "beepy-kbd: input_display_probe failed\n");
		return rc;
	}
	if ((rc = input_meta_probe(i2c_client, g_ctx))) {
		dev_err(&i2c_client->dev, "beepy-kbd: input_meta_probe failed\n");
		return rc;
	}

	// Allocate input device
	if ((g_ctx->input_dev = devm_input_allocate_device(&i2c_client->dev)) == NULL) {
		dev_err(&i2c_client->dev,
			"%s Could not devm_input_allocate_device BBQX0KBD.\n", __func__);
		return -ENOMEM;
	}

	// Initialize input device
	g_ctx->input_dev->name = i2c_client->name;
	g_ctx->input_dev->id.bustype = BBQX0KBD_BUS_TYPE;
	g_ctx->input_dev->id.vendor  = BBQX0KBD_VENDOR_ID;
	g_ctx->input_dev->id.product = BBQX0KBD_PRODUCT_ID;
	g_ctx->input_dev->id.version = BBQX0KBD_VERSION_ID;

	// Initialize input device keycodes
	g_ctx->input_dev->keycode = g_ctx->keycode_map;
	g_ctx->input_dev->keycodesize = sizeof(g_ctx->keycode_map[0]);
	g_ctx->input_dev->keycodemax = ARRAY_SIZE(keycodes);

	// Set input device keycode bits
	for (i = 0; i < NUM_KEYCODES; i++) {
		__set_bit(g_ctx->keycode_map[i], g_ctx->input_dev->keybit);
	}
	__clear_bit(KEY_RESERVED, g_ctx->input_dev->keybit);
	__set_bit(EV_REP, g_ctx->input_dev->evbit);
	__set_bit(EV_KEY, g_ctx->input_dev->evbit);

	// Set input device capabilities
	input_set_capability(g_ctx->input_dev, EV_MSC, MSC_SCAN);
	#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)
		input_set_capability(g_ctx->input_dev, EV_REL, REL_X);
		input_set_capability(g_ctx->input_dev, EV_REL, REL_Y);
	#endif

	// Request IRQ handler for I2C client and initialize workqueue
	if ((rc = devm_request_threaded_irq(&i2c_client->dev,
		i2c_client->irq, NULL, input_irq_handler, IRQF_SHARED | IRQF_ONESHOT,
		i2c_client->name, g_ctx))) {

		dev_err(&i2c_client->dev,
			"Could not claim IRQ %d; error %d\n", i2c_client->irq, rc);
		return rc;
	}
	INIT_WORK(&g_ctx->work_struct, input_workqueue_handler);

	// Register input device with input subsystem
	dev_info(&i2c_client->dev,
		"%s registering input device", __func__);
	if ((rc = input_register_device(g_ctx->input_dev))) {
		dev_err(&i2c_client->dev,
			"Failed to register input device, error: %d\n", rc);
		return rc;
	}

	return 0;
}

void input_shutdown(struct i2c_client* i2c_client)
{
	// Run subsystem shutdowns
	input_fw_shutdown(i2c_client);
	input_display_shutdown(i2c_client);

	// Remove context from global state
	// (It is freed by the device-specific memory mananger)
	g_ctx = NULL;
}

irqreturn_t input_irq_handler(int irq, void *param)
{
	struct kbd_ctx *ctx;
	uint8_t irq_type;

	// `param` is current keyboard context as started in _probe
	ctx = (struct kbd_ctx *)param;

	dev_info_ld(&ctx->i2c_client->dev,
		"%s Interrupt Fired. IRQ: %d\n", __func__, irq);

	// Read interrupt type from client
	if (kbd_read_i2c_u8(ctx->i2c_client, REG_INT, &irq_type)) {
		return IRQ_NONE;
	}
	dev_info_ld(&ctx->i2c_client->dev,
		"%s Interrupt type: 0x%02x\n", __func__, irq_type);

	// Reported no interrupt type
	if (irq_type == 0x00) {
		return IRQ_NONE;
	}

	// Client reported a key overflow
	if (irq_type & REG_INT_OVERFLOW) {
		dev_warn(&ctx->i2c_client->dev, "%s overflow occurred.\n", __func__);
	}

	// Client reported a key event
	if (irq_type & REG_INT_KEY) {
		input_fw_read_fifo(ctx);
		schedule_work(&ctx->work_struct);
	}

	// Client reported a touch event
	if (irq_type & REG_INT_TOUCH) {

		// Read touch X-coordinate
		if (kbd_read_i2c_u8(ctx->i2c_client, REG_TOX, &ctx->touch_rel_x)) {
			return IRQ_NONE;
		}

		// Read touch Y-coordinate
		if (kbd_read_i2c_u8(ctx->i2c_client, REG_TOY, &ctx->touch_rel_y)) {
			return IRQ_NONE;
		}

		// Set touch event flag and schedule touch work
		ctx->touch_event_flag = 1;
		schedule_work(&ctx->work_struct);

	} else {

		// Clear touch event flag
		ctx->touch_event_flag = 0;
	}

	return IRQ_HANDLED;
}

void input_workqueue_handler(struct work_struct *work_struct_ptr)
{
	struct kbd_ctx *ctx;
	uint8_t fifo_idx;

	// Get keyboard context from work struct
	ctx = container_of(work_struct_ptr, struct kbd_ctx, work_struct);

	// Process FIFO items
	for (fifo_idx = 0; fifo_idx < ctx->fifo_count; fifo_idx++) {
		report_key_input_event(ctx, &ctx->fifo_data[fifo_idx]);
	}

	// Reset pending FIFO count
	ctx->fifo_count = 0;

	// Handle touch interrupt flag
	if (ctx->touch_event_flag) {

		dev_info_ld(&ctx->i2c_client->dev,
			"%s X Reg: %d Y Reg: %d.\n",
			__func__, ctx->touch_rel_x, ctx->touch_rel_y);

		#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)

			// Report mouse movement
			input_report_rel(ctx->input_dev, REL_X, ctx->touch_rel_x);
			input_report_rel(ctx->input_dev, REL_Y, ctx->touch_rel_y);

			// Clear touch interrupt flag
			ctx->touch_event_flag = 0;
		#endif

		#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_KEYS)

		// Touchpad-as-keys mode will always use the touchpad as arrow keys
		if (ctx->touchpad_always_keys) {

			// Negative X: left arrow key
			if (ctx->touch_rel_x < -4) {
				input_report_key(ctx->input_dev, KEY_LEFT, TRUE);
				input_report_key(ctx->input_dev, KEY_LEFT, FALSE);

			// Positive X: right arrow key
			} else if (ctx->touch_rel_x > 4) {
				input_report_key(ctx->input_dev, KEY_RIGHT, TRUE);
				input_report_key(ctx->input_dev, KEY_RIGHT, FALSE);
			}

			// Negative Y: up arrow key
			if (ctx->touch_rel_y < -4) {
				input_report_key(ctx->input_dev, KEY_UP, TRUE);
				input_report_key(ctx->input_dev, KEY_UP, FALSE);

			// Positive Y: down arrow key
			} else if (ctx->touch_rel_y > 4) {
				input_report_key(ctx->input_dev, KEY_DOWN, TRUE);
				input_report_key(ctx->input_dev, KEY_DOWN, FALSE);
			}

		// Meta touch keys will only send up/down keys when enabled
		} else if (ctx->meta_touch_keys_mode) {

			// Negative Y: up arrow key
			if (ctx->touch_rel_y < -4) {
				input_report_key(ctx->input_dev, KEY_UP, TRUE);
				input_report_key(ctx->input_dev, KEY_UP, FALSE);

			// Positive Y: down arrow key
			} else if (ctx->touch_rel_y > 4) {
				input_report_key(ctx->input_dev, KEY_DOWN, TRUE);
				input_report_key(ctx->input_dev, KEY_DOWN, FALSE);
			}
		}

		// Clear touch interrupt flag
		ctx->touch_event_flag = 0;
		#endif
	}

	// Synchronize input system and clear client interrupt flag
	input_sync(ctx->input_dev);
	if (kbd_write_i2c_u8(ctx->i2c_client, REG_INT, 0)) {
		return;
	}
}
