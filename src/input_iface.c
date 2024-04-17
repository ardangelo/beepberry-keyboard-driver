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

// Main key event handler
static void key_report_event(struct kbd_ctx* ctx,
	struct key_fifo_item const* ev)
{
	uint8_t keycode;

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
	}

	// Update last keypress time
	g_ctx->last_keypress_at = ktime_get_boottime_ns();

	if (keycode == KEY_STOP) {

		// Pressing power button sends Tmux prefix (Control + code 171 in keymap)
		if (ev->state == KEY_STATE_PRESSED) {
			input_report_key(ctx->input_dev, KEY_LEFTCTRL, TRUE);
			input_report_key(ctx->input_dev, 171, TRUE);
			input_report_key(ctx->input_dev, 171, FALSE);
			input_report_key(ctx->input_dev, KEY_LEFTCTRL, FALSE);

		// Short hold power buttion opens Tmux menu (Control + code 174 in keymap)
		} else if (ev->state == KEY_STATE_HOLD) {
			input_report_key(ctx->input_dev, KEY_LEFTCTRL, TRUE);
			input_report_key(ctx->input_dev, 174, TRUE);
			input_report_key(ctx->input_dev, 174, FALSE);
			input_report_key(ctx->input_dev, KEY_LEFTCTRL, FALSE);
		}
		return;
	}

	// Subsystem key handling
	if (input_fw_consumes_keycode(ctx, &keycode, keycode, ev->state)
	 || input_touch_consumes_keycode(ctx, &keycode, keycode, ev->state)
	 || input_modifiers_consumes_keycode(ctx, &keycode, keycode, ev->state)
	 || input_meta_consumes_keycode(ctx, &keycode, keycode, ev->state)) {
		return;
	}

	// Ignore hold keys at this point
	if (ev->state == KEY_STATE_HOLD) {
		return;
	}

	// Apply pending sticky modifiers
	keycode = input_modifiers_apply_pending(ctx, keycode);

	// Report key to input system
	input_report_key(ctx->input_dev, keycode, ev->state == KEY_STATE_PRESSED);

	// Reset sticky modifiers
	input_modifiers_reset(ctx);
}

static irqreturn_t input_irq_handler(int irq, void *param)
{
	struct kbd_ctx *ctx;
	uint8_t irq_type;
	int8_t reg_value;

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
		if (kbd_read_i2c_u8(ctx->i2c_client, REG_TOX, &reg_value)) {
			return IRQ_NONE;
		}
		ctx->touch.dx += reg_value;

		// Read touch Y-coordinate
		if (kbd_read_i2c_u8(ctx->i2c_client, REG_TOY, &reg_value)) {
			return IRQ_NONE;
		}
		ctx->touch.dy += reg_value;

		// Set touch event flag and schedule touch work
		ctx->raised_touch_event = 1;
		schedule_work(&ctx->work_struct);

	} else {

		// Clear touch event flag
		ctx->raised_touch_event = 0;
	}

	return IRQ_HANDLED;
}

static void input_workqueue_handler(struct work_struct *work_struct_ptr)
{
	struct kbd_ctx *ctx;
	uint8_t fifo_idx;

	// Get keyboard context from work struct
	ctx = container_of(work_struct_ptr, struct kbd_ctx, work_struct);

	// Process FIFO items
	for (fifo_idx = 0; fifo_idx < ctx->key_fifo_count; fifo_idx++) {
		key_report_event(ctx, &ctx->key_fifo_data[fifo_idx]);
	}

	// Reset pending FIFO count
	ctx->key_fifo_count = 0;

	// Handle any pending touch events
	if (ctx->raised_touch_event) {
		input_touch_report_event(ctx);
		ctx->raised_touch_event = 0;
	}

	// Synchronize input system and clear client interrupt flag
	input_sync(ctx->input_dev);
	if (kbd_write_i2c_u8(ctx->i2c_client, REG_INT, 0)) {
		return;
	}
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
	g_ctx->last_keypress_at = ktime_get_boottime_ns();

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
	if ((rc = input_modifiers_probe(i2c_client, g_ctx))) {
		dev_err(&i2c_client->dev, "beepy-kbd: input_modifiers_probe failed\n");
		return rc;
	}
	if ((rc = input_touch_probe(i2c_client, g_ctx))) {
		dev_err(&i2c_client->dev, "beepy-kbd: input_touch_probe failed\n");
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
	input_set_capability(g_ctx->input_dev, EV_REL, REL_X);
	input_set_capability(g_ctx->input_dev, EV_REL, REL_Y);
	input_set_capability(g_ctx->input_dev, EV_KEY, BTN_LEFT);
	input_set_capability(g_ctx->input_dev, EV_KEY, BTN_RIGHT);

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
	input_meta_shutdown(i2c_client, g_ctx);
	input_touch_shutdown(i2c_client, g_ctx);
	input_modifiers_shutdown(i2c_client, g_ctx);
	input_display_shutdown(i2c_client, g_ctx);
	input_rtc_shutdown(i2c_client, g_ctx);
	input_fw_shutdown(i2c_client, g_ctx);

	// Remove context from global state
	// (It is freed by the device-specific memory mananger)
	g_ctx = NULL;
}
