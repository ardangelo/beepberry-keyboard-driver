#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/rtc.h>

#include "input_iface.h"
#include "ioctl_iface.h"

#define RTC_DEV_MAX 16

static int i2c_set_time(struct rtc_time const* tm)
{
	int rc;

	if ((rc = input_set_rtc((uint8_t)tm->tm_year, (uint8_t)tm->tm_mon,
		(uint8_t)tm->tm_mday, (uint8_t)tm->tm_hour, (uint8_t)tm->tm_min,
		(uint8_t)tm->tm_sec))) {

		printk(KERN_ERR "i2c_set_time failed: %d\n", rc);
		return rc;
	}

	printk(KERN_INFO "bbqX0kbd: updated RTC\n");

	return 0;
}

static int i2c_get_time(struct rtc_time* tm)
{
	int rc;
	uint8_t year, mon, mday, hour, min, sec;

	if ((rc = input_get_rtc(&year, &mon, &mday, &hour, &min, &sec))) {
		printk(KERN_ERR "i2c_get_time failed: %d\n", rc);
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

// Cut down version of the Linux RTC driver at drivers/rtc/rtc-dev.c

static int rtc_dev_open(struct inode *inode, struct file *file)
{
	struct rtc_device *rtc = container_of(inode->i_cdev,
					struct rtc_device, char_dev);

	if (test_and_set_bit_lock(RTC_DEV_BUSY, &rtc->flags))
		return -EBUSY;

	file->private_data = rtc;

	return 0;
}

static ssize_t
rtc_dev_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long data;
	ssize_t ret;

	if (count != sizeof(unsigned int) && count < sizeof(unsigned long))
		return -EINVAL;

	if (ret == 0) {
		if (sizeof(int) != sizeof(long) &&
		    count == sizeof(unsigned int))
			ret = put_user(data, (unsigned int __user *)buf) ?:
				sizeof(unsigned int);
		else
			ret = put_user(data, (unsigned long __user *)buf) ?:
				sizeof(unsigned long);
	}

	return ret;
}

static unsigned int rtc_dev_poll(struct file *file, poll_table *wait)
{
	return -EINVAL;
}

static long rtc_dev_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct rtc_time tm;
	void __user *uarg = (void __user *) arg;

/*
	err = mutex_lock_interruptible(&rtc->ops_lock);
	if (err) {
		return err;
	}
*/

	/* check that the calling task has appropriate permissions
	 * for certain ioctls. doing this check here is useful
	 * to avoid duplicate code in each driver.
	 */
	switch (cmd) {
	case RTC_SET_TIME:
		if (!capable(CAP_SYS_TIME)) {
			printk(KERN_INFO "No permission to set time\n");
			err = -EACCES;
		}
		break;
	}

	if (err)
		goto done;

	switch (cmd) {

	case RTC_RD_TIME:
		//mutex_unlock(&rtc->ops_lock);
		err = i2c_get_time(&tm);
		if (err < 0)
			return err;

		if (copy_to_user(uarg, &tm, sizeof(tm)))
			err = -EFAULT;
		return err;

	case RTC_SET_TIME:
		//mutex_unlock(&rtc->ops_lock);
		if (copy_from_user(&tm, uarg, sizeof(tm)))
			return -EFAULT;

		return i2c_set_time(&tm);

	default:
		err = -ENOTTY;
		break;
	}

done:
	printk(KERN_INFO "Done, unlocking mutex\n");
	//mutex_unlock(&rtc->ops_lock);
	return err;
}

static int rtc_dev_fasync(int fd, struct file *file, int on)
{
	struct rtc_device *rtc = file->private_data;
	return fasync_helper(fd, file, on, &rtc->async_queue);
}

static int rtc_dev_release(struct inode *inode, struct file *file)
{
	struct rtc_device *rtc = file->private_data;

	clear_bit_unlock(RTC_DEV_BUSY, &rtc->flags);
	return 0;
}

static const struct file_operations fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= rtc_dev_read,
	.poll		= rtc_dev_poll,
	.unlocked_ioctl	= rtc_dev_ioctl,
	.open		= rtc_dev_open,
	.release	= rtc_dev_release,
	.fasync		= rtc_dev_fasync,
};

static struct cdev g_cdev;
static dev_t g_dev;
struct class *g_class;

int ioctl_probe(void)
{
	int rc;

	if ((rc = alloc_chrdev_region(&g_dev, 0, RTC_DEV_MAX, "rtc"))) {
		printk(KERN_ERR "Failed to allocate device numbers\n");
		return rc;
	}

	g_class = class_create(THIS_MODULE, "rtc_class");
	device_create(g_class, NULL, g_dev, NULL, "rtc");

	cdev_init(&g_cdev, &fops);
	g_cdev.owner = THIS_MODULE;

	if ((rc = cdev_add(&g_cdev, g_dev, 1))) {
		unregister_chrdev_region(g_dev, 1);
		printk(KERN_ERR "Failed to add the character device\n");
		return rc;
	}

	printk(KERN_INFO "Character device registered: major = %d, minor = %d\n",
		MAJOR(g_dev), MINOR(g_dev));

	return 0;
}

void ioctl_shutdown(void)
{
	cdev_del(&g_cdev);
	device_destroy(g_class, g_dev);
	class_destroy(g_class);
	unregister_chrdev_region(g_dev, 1);
	printk(KERN_INFO "Character device unregistered\n");
}
