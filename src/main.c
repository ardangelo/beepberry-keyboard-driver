// SPDX-License-Identifier: GPL-2.0-only

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>

#include "config.h"
#include "debug_levels.h"

#include "input_iface.h"
#include "params_iface.h"
#include "sysfs_iface.h"

static int emate_kbd_probe
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

static void emate_kbd_shutdown(struct i2c_client* i2c_client)
{
	sysfs_shutdown(i2c_client);
	params_shutdown();
	input_shutdown(i2c_client);
}

static void emate_kbd_remove(struct i2c_client* i2c_client)
{
	dev_info_fe(&i2c_client->dev,
		"%s Removing emate-kbd.\n", __func__);

	emate_kbd_shutdown(i2c_client);
}

// Driver definitions

// Device IDs
static const struct i2c_device_id emate_kbd_i2c_device_id[] = {
	{ "emate-kbd", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, emate_kbd_i2c_device_id);
static const struct of_device_id emate_kbd_of_device_id[] = {
	{ .compatible = "emate-kbd", },
	{ }
};
MODULE_DEVICE_TABLE(of, emate_kbd_of_device_id);

// Callbacks
static struct i2c_driver emate_kbd_driver = {
	.driver = {
		.name = "emate-kbd",
		.of_match_table = emate_kbd_of_device_id,
	},
	.probe    = emate_kbd_probe,
	.shutdown = emate_kbd_shutdown,
	.remove   = emate_kbd_remove,
	.id_table = emate_kbd_i2c_device_id,
};

// Module constructor
static int __init emate_kbd_init(void)
{
	int rc;

	// Adding the I2C driver will call the _probe function to continue setup
	if ((rc = i2c_add_driver(&emate_kbd_driver))) {
		pr_err("%s Could not initialise emate-kbd! Error: %d\n",
			__func__, rc);
		return rc;
	}
	pr_info("%s Initalised emate-kbd.\n", __func__);

	return rc;
}
module_init(emate_kbd_init);

// Module destructor
static void __exit emate_kbd_exit(void)
{
	pr_info("%s Exiting emate-kbd.\n", __func__);
	i2c_del_driver(&emate_kbd_driver);
}
module_exit(emate_kbd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrew D'Angelo <dangeloandrew@outlook.com>");
MODULE_DESCRIPTION("eMate-CM4 keyboard driver");
MODULE_VERSION("0.9");
