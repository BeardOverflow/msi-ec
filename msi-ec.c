// SPDX-License-Identifier: GPL-2.0-or-later

#include <acpi/battery.h>
#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#define MSI_EC_CHARGE_CONTROL_ADDRESS 0xef
#define MSI_EC_CHARGE_CONTROL_OFFSET_START 0x8a
#define MSI_EC_CHARGE_CONTROL_OFFSET_END 0x80
#define MSI_EC_CHARGE_CONTROL_RANGE_MIN 0x8a
#define MSI_EC_CHARGE_CONTROL_RANGE_MAX 0xe4

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

static struct attribute *msi_ec_battery_attrs[] = {
	&dev_attr_charge_control_start_threshold.attr,
	&dev_attr_charge_control_end_threshold.attr,
	NULL,
};

ATTRIBUTE_GROUPS(msi_ec_battery);

static int msi_ec_battery_add(struct power_supply *battery)
{
	if (device_add_groups(&battery->dev, msi_ec_battery_groups))
		return -ENODEV;
	return 0;
}

static int msi_ec_battery_remove(struct power_supply *battery)
{
	device_remove_groups(&battery->dev, msi_ec_battery_groups);
	return 0;
}

static struct acpi_battery_hook battery_hook = {
	.add_battery = msi_ec_battery_add,
	.remove_battery = msi_ec_battery_remove,
	.name = "msi-ec: battery extension",
};

static int __init msi_ec_module_init(void)
{
	pr_info("msi-ec: module_init\n");
	battery_hook_register(&battery_hook);
	return 0;
}

static void __exit msi_ec_module_exit(void)
{
	pr_info("msi-ec: module_exit\n");
	battery_hook_unregister(&battery_hook);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jose Angel Pastrana <japp0005@red.ujaen.es>");
MODULE_DESCRIPTION("MSI Embedded Controller");
MODULE_VERSION("0.01");

module_init(msi_ec_module_init);
module_exit(msi_ec_module_exit);
