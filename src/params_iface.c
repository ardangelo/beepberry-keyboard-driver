// SPDX-License-Identifier: GPL-2.0-only

#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "config.h"

#include "i2c_helper.h"
#include "input_iface.h"
#include "params_iface.h"

// Kernel module parameters
static uint32_t sysfs_gid_setting = 0; // GID of files in /sys/firmware/emate

// Time between shutdown signal and power off in seconds
static int sysfs_gid_param_set(const char *val, const struct kernel_param *kp)
{
	char *stripped_val;
	char stripped_val_buf[11];

	// Copy provided value to buffer and strip it of newlines
	strncpy(stripped_val_buf, val, 11);
	stripped_val_buf[10] = '\0';
	stripped_val = strstrip(stripped_val_buf);

	return param_set_uint(stripped_val, kp);
}

static const struct kernel_param_ops sysfs_gid_param_ops = {
	.set = sysfs_gid_param_set,
	.get = param_get_uint,
};

module_param_cb(sysfs_gid, &sysfs_gid_param_ops, &sysfs_gid_setting, 0664);
MODULE_PARM_DESC(sysfs_gid_setting, "Set group ID for entries in /sys/firmware/emate");

// No setup
int params_probe(void)
{
	return 0;
}

// No cleanup
void params_shutdown(void)
{
	return;
}

uint32_t params_get_sysfs_gid(void)
{
	return sysfs_gid_setting;
}
