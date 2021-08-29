// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * msi-ec.c - MSI Embedded Controller for laptops support.
 *
 * This driver exports a few files in /sys/devices/platform/msi-laptop:
 *   webcam         Integrated webcam activation
 *   fn_key         Function key location
 *   win_key        Windows key location
 *   battery_mode   Battery health options
 *
 * In addition to these platform device attributes the driver
 * registers itself in the Linux power_supply subsystem and is
 * available to userspace under /sys/class/power_supply/<power_supply>:
 *
 *   charge_control_start_threshold
 *   charge_control_end_threshold
 *
 * This driver might not work on other laptops produced by MSI. Also, and until
 * future enhancements, no DMI data are used to identify your compatibility
 *
 */

#include <acpi/battery.h>
#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define MSI_DRIVER_NAME "msi-ec"
#define MSI_EC_WEBCAM_ADDRESS 0x2e
#define MSI_EC_WEBCAM_ON 0x4a
#define MSI_EC_WEBCAM_OFF 0x48
#define MSI_EC_FN_WIN_ADDRESS 0xbf
#define MSI_EC_FN_KEY_LEFT 0x40
#define MSI_EC_FN_KEY_RIGHT 0x50
#define MSI_EC_WIN_KEY_LEFT 0x50
#define MSI_EC_WIN_KEY_RIGHT 0x40
#define MSI_EC_BATTERY_MODE_ADDRESS 0xef
#define MSI_EC_BATTERY_MODE_HIGH_CAPACITY 0xe4
#define MSI_EC_BATTERY_MODE_MEDIUM_CAPACITY 0xd0
#define MSI_EC_BATTERY_MODE_LOW_CAPACITY 0xbc
#define MSI_EC_CHARGE_CONTROL_ADDRESS 0xef
#define MSI_EC_CHARGE_CONTROL_OFFSET_START 0x8a
#define MSI_EC_CHARGE_CONTROL_OFFSET_END 0x80
#define MSI_EC_CHARGE_CONTROL_RANGE_MIN 0x8a
#define MSI_EC_CHARGE_CONTROL_RANGE_MAX 0xe4

#define streq(x, y) (strcmp(x, y) == 0 || strcmp(x, y "\n") == 0)

// ============================================================ //
// Sysfs power_supply subsystem
// ============================================================ //

static ssize_t charge_control_threshold_show(u8 offset,
				struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(MSI_EC_CHARGE_CONTROL_ADDRESS, &rdata);
	if (result < 0)
		return result;

	return sprintf(buf, "%i\n", rdata - offset);
}

static ssize_t charge_control_start_threshold_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	return charge_control_threshold_show(MSI_EC_CHARGE_CONTROL_OFFSET_START, device, attr, buf);
}


static ssize_t charge_control_end_threshold_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	return charge_control_threshold_show(MSI_EC_CHARGE_CONTROL_OFFSET_END, device, attr, buf);
}

static ssize_t charge_control_threshold_store(u8 offset,
				struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	u8 wdata;
	int result;

	result = kstrtou8(buf, 10, &wdata);
	if (result < 0)
		return result;

	wdata += offset;
	if (wdata < MSI_EC_CHARGE_CONTROL_RANGE_MIN || wdata > MSI_EC_CHARGE_CONTROL_RANGE_MAX)
		return -EINVAL;

	result = ec_write(MSI_EC_CHARGE_CONTROL_ADDRESS, wdata);
	if (result < 0)
		return result;

	return count;
}

static ssize_t charge_control_start_threshold_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return charge_control_threshold_store(MSI_EC_CHARGE_CONTROL_OFFSET_START, dev, attr, buf, count);
}

static ssize_t charge_control_end_threshold_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return charge_control_threshold_store(MSI_EC_CHARGE_CONTROL_OFFSET_END, dev, attr, buf, count);
}

static DEVICE_ATTR_RW(charge_control_start_threshold);
static DEVICE_ATTR_RW(charge_control_end_threshold);

static struct attribute *msi_battery_attrs[] = {
	&dev_attr_charge_control_start_threshold.attr,
	&dev_attr_charge_control_end_threshold.attr,
	NULL,
};

ATTRIBUTE_GROUPS(msi_battery);

static int msi_battery_add(struct power_supply *battery)
{
	if (device_add_groups(&battery->dev, msi_battery_groups))
		return -ENODEV;
	return 0;
}

static int msi_battery_remove(struct power_supply *battery)
{
	device_remove_groups(&battery->dev, msi_battery_groups);
	return 0;
}

static struct acpi_battery_hook battery_hook = {
	.add_battery = msi_battery_add,
	.remove_battery = msi_battery_remove,
	.name = MSI_DRIVER_NAME,
};

// ============================================================ //
// Sysfs platform device attributes
// ============================================================ //

static ssize_t webcam_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(MSI_EC_WEBCAM_ADDRESS, &rdata);
	if (result < 0)
		return result;

	switch (rdata) {
		case MSI_EC_WEBCAM_ON:
			return sprintf(buf, "%s\n", "on");
		case MSI_EC_WEBCAM_OFF:
			return sprintf(buf, "%s\n", "off");
		default:
			return sprintf(buf, "%s\n", "unknown");
	}
}


static ssize_t webcam_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int result = -EINVAL;

	if (streq(buf, "on"))
		result = ec_write(MSI_EC_WEBCAM_ADDRESS, MSI_EC_WEBCAM_ON);

	if (streq(buf, "off"))
		result = ec_write(MSI_EC_WEBCAM_ADDRESS, MSI_EC_WEBCAM_OFF);

	if (result < 0)
		return result;

	return count;
}

