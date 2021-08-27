// SPDX-License-Identifier: GPL-2.0-or-later

#include <acpi/battery.h>
#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define MSI_PROC_DIR "msi-ec"
#define MSI_PROC_VERSION 1
#define MSI_EC_SWITCH_WEBCAM 0x2e
#define MSI_EC_SWITCH_WEBCAM_ON 0x4a
#define MSI_EC_SWITCH_WEBCAM_OFF 0x48
#define MSI_EC_CHARGE_CONTROL_ADDRESS 0xef
#define MSI_EC_CHARGE_CONTROL_OFFSET_START 0x8a
#define MSI_EC_CHARGE_CONTROL_OFFSET_END 0x80
#define MSI_EC_CHARGE_CONTROL_RANGE_MIN 0x8a
#define MSI_EC_CHARGE_CONTROL_RANGE_MAX 0xe4

#define compare_strings(x, y) (strcmp(x, y) == 0 || strcmp(x, y "\n") == 0)

static inline char* kstring(const char *buf, size_t count)
{
	char *kbuf = kmalloc(count + 1, GFP_KERNEL);
	if (kbuf == NULL)
		return NULL;

	if (copy_from_user(kbuf, buf, count) > 0) {
		kfree(kbuf);
		return NULL;
	}

	kbuf[count] = 0;
	return kbuf;
}

static struct proc_dir_entry *msi_proc_dir;

static int __maybe_unused version_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "driver: %s\n", MSI_PROC_DIR);
	seq_printf(m, "version: %d\n", MSI_PROC_VERSION);
	return 0;
}

static int webcam_proc_show(struct seq_file *m, void *v)
{
        u8 rdata;
        int result;

        result = ec_read(MSI_EC_SWITCH_WEBCAM, &rdata);
        if (result < 0)
                return result;

	switch (rdata) {
		case MSI_EC_SWITCH_WEBCAM_ON:
			seq_printf(m, "%s\n", "on");
			break;
		case MSI_EC_SWITCH_WEBCAM_OFF:
			seq_printf(m, "%s\n", "off");
			break;
		default:
			seq_printf(m, "%s\n", "unknown");
			break;
	}

	return 0;
}

static int webcam_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, webcam_proc_show, PDE_DATA(inode));
}

static ssize_t webcam_proc_write(struct file *file, const char __user *buf,
			      size_t count, loff_t *pos)
{
	int result = -EINVAL;

	char *rdata = kstring(buf, count);
	if (rdata == NULL)
		return -EFAULT;

	if (compare_strings(rdata, "on"))
        	result = ec_write(MSI_EC_SWITCH_WEBCAM, MSI_EC_SWITCH_WEBCAM_ON);

	if (compare_strings(rdata, "off"))
        	result = ec_write(MSI_EC_SWITCH_WEBCAM, MSI_EC_SWITCH_WEBCAM_OFF);

	kfree(rdata);

        if (result < 0)
                return result;

	return count;
}

static const struct proc_ops webcam_proc_ops = {
	.proc_open	= webcam_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
	.proc_write	= webcam_proc_write,
};

static void create_msi_proc_entries(void)
{
	proc_create_data("webcam", S_IRUGO | S_IWUSR, msi_proc_dir,
				&webcam_proc_ops, NULL);
	proc_create_single_data("version", S_IRUGO, msi_proc_dir,
				version_proc_show, NULL);
}

static void remove_msi_proc_entries(void)
{
	remove_proc_entry("webcam", msi_proc_dir);
	remove_proc_entry("version", msi_proc_dir);
}

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
	if (acpi_disabled) {
		pr_err("ACPI needs to be enabled for this driver to work!\n");
		return -ENODEV;
	}

	msi_proc_dir = proc_mkdir(MSI_PROC_DIR, acpi_root_dir);
	if (!msi_proc_dir) {
		pr_err("Unable to create proc dir " MSI_PROC_DIR "\n");
		return -ENODEV;
	}
	create_msi_proc_entries();

	pr_info("msi-ec: module_init\n");
	battery_hook_register(&battery_hook);

	return 0;
}

static void __exit msi_ec_module_exit(void)
{
	pr_info("msi-ec: module_exit\n");

	battery_hook_unregister(&battery_hook);

	remove_msi_proc_entries();
	if (msi_proc_dir)
		remove_proc_entry(MSI_PROC_DIR, acpi_root_dir);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jose Angel Pastrana <japp0005@red.ujaen.es>");
MODULE_DESCRIPTION("MSI Embedded Controller");
MODULE_VERSION("0.01");

module_init(msi_ec_module_init);
module_exit(msi_ec_module_exit);
