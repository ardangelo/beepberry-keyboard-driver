// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * main.c: Main C File.
 */

#include <linux/version.h>
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

static int beepy_kbd_probe
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
(struct i2c_client* i2c_client, struct i2c_device_id const* i2c_id)
#else
(struct i2c_client* i2c_client)
#endif
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
	if ((rc = sysfs_probe(i2c_client))) {
		return rc;
	}

	return 0;
}

static void beepy_kbd_shutdown(struct i2c_client* i2c_client)
{
	sysfs_shutdown(i2c_client);
	params_shutdown();
	input_shutdown(i2c_client);
}

static void beepy_kbd_remove(struct i2c_client* i2c_client)
{
	dev_info_fe(&i2c_client->dev,
		"%s Removing beepy-kbd.\n", __func__);

	beepy_kbd_shutdown(i2c_client);
}

// Driver definitions

// Device IDs
static const struct i2c_device_id beepy_kbd_i2c_device_id[] = {
	{ "beepy-kbd", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, beepy_kbd_i2c_device_id);
static const struct of_device_id beepy_kbd_of_device_id[] = {
	{ .compatible = "beepy-kbd", },
	{ }
};
MODULE_DEVICE_TABLE(of, beepy_kbd_of_device_id);

// Callbacks
static struct i2c_driver beepy_kbd_driver = {
	.driver = {
		.name = "beepy-kbd",
		.of_match_table = beepy_kbd_of_device_id,
	},
	.probe    = beepy_kbd_probe,
	.shutdown = beepy_kbd_shutdown,
	.remove   = beepy_kbd_remove,
	.id_table = beepy_kbd_i2c_device_id,
};

// Module constructor
static int __init beepy_kbd_init(void)
{
	int rc;

	// Adding the I2C driver will call the _probe function to continue setup
	if ((rc = i2c_add_driver(&beepy_kbd_driver))) {
		pr_err("%s Could not initialise beepy-kbd! Error: %d\n",
			__func__, rc);
		return rc;
	}
	pr_info("%s Initalised beepy-kbd.\n", __func__);

	return rc;
}
module_init(beepy_kbd_init);

// Module destructor
static void __exit beepy_kbd_exit(void)
{
	pr_info("%s Exiting beepy-kbd.\n", __func__);
	i2c_del_driver(&beepy_kbd_driver);
}
module_exit(beepy_kbd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wallComputer and Andrew D'Angelo <dangeloandrew@outlook.com>");
MODULE_DESCRIPTION("BB Classic keyboard driver for Beepy");
MODULE_VERSION("2.11");