static ssize_t fn_key_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(MSI_EC_FN_WIN_ADDRESS, &rdata);
	if (result < 0)
		return result;

	switch (rdata) {
		case MSI_EC_FN_KEY_LEFT:
			return sprintf(buf, "%s\n", "left");
		case MSI_EC_FN_KEY_RIGHT:
			return sprintf(buf, "%s\n", "right");
		default:
			return sprintf(buf, "%s\n", "unknown");
	}
}


static ssize_t fn_key_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int result = -EINVAL;

	if (streq(buf, "left"))
		result = ec_write(MSI_EC_FN_WIN_ADDRESS, MSI_EC_FN_KEY_LEFT);

	if (streq(buf, "right"))
		result = ec_write(MSI_EC_FN_WIN_ADDRESS, MSI_EC_FN_KEY_RIGHT);

	if (result < 0)
		return result;

	return count;
}

static ssize_t win_key_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(MSI_EC_FN_WIN_ADDRESS, &rdata);
	if (result < 0)
		return result;

	switch (rdata) {
		case MSI_EC_WIN_KEY_LEFT:
			return sprintf(buf, "%s\n", "left");
		case MSI_EC_WIN_KEY_RIGHT:
			return sprintf(buf, "%s\n", "right");
		default:
			return sprintf(buf, "%s\n", "unknown");
	}
}


static ssize_t win_key_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int result = -EINVAL;

	if (streq(buf, "left"))
		result = ec_write(MSI_EC_FN_WIN_ADDRESS, MSI_EC_WIN_KEY_LEFT);

	if (streq(buf, "right"))
		result = ec_write(MSI_EC_FN_WIN_ADDRESS, MSI_EC_WIN_KEY_RIGHT);

	if (result < 0)
		return result;

	return count;
}

static ssize_t battery_mode_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(MSI_EC_BATTERY_MODE_ADDRESS, &rdata);
	if (result < 0)
		return result;

	switch (rdata) {
		case MSI_EC_BATTERY_MODE_HIGH_CAPACITY:
			return sprintf(buf, "%s\n", "high");
		case MSI_EC_BATTERY_MODE_MEDIUM_CAPACITY:
			return sprintf(buf, "%s\n", "medium");
		case MSI_EC_BATTERY_MODE_LOW_CAPACITY:
			return sprintf(buf, "%s\n", "low");
		default:
			return sprintf(buf, "%s\n", "unknown");
	}
}


static ssize_t battery_mode_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int result = -EINVAL;

	if (streq(buf, "high"))
		result = ec_write(MSI_EC_BATTERY_MODE_ADDRESS, MSI_EC_BATTERY_MODE_HIGH_CAPACITY);

	if (streq(buf, "medium"))
		result = ec_write(MSI_EC_BATTERY_MODE_ADDRESS, MSI_EC_BATTERY_MODE_MEDIUM_CAPACITY);

	if (streq(buf, "low"))
		result = ec_write(MSI_EC_BATTERY_MODE_ADDRESS, MSI_EC_BATTERY_MODE_LOW_CAPACITY);

	if (result < 0)
		return result;

	return count;
}

static DEVICE_ATTR_RW(webcam);
static DEVICE_ATTR_RW(fn_key);
static DEVICE_ATTR_RW(win_key);
static DEVICE_ATTR_RW(battery_mode);

static struct attribute *msi_platform_attrs[] = {
	&dev_attr_webcam.attr,
	&dev_attr_fn_key.attr,
	&dev_attr_win_key.attr,
	&dev_attr_battery_mode.attr,
	NULL,
};

ATTRIBUTE_GROUPS(msi_platform);

static int msi_platform_probe(struct platform_device *pdev)
{
	int result;
	result = sysfs_create_groups(&pdev->dev.kobj, msi_platform_groups);
	if (result < 0)
		return result;
	return 0;
}

static int msi_platform_remove(struct platform_device *pdev)
{
	sysfs_remove_groups(&pdev->dev.kobj, msi_platform_groups);
	return 0;
}

static struct platform_device *msi_platform_device;

static struct platform_driver msi_platform_driver = {
	.driver = {
		.name = MSI_DRIVER_NAME,
	},
	.probe	= msi_platform_probe,
	.remove	= msi_platform_remove,
};

// ============================================================ //
// Module load/unload
// ============================================================ //

static int __init msi_ec_init(void)
{
	int result;

	if (acpi_disabled) {
		pr_err("Unable to init because ACPI needs to be enabled first!\n");
		return -ENODEV;
	}

	result = platform_driver_register(&msi_platform_driver);
	if (result < 0) {
		return result;
	}

	msi_platform_device = platform_device_alloc(MSI_DRIVER_NAME, -1);
	if (msi_platform_device == NULL) {
		platform_driver_unregister(&msi_platform_driver);
		return -ENOMEM;
	}

	result = platform_device_add(msi_platform_device);
	if (result < 0) {
		platform_device_del(msi_platform_device);
		platform_driver_unregister(&msi_platform_driver);
		return result;
	}

	battery_hook_register(&battery_hook);

	pr_info("msi-ec: module_init\n");
	return 0;
}

static void __exit msi_ec_exit(void)
{
	platform_driver_unregister(&msi_platform_driver);
	platform_device_del(msi_platform_device);
	battery_hook_unregister(&battery_hook);

	pr_info("msi-ec: module_exit\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jose Angel Pastrana <japp0005@red.ujaen.es>");
MODULE_DESCRIPTION("MSI Embedded Controller");
MODULE_VERSION("0.05");

module_init(msi_ec_init);
module_exit(msi_ec_exit);
