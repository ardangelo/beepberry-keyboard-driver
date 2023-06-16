// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * main.c: Main C File.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>

#include "config.h"
#include "debug_levels.h"

#include "input_iface.h"
#include "params_iface.h"
#include "sysfs_iface.h"

#if (BBQX0KBD_INT != BBQX0KBD_USE_INT)
#error "Only supporting interrupts mode right now"
#endif

#if (BBQX0KBD_TYPE != BBQ20KBD_PMOD)
#error "Only supporting BBQ20 keyboard right now"
#endif

static int bbqX0kbd_probe(struct i2c_client* i2c_client, struct i2c_device_id const* i2c_id)
{
	int rc;

	// Initialize key handler system
	if ((rc = input_probe(i2c_client))) {
		return rc;
	}

	// Initialize module parameters
	if ((rc = params_probe())) {
		return rc;
	}

	// Initialize sysfs interface
	if ((rc = sysfs_probe())) {
		return rc;
	}

	return 0;
}

static void bbqX0kbd_shutdown(struct i2c_client* i2c_client)
{
	sysfs_shutdown();
	params_shutdown();
	input_shutdown(i2c_client);
}

static void bbqX0kbd_remove(struct i2c_client* i2c_client)
{
	dev_info_fe(&i2c_client->dev,
		"%s Removing BBQX0KBD.\n", __func__);

	bbqX0kbd_shutdown(i2c_client);
}

// Driver definitions

// Device IDs
static const struct i2c_device_id bbqX0kbd_i2c_device_id[] = {
	{ "bbqX0kbd", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bbqX0kbd_i2c_device_id);
static const struct of_device_id bbqX0kbd_of_device_id[] = {
	{ .compatible = "wallComputer,bbqX0kbd", },
	{ }
};
MODULE_DEVICE_TABLE(of, bbqX0kbd_of_device_id);

// Callbacks
static struct i2c_driver bbqX0kbd_driver = {
	.driver = {
		.name = "bbqX0kbd",
		.of_match_table = bbqX0kbd_of_device_id,
	},
	.probe    = bbqX0kbd_probe,
	.shutdown = bbqX0kbd_shutdown,
	.remove   = bbqX0kbd_remove,
	.id_table = bbqX0kbd_i2c_device_id,
};

// Module constructor
static int __init bbqX0kbd_init(void)
{
	int rc;

	// Adding the I2C driver will call the _probe function to continue setup
	if ((rc = i2c_add_driver(&bbqX0kbd_driver))) {
		pr_err("%s Could not initialise BBQX0KBD driver! Error: %d\n",
			__func__, rc);
		return rc;
	}
	pr_info("%s Initalised BBQX0KBD.\n", __func__);

	return rc;
}
module_init(bbqX0kbd_init);

// Module destructor
static void __exit bbqX0kbd_exit(void)
{
	pr_info("%s Exiting BBQX0KBD.\n", __func__);
	i2c_del_driver(&bbqX0kbd_driver);
}
module_exit(bbqX0kbd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wallComputer and Andrew D'Angelo <dangeloandrew@outlook.com>");
MODULE_DESCRIPTION("BB Classic keyboard driver for Beepberry");
MODULE_VERSION("0.5");