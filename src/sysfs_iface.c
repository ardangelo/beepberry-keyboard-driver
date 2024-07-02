// SPDX-License-Identifier: GPL-2.0-only
/*
 * Kernel driver for Q20 keyboard by ardangelo
 * References work by arturo182, wallComputer
 * sysfs.c: /sys/firmware/emate interface
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/timekeeping.h>
#include <linux/math64.h>

#include "config.h"

#include "i2c_helper.h"
#include "input_iface.h"
#include "params_iface.h"
#include "sysfs_iface.h"

static int parse_and_write_i2c_u8(char const* buf, size_t count, uint8_t reg)
{
	int parsed;

	// Parse string entry
	if ((parsed = parse_u8(buf)) < 0) {
		return -EINVAL;
	}

	// Write value to LED register if available
	if (g_ctx && g_ctx->i2c_client) {
		kbd_write_i2c_u8(g_ctx->i2c_client, reg, (uint8_t)parsed);
	}

	return count;
}

// Sysfs entries

// Firmware version
static ssize_t fw_version_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	int rc;
	uint8_t version;

	// Make sure I2C client was initialized
	if ((g_ctx == NULL) || (g_ctx->i2c_client == NULL)) {
		return -EINVAL;
	}

	// Read firmware version
	if ((rc = kbd_read_i2c_u8(g_ctx->i2c_client, REG_VER, &version)) < 0) {
		return rc;
	}

	return sprintf(buf, "%d.%d\n", version >> 4, version & 0xf);
}
struct kobj_attribute fw_version_attr
	= __ATTR(fw_version, 0444, fw_version_show, NULL);

// Firmware update
static ssize_t __used fw_update_store(struct kobject *kobj,
	struct kobj_attribute *attr, char const *buf, size_t count)
{
	int rc;
	size_t i;
	uint8_t update_state;
	char const* update_error = NULL;

	// Write value to update
	if (g_ctx && g_ctx->i2c_client) {

		// Read update status
		if ((rc = kbd_read_i2c_u8(g_ctx->i2c_client, REG_UPDATE_DATA, &update_state)) < 0) {
			return rc;
		}

		// Start a new update
		if ((update_state == UPDATE_OFF) || (update_state >= UPDATE_FAILED)) {
			dev_info(&g_ctx->i2c_client->dev,
				"fw_update: starting new update, writing %zu bytes\n", count);

		// In-progress update
		} else if (update_state == UPDATE_RECV) {
			dev_info(&g_ctx->i2c_client->dev,
				"fw_update: writing %zu bytes\n", count);
		}

		for (i = 0; i < count; i++) {
			kbd_write_i2c_u8(g_ctx->i2c_client, REG_UPDATE_DATA, (uint8_t)buf[i]);
		}

		// Read update status
		if ((rc = kbd_read_i2c_u8(g_ctx->i2c_client, REG_UPDATE_DATA, &update_state)) < 0) {
			return rc;
		}

		// Successful update
		if (update_state == UPDATE_OFF) {
			dev_info(&g_ctx->i2c_client->dev,
				"fw_update: wrote %zu bytes, update completed\n", count);

		// Update still in-progress
		} else if (update_state == UPDATE_RECV) {
			dev_info(&g_ctx->i2c_client->dev,
				"fw_update: wrote %zu bytes\n", count);

		// Update failed
		} else if (update_state >= UPDATE_FAILED) {

			update_error = "update failed";

			switch (update_state) {
			case UPDATE_FAILED_LINE_OVERFLOW:
				update_error = "hex line too long"; break;
			case UPDATE_FAILED_FLASH_EMPTY:
				update_error = "flash image empty"; break;
			case UPDATE_FAILED_FLASH_OVERFLOW:
				update_error = "flash image > 64k"; break;
			case UPDATE_FAILED_BAD_LINE:
				update_error = "could not parse hex line"; break;
			case UPDATE_FAILED_BAD_CHECKSUM:
				update_error = "bad checksum"; break;
			}

			dev_info(&g_ctx->i2c_client->dev,
				"fw_update: failed: %s\n", update_error);
			return -EINVAL;
		}
	}

	return count;
}
struct kobj_attribute fw_update_attr
	= __ATTR(fw_update, 0220, NULL, fw_update_store);

// Sysfs attributes (entries)
struct kobject *emate_kobj = NULL;
static struct attribute *emate_attrs[] = {
	&fw_version_attr.attr,
	&fw_update_attr.attr,
	NULL,
};
static struct attribute_group emate_attr_group = {
	.attrs = emate_attrs
};

static void emate_get_ownership
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
(struct kobject *kobj, kuid_t *uid, kgid_t *gid)
#else
(struct kobject const *kobj, kuid_t *uid, kgid_t *gid)
#endif
{
	if (gid != NULL) {
		gid->val = params_get_sysfs_gid();
	}
}

static struct kobj_type emate_ktype = {
	.get_ownership = emate_get_ownership,
	.sysfs_ops = &kobj_sysfs_ops
};

int sysfs_probe(struct i2c_client* i2c_client)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
	int rc;
#endif

	// Allocate custom sysfs type
	if ((emate_kobj = devm_kzalloc(&i2c_client->dev, sizeof(*emate_kobj), GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	}

	// Create sysfs entries for emate with custom type
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
	rc =
#endif
	kobject_init_and_add(emate_kobj, &emate_ktype, firmware_kobj, "emate");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
	if (rc < 0) {
		kobject_put(emate_kobj);
		return rc;
	}
#endif

	// Create sysfs attributes
	if (sysfs_create_group(emate_kobj, &emate_attr_group)) {
		kobject_put(emate_kobj);
		return -ENOMEM;
	}

	return 0;
}

void sysfs_shutdown(struct i2c_client* i2c_client)
{
	// Remove sysfs entry
	if (emate_kobj) {
		kobject_put(emate_kobj);
		emate_kobj = NULL;
	}
}

