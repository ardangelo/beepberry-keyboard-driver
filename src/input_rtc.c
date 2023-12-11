// SPDX-License-Identifier: GPL-2.0-only
// Input RTC subsystem

#include <linux/rtc.h>

#include "config.h"
#include "input_iface.h"

static int i2c_set_time(struct device *dev, struct rtc_time *tm)
{
	int rc;

	if ((rc = input_fw_set_rtc((uint8_t)tm->tm_year, (uint8_t)tm->tm_mon,
		(uint8_t)tm->tm_mday, (uint8_t)tm->tm_hour, (uint8_t)tm->tm_min,
		(uint8_t)tm->tm_sec))) {
		printk(KERN_ERR "i2c_set_time failed: %d\n", rc);
		return rc;
	}

	printk(KERN_INFO "beepy-kbd: updated RTC\n");

	return 0;
}

static int i2c_read_time(struct device *dev, struct rtc_time *tm)
{
	int rc;
	uint8_t year, mon, mday, hour, min, sec;

	if ((rc = input_fw_get_rtc(&year, &mon, &mday, &hour, &min, &sec))) {
		printk(KERN_ERR "i2c_read_time failed: %d\n", rc);
		return rc;
	}

	tm->tm_year = year;
	tm->tm_mon = mon;
	tm->tm_mday = mday;
	tm->tm_hour = hour;
	tm->tm_min = min;
	tm->tm_sec = sec;

	return 0;
}

static const struct rtc_class_ops beepy_rtc_ops = {
	.read_time = i2c_read_time,
	.set_time = i2c_set_time,
};

int input_rtc_probe(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{
	struct rtc_device *rtc;

	// Register RTC device
	rtc = devm_rtc_device_register(&i2c_client->dev,
		"beepy-rtc", &beepy_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc)) {
		dev_err(&i2c_client->dev,
			"Failed to register RTC device\n");
		return PTR_ERR(rtc);
	}

	return 0;
}

void input_rtc_shutdown(struct i2c_client* i2c_client, struct kbd_ctx *ctx)
{}
