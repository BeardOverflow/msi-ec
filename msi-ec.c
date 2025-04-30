// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * msi-ec.c - Embedded Controller driver for MSI laptops.
 *
 * This driver registers a platform driver at /sys/devices/platform/msi-ec
 * The list of supported attributes is available in the docs/sysfs-platform-msi-ec file
 *
 * In addition to these platform device attributes the driver
 * registers itself in the Linux power_supply subsystem and is
 * available to userspace under /sys/class/power_supply/<power_supply>:
 *
 *   charge_control_start_threshold
 *   charge_control_end_threshold
 *
 * This driver also registers available led class devices for
 * mute, micmute and keyboard_backlight leds
 *
 * This driver might not work on other laptops produced by MSI. Also, and until
 * future enhancements, no DMI data are used to identify your compatibility
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "ec_memory_configuration.h"

#include <acpi/battery.h>
#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/rtc.h>
#include <linux/string_choices.h>

static DEFINE_MUTEX(ec_set_by_mask_mutex);
static DEFINE_MUTEX(ec_unset_by_mask_mutex);
static DEFINE_MUTEX(ec_set_bit_mutex);

#define SM_ECO_NAME "eco"
#define SM_COMFORT_NAME "comfort"
#define SM_SPORT_NAME "sport"
#define SM_TURBO_NAME "turbo"

#define FM_AUTO_NAME "auto"
#define FM_SILENT_NAME "silent"
#define FM_BASIC_NAME "basic"
#define FM_ADVANCED_NAME "advanced"

static const char *ALLOWED_FW_49[] __initconst = {
	"16R8IMS2.112", // MSI Thin 15 B12VE
	NULL
};

static struct msi_ec_conf CONF49 __initdata = {
	.allowed_fw = ALLOWED_FW_49,
	.charge_control_address = 0xd7,
	.fn_win_swap = {
		.address = 0xe8,
		.bit = 4,
		.invert = true,
	},
	.shift_mode = {
		.address = 0xd2,
		.modes = {
			{ SM_ECO_NAME, 0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_TURBO_NAME, 0xc4 },
			MSI_EC_MODE_NULL,
		},
	},
	.super_battery = {
		.address = MSI_EC_ADDR_UNSUPP,
	},
	.cooler_boost = {
		.address = MSI_EC_ADDR_UNSUPP,
	},
	.fan_mode = {
		.address = 0xd4,
		.modes = {
			{ FM_AUTO_NAME,	0x0d },
			{ FM_ADVANCED_NAME, 0x8d },
		},
	},
	.cpu = {
		.rt_temp_address = 0x68,
		.rt_fan_speed_address = 0x89,
	},
	.gpu = {
		.rt_temp_address = 0x80,
		.rt_fan_speed_address = 0x89,
	},
	.kbd_bl = {
		.bl_mode_address = MSI_EC_ADDR_UNSUPP,
		.bl_modes = { },
		.max_mode = 1,
		.bl_state_address = 0xd3,
		.state_base_value = 0x80,
		.max_state = 3,
	},
};

static struct msi_ec_conf *CONFIGURATIONS[] __initdata = {
	&CONF49,
	NULL
};

static bool conf_loaded = false;
static struct msi_ec_conf conf; // current configuration

static bool charge_control_supported = false;

static char *firmware = NULL;
module_param(firmware, charp, 0);
MODULE_PARM_DESC(firmware,
		 "Load a configuration for a specified firmware version");

static bool debug = false;
module_param(debug, bool, 0);
MODULE_PARM_DESC(
	debug,
	"Load the driver in the debug mode, exporting the debug attributes");

// ============================================================ //
// Helper functions
// ============================================================ //

static int ec_read_seq(u8 addr, u8 *buf, u8 len)
{
	int result;
	for (u8 i = 0; i < len; i++) {
		result = ec_read(addr + i, buf + i);
		if (result < 0)
			return result;
	}
	return 0;
}

static int ec_set_by_mask(u8 addr, u8 mask)
{
	int result;
	u8 stored;

	mutex_lock(&ec_set_by_mask_mutex);
	result = ec_read(addr, &stored);
	if (result < 0)
		goto unlock;

	stored |= mask;
	result = ec_write(addr, stored);

unlock:
	mutex_unlock(&ec_set_by_mask_mutex);
	return result;
}

static int ec_unset_by_mask(u8 addr, u8 mask)
{
	int result;
	u8 stored;

	mutex_lock(&ec_unset_by_mask_mutex);
	result = ec_read(addr, &stored);
	if (result < 0)
		goto unlock;

	stored &= ~mask;
	result = ec_write(addr, stored);

unlock:
	mutex_unlock(&ec_unset_by_mask_mutex);
	return result;
}

static int ec_check_by_mask(u8 addr, u8 mask, bool *output)
{
	int result;
	u8 stored;

	result = ec_read(addr, &stored);
	if (result < 0)
		return result;

	*output = ((stored & mask) == mask);

	return 0;
}

static int ec_set_bit(u8 addr, u8 bit, bool value)
{
	int result;
	u8 stored;

	mutex_lock(&ec_set_bit_mutex);
	result = ec_read(addr, &stored);
	if (result < 0)
		goto unlock;

	if (value)
		stored |= BIT(bit);
	else
		stored &= ~BIT(bit);

	result = ec_write(addr, stored);

unlock:
	mutex_unlock(&ec_set_bit_mutex);
	return result;
}

static int ec_check_bit(u8 addr, u8 bit, bool *output)
{
	int result;
	u8 stored;

	result = ec_read(addr, &stored);
	if (result < 0)
		return result;

	*output = stored & BIT(bit);

	return 0;
}

static int ec_get_firmware_version(u8 buf[MSI_EC_FW_VERSION_LENGTH + 1])
{
	int result;

	memset(buf, 0, MSI_EC_FW_VERSION_LENGTH + 1);
	result = ec_read_seq(MSI_EC_FW_VERSION_ADDRESS, buf,
			     MSI_EC_FW_VERSION_LENGTH);
	if (result < 0)
		return result;

	return MSI_EC_FW_VERSION_LENGTH + 1;
}

static inline const char *str_left_right(bool v)
{
	return v ? "left" : "right";
}

static int direction_is_left(const char *s, bool *res)
{
	if (!s)
		return -EINVAL;

	switch (s[0]) {
	case 'l':
	case 'L':
		*res = true;
		return 0;
	case 'r':
	case 'R':
		*res = false;
		return 0;
	default:
		break;
	}

	return -EINVAL;
}

// ============================================================ //
// Sysfs power_supply subsystem
// ============================================================ //

static int get_end_threshold(u8 *out)
{
	u8 rdata;
	int result;

	result = ec_read(conf.charge_control_address, &rdata);
	if (result < 0)
		return result;

	rdata &= ~BIT(7); // last 7 bits contain the threshold

	// the thresholds are unknown
	if (rdata == 0)
		return -ENODATA;

	if (rdata < 10 || rdata > 100)
		return -EINVAL;

	*out = rdata;
	return 0;
}

static int set_end_threshold(u8 value)
{
	if (value < 10 || value > 100)
		return -EINVAL;

	return ec_write(conf.charge_control_address, value | BIT(7));
}

static ssize_t
charge_control_start_threshold_show(struct device *device,
				    struct device_attribute *attr, char *buf)
{
	int result;
	u8 threshold;

	result = get_end_threshold(&threshold);

	if (result == -ENODATA)
		return sysfs_emit(buf, "0\n");
	else if (result < 0)
		return result;

	return sysfs_emit(buf, "%u\n", threshold - 10);
}

static ssize_t
charge_control_start_threshold_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int result;
	u8 threshold;

	result = kstrtou8(buf, 10, &threshold);
	if (result < 0)
		return result;

	result = set_end_threshold(threshold + 10);
	if (result < 0)
		return result;

	return count;
}

static ssize_t charge_control_end_threshold_show(struct device *device,
						 struct device_attribute *attr,
						 char *buf)
{
	int result;
	u8 threshold;

	result = get_end_threshold(&threshold);

	if (result == -ENODATA)
		return sysfs_emit(buf, "0\n");
	else if (result < 0)
		return result;

	return sysfs_emit(buf, "%u\n", threshold);
}

static ssize_t charge_control_end_threshold_store(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t count)
{
	int result;
	u8 threshold;

	result = kstrtou8(buf, 10, &threshold);
	if (result < 0)
		return result;

	result = set_end_threshold(threshold);
	if (result < 0)
		return result;

	return count;
}

static DEVICE_ATTR_RW(charge_control_start_threshold);
static DEVICE_ATTR_RW(charge_control_end_threshold);

static struct attribute *msi_battery_attrs[] = {
	&dev_attr_charge_control_start_threshold.attr,
	&dev_attr_charge_control_end_threshold.attr, NULL
};

ATTRIBUTE_GROUPS(msi_battery);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0))
static int msi_battery_add(struct power_supply *battery,
			   struct acpi_battery_hook *hook)
#else
static int msi_battery_add(struct power_supply *battery)
#endif
{
	return device_add_groups(&battery->dev, msi_battery_groups);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0))
static int msi_battery_remove(struct power_supply *battery,
			      struct acpi_battery_hook *hook)
#else
static int msi_battery_remove(struct power_supply *battery)
#endif
{
	device_remove_groups(&battery->dev, msi_battery_groups);
	return 0;
}

static struct acpi_battery_hook battery_hook = {
	.add_battery = msi_battery_add,
	.remove_battery = msi_battery_remove,
	.name = MSI_EC_DRIVER_NAME,
};

// ============================================================ //
// Sysfs platform device attributes (root)
// ============================================================ //

static ssize_t webcam_common_show(u8 address, char *buf, bool inverted)
{
	int result;
	bool value;

	result = ec_check_bit(address, conf.webcam.bit, &value);
	if (result < 0)
		return result;

	return sysfs_emit(buf, "%s\n", str_on_off(value ^ inverted));
}

static ssize_t webcam_common_store(u8 address, const char *buf, size_t count,
				   bool inverted)
{
	int result;
	bool value;

	result = kstrtobool(buf, &value);
	if (result)
		return result;

	result = ec_set_bit(address, conf.webcam.bit, value ^ inverted);
	if (result < 0)
		return result;

	return count;
}

static ssize_t webcam_show(struct device *device, struct device_attribute *attr,
			   char *buf)
{
	return webcam_common_show(conf.webcam.address, buf, false);
}

static ssize_t webcam_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	return webcam_common_store(conf.webcam.address, buf, count, false);
}

static ssize_t webcam_block_show(struct device *device,
				 struct device_attribute *attr, char *buf)
{
	return webcam_common_show(conf.webcam.block_address, buf, true);
}

static ssize_t webcam_block_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	return webcam_common_store(conf.webcam.block_address, buf, count, true);
}

static ssize_t fn_key_show(struct device *device, struct device_attribute *attr,
			   char *buf)
{
	int result;
	bool value;

	result = ec_check_bit(conf.fn_win_swap.address, conf.fn_win_swap.bit,
			      &value);
	if (result < 0)
		return result;

	value ^=
		conf.fn_win_swap.invert; // invert the direction for some laptops
	value = !value; // fn key position is the opposite of win key

	return sysfs_emit(buf, "%s\n", str_left_right(value));
}

static ssize_t fn_key_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int result;
	bool value;

	result = direction_is_left(buf, &value);
	if (result < 0)
		return result;

	value ^=
		conf.fn_win_swap.invert; // invert the direction for some laptops
	value = !value; // fn key position is the opposite of win key

	result = ec_set_bit(conf.fn_win_swap.address, conf.fn_win_swap.bit,
			    value);

	if (result < 0)
		return result;

	return count;
}

static ssize_t win_key_show(struct device *device,
			    struct device_attribute *attr, char *buf)
{
	int result;
	bool value;

	result = ec_check_bit(conf.fn_win_swap.address, conf.fn_win_swap.bit,
			      &value);
	if (result < 0)
		return result;

	value ^=
		conf.fn_win_swap.invert; // invert the direction for some laptops

	return sysfs_emit(buf, "%s\n", str_left_right(value));
}

static ssize_t win_key_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int result;
	bool value;

	result = direction_is_left(buf, &value);
	if (result < 0)
		return result;

	value ^=
		conf.fn_win_swap.invert; // invert the direction for some laptops

	result = ec_set_bit(conf.fn_win_swap.address, conf.fn_win_swap.bit,
			    value);

	if (result < 0)
		return result;

	return count;
}

static ssize_t cooler_boost_show(struct device *device,
				 struct device_attribute *attr, char *buf)
{
	int result;
	bool value;

	result = ec_check_bit(conf.cooler_boost.address, conf.cooler_boost.bit,
			      &value);
	if (result < 0)
		return result;

	return sysfs_emit(buf, "%s\n", str_on_off(value));
}

static ssize_t cooler_boost_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int result;
	bool value;

	result = kstrtobool(buf, &value);
	if (result)
		return result;

	result = ec_set_bit(conf.cooler_boost.address, conf.cooler_boost.bit,
			    value);
	if (result < 0)
		return result;

	return count;
}

static ssize_t available_shift_modes_show(struct device *device,
					  struct device_attribute *attr,
					  char *buf)
{
	int result = 0;
	int count = 0;

	for (int i = 0; conf.shift_mode.modes[i].name; i++) {
		// NULL entries have NULL name

		result = sysfs_emit_at(buf, count, "%s\n",
				       conf.shift_mode.modes[i].name);
		if (result < 0)
			return result;
		count += result;
	}

	return count;
}

static ssize_t shift_mode_show(struct device *device,
			       struct device_attribute *attr, char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(conf.shift_mode.address, &rdata);
	if (result < 0)
		return result;

	if (rdata == 0x80)
		return sysfs_emit(buf, "%s\n", "unspecified");

	for (int i = 0; conf.shift_mode.modes[i].name; i++) {
		// NULL entries have NULL name

		if (rdata == conf.shift_mode.modes[i].value) {
			return sysfs_emit(buf, "%s\n",
					  conf.shift_mode.modes[i].name);
		}
	}

	return sysfs_emit(buf, "%s (%i)\n", "unknown", rdata);
}

static ssize_t shift_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int result;

	for (int i = 0; conf.shift_mode.modes[i].name; i++) {
		// NULL entries have NULL name

		if (sysfs_streq(conf.shift_mode.modes[i].name, buf)) {
			result = ec_write(conf.shift_mode.address,
					  conf.shift_mode.modes[i].value);
			if (result < 0)
				return result;

			return count;
		}
	}

	return -EINVAL;
}

static ssize_t super_battery_show(struct device *device,
				  struct device_attribute *attr, char *buf)
{
	int result;
	bool enabled;

	result = ec_check_by_mask(conf.super_battery.address,
				  conf.super_battery.mask, &enabled);
	if (result < 0)
		return result;

	return sysfs_emit(buf, "%s\n", str_on_off(enabled));
}

static ssize_t super_battery_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int result;
	bool value;

	result = kstrtobool(buf, &value);
	if (result)
		return result;

	if (value)
		result = ec_set_by_mask(conf.super_battery.address,
					conf.super_battery.mask);
	else
		result = ec_unset_by_mask(conf.super_battery.address,
					  conf.super_battery.mask);

	if (result < 0)
		return result;

	return count;
}

static ssize_t available_fan_modes_show(struct device *device,
					struct device_attribute *attr,
					char *buf)
{
	int result = 0;
	int count = 0;

	for (int i = 0; conf.fan_mode.modes[i].name; i++) {
		// NULL entries have NULL name

		result = sysfs_emit_at(buf, count, "%s\n",
				       conf.fan_mode.modes[i].name);
		if (result < 0)
			return result;
		count += result;
	}

	return count;
}

static ssize_t fan_mode_show(struct device *device,
			     struct device_attribute *attr, char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(conf.fan_mode.address, &rdata);
	if (result < 0)
		return result;

	for (int i = 0; conf.fan_mode.modes[i].name; i++) {
		// NULL entries have NULL name

		if (rdata == conf.fan_mode.modes[i].value) {
			return sysfs_emit(buf, "%s\n",
					  conf.fan_mode.modes[i].name);
		}
	}

	return sysfs_emit(buf, "%s (%i)\n", "unknown", rdata);
}

static ssize_t fan_mode_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int result;

	for (int i = 0; conf.fan_mode.modes[i].name; i++) {
		// NULL entries have NULL name

		if (sysfs_streq(conf.fan_mode.modes[i].name, buf)) {
			result = ec_write(conf.fan_mode.address,
					  conf.fan_mode.modes[i].value);
			if (result < 0)
				return result;

			return count;
		}
	}

	return -EINVAL;
}

static ssize_t fw_version_show(struct device *device,
			       struct device_attribute *attr, char *buf)
{
	u8 rdata[MSI_EC_FW_VERSION_LENGTH + 1];
	int result;

	result = ec_get_firmware_version(rdata);
	if (result < 0)
		return result;

	return sysfs_emit(buf, "%s\n", rdata);
}

static ssize_t fw_release_date_show(struct device *device,
				    struct device_attribute *attr, char *buf)
{
	u8 rdate[MSI_EC_FW_DATE_LENGTH + 1];
	u8 rtime[MSI_EC_FW_TIME_LENGTH + 1];
	int result;
	struct rtc_time time;

	memset(rdate, 0, sizeof(rdate));
	result = ec_read_seq(MSI_EC_FW_DATE_ADDRESS, rdate,
			     MSI_EC_FW_DATE_LENGTH);
	if (result < 0)
		return result;

	result = sscanf(rdate, "%02d%02d%04d", &time.tm_mon, &time.tm_mday,
			&time.tm_year);
	if (result != 3)
		return -ENODATA;

	/* the number of months since January and number of years since 1900 */
	time.tm_mon -= 1;
	time.tm_year -= 1900;

	memset(rtime, 0, sizeof(rtime));
	result = ec_read_seq(MSI_EC_FW_TIME_ADDRESS, rtime,
			     MSI_EC_FW_TIME_LENGTH);
	if (result < 0)
		return result;

	result = sscanf(rtime, "%02d:%02d:%02d", &time.tm_hour, &time.tm_min,
			&time.tm_sec);
	if (result != 3)
		return -ENODATA;

	return sysfs_emit(buf, "%ptR\n", &time);
}

static DEVICE_ATTR_RW(webcam);
static DEVICE_ATTR_RW(webcam_block);
static DEVICE_ATTR_RW(fn_key);
static DEVICE_ATTR_RW(win_key);
static DEVICE_ATTR_RW(cooler_boost);
static DEVICE_ATTR_RO(available_shift_modes);
static DEVICE_ATTR_RW(shift_mode);
static DEVICE_ATTR_RW(super_battery);
static DEVICE_ATTR_RO(available_fan_modes);
static DEVICE_ATTR_RW(fan_mode);
static DEVICE_ATTR_RO(fw_version);
static DEVICE_ATTR_RO(fw_release_date);

static struct attribute *msi_root_attrs[] = {
	&dev_attr_webcam.attr,
	&dev_attr_webcam_block.attr,
	&dev_attr_fn_key.attr,
	&dev_attr_win_key.attr,
	&dev_attr_cooler_boost.attr,
	&dev_attr_available_shift_modes.attr,
	&dev_attr_shift_mode.attr,
	&dev_attr_super_battery.attr,
	&dev_attr_available_fan_modes.attr,
	&dev_attr_fan_mode.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_fw_release_date.attr,
	NULL
};

// ============================================================ //
// Sysfs platform device attributes (cpu)
// ============================================================ //

static ssize_t cpu_realtime_temperature_show(struct device *device,
					     struct device_attribute *attr,
					     char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(conf.cpu.rt_temp_address, &rdata);
	if (result < 0)
		return result;

	return sysfs_emit(buf, "%i\n", rdata);
}

static ssize_t cpu_realtime_fan_speed_show(struct device *device,
					   struct device_attribute *attr,
					   char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(conf.cpu.rt_fan_speed_address, &rdata);
	if (result < 0)
		return result;

	return sysfs_emit(buf, "%i\n", rdata);
}

static struct device_attribute dev_attr_cpu_realtime_temperature = {
	.attr = {
		.name = "realtime_temperature",
		.mode = 0444,
	},
	.show = cpu_realtime_temperature_show,
};

static struct device_attribute dev_attr_cpu_realtime_fan_speed = {
	.attr = {
		.name = "realtime_fan_speed",
		.mode = 0444,
	},
	.show = cpu_realtime_fan_speed_show,
};

static struct attribute *msi_cpu_attrs[] = {
	&dev_attr_cpu_realtime_temperature.attr,
	&dev_attr_cpu_realtime_fan_speed.attr, NULL
};

// ============================================================ //
// Sysfs platform device attributes (gpu)
// ============================================================ //

static ssize_t gpu_realtime_temperature_show(struct device *device,
					     struct device_attribute *attr,
					     char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(conf.gpu.rt_temp_address, &rdata);
	if (result < 0)
		return result;

	return sysfs_emit(buf, "%i\n", rdata);
}

static ssize_t gpu_realtime_fan_speed_show(struct device *device,
					   struct device_attribute *attr,
					   char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(conf.gpu.rt_fan_speed_address, &rdata);
	if (result < 0)
		return result;

	return sysfs_emit(buf, "%i\n", rdata);
}

static struct device_attribute dev_attr_gpu_realtime_temperature = {
	.attr = {
		.name = "realtime_temperature",
		.mode = 0444,
	},
	.show = gpu_realtime_temperature_show,
};

static struct device_attribute dev_attr_gpu_realtime_fan_speed = {
	.attr = {
		.name = "realtime_fan_speed",
		.mode = 0444,
	},
	.show = gpu_realtime_fan_speed_show,
};

static struct attribute *msi_gpu_attrs[] = {
	&dev_attr_gpu_realtime_temperature.attr,
	&dev_attr_gpu_realtime_fan_speed.attr, NULL
};

// ============================================================ //
// Sysfs platform device attributes (debug)
// ============================================================ //

// Prints an EC memory dump in form of a table
static ssize_t ec_dump_show(struct device *device,
			    struct device_attribute *attr, char *buf)
{
	int count = 0;
	char ascii_row[16]; // not null-terminated

	// print header
	count += sysfs_emit(
		buf,
		"|      | _0 _1 _2 _3 _4 _5 _6 _7 _8 _9 _a _b _c _d _e _f\n"
		"|------+------------------------------------------------\n");

	// print dump
	for (u8 i = 0x0; i <= 0xf; i++) {
		u8 addr_base = i * 16;

		count += sysfs_emit_at(buf, count, "| %#x_ |", i);
		for (u8 j = 0x0; j <= 0xf; j++) {
			u8 rdata;
			int result = ec_read(addr_base + j, &rdata);
			if (result < 0)
				return result;

			count += sysfs_emit_at(buf, count, " %02x", rdata);
			ascii_row[j] =
				isascii(rdata) && isgraph(rdata) ? rdata : '.';
		}

		count += sysfs_emit_at(buf, count, "  |%.16s|\n", ascii_row);
	}

	return count;
}

// stores a value in the specified EC memory address. Format: "xx=xx", xx - hex u8
static ssize_t ec_set_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	if (count > 6) // "xx=xx\n" - 6 chars
		return -EINVAL;

	int result;

	char addr_s[3], val_s[3];
	result = sscanf(buf, "%2s=%2s", addr_s, val_s);
	if (result != 2)
		return -EINVAL;

	u8 addr, val;

	// convert addr
	result = kstrtou8(addr_s, 16, &addr);
	if (result < 0)
		return result;

	// convert val
	result = kstrtou8(val_s, 16, &val);
	if (result < 0)
		return result;

	// write val to EC[addr]
	result = ec_write(addr, val);
	if (result < 0)
		return result;

	return count;
}

// ec_get. stores the specified EC memory address. MAY BE UNSAFE!!!
static u8 ec_get_addr;

// ec_get. reads and stores the specified EC memory address. Format: "xx", xx - hex u8
static ssize_t ec_get_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	if (count > 3) // "xx\n" - 3 chars
		return -EINVAL;

	int result;
	char addr_s[3];

	result = sscanf(buf, "%2s", addr_s);
	if (result != 1)
		return -EINVAL;

	// convert addr
	result = kstrtou8(addr_s, 16, &ec_get_addr);
	if (result < 0)
		return result;

	return count;
};

// ec_get. prints value of previously stored EC memory address
static ssize_t ec_get_show(struct device *device, struct device_attribute *attr,
			   char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(ec_get_addr, &rdata);
	if (result < 0)
		return result;

	//	return sysfs_emit(buf, "%02x=%02x\n", ec_get_addr, rdata);
	return sysfs_emit(buf, "%02x\n", rdata);
};

static DEVICE_ATTR_RO(ec_dump);
static DEVICE_ATTR_WO(ec_set);
static DEVICE_ATTR_RW(ec_get);

static struct attribute *msi_debug_attrs[] = { &dev_attr_fw_version.attr,
					       &dev_attr_ec_dump.attr,
					       &dev_attr_ec_set.attr,
					       &dev_attr_ec_get.attr, NULL };

// ============================================================ //
// Sysfs leds subsystem
// ============================================================ //

static int micmute_led_sysfs_set(struct led_classdev *led_cdev,
				 enum led_brightness brightness)
{
	int result;

	result = ec_set_bit(conf.leds.micmute_led_address, conf.leds.bit,
			    brightness);

	if (result < 0)
		return result;

	return 0;
}

static int mute_led_sysfs_set(struct led_classdev *led_cdev,
			      enum led_brightness brightness)
{
	int result;

	result = ec_set_bit(conf.leds.mute_led_address, conf.leds.bit,
			    brightness);

	if (result < 0)
		return result;

	return 0;
}

static enum led_brightness kbd_bl_sysfs_get(struct led_classdev *led_cdev)
{
	u8 rdata;
	int result = ec_read(conf.kbd_bl.bl_state_address, &rdata);
	if (result < 0)
		return 0;
	return rdata & MSI_EC_KBD_BL_STATE_MASK;
}

static int kbd_bl_sysfs_set(struct led_classdev *led_cdev,
			    enum led_brightness brightness)
{
	// By default, on an unregister event,
	// kernel triggers the setter with 0 brightness.
	if (led_cdev->flags & LED_UNREGISTERING)
		return 0;

	u8 wdata;
	if (brightness < 0 || brightness > 3)
		return -1;
	wdata = conf.kbd_bl.state_base_value | brightness;
	return ec_write(conf.kbd_bl.bl_state_address, wdata);
}

static struct led_classdev micmute_led_cdev = {
	.name = "platform::micmute",
	.max_brightness = 1,
	.brightness_set_blocking = &micmute_led_sysfs_set,
	.default_trigger = "audio-micmute",
};

static struct led_classdev mute_led_cdev = {
	.name = "platform::mute",
	.max_brightness = 1,
	.brightness_set_blocking = &mute_led_sysfs_set,
	.default_trigger = "audio-mute",
};

static struct led_classdev msiacpi_led_kbdlight = {
	.name = "msiacpi::kbd_backlight",
	.max_brightness = 3,
	.flags = LED_BRIGHT_HW_CHANGED,
	.brightness_set_blocking = &kbd_bl_sysfs_set,
	.brightness_get = &kbd_bl_sysfs_get,
};

// ============================================================ //
// Sysfs platform driver
// ============================================================ //

static umode_t msi_ec_is_visible(struct kobject *kobj, struct attribute *attr,
				 int idx)
{
	int address;

	if (!conf_loaded)
		return 0;

	/* root group */
	if (attr == &dev_attr_webcam.attr)
		address = conf.webcam.address;

	else if (attr == &dev_attr_webcam_block.attr)
		address = conf.webcam.block_address;

	else if (attr == &dev_attr_fn_key.attr ||
		 attr == &dev_attr_win_key.attr)
		address = conf.fn_win_swap.address;

	else if (attr == &dev_attr_cooler_boost.attr)
		address = conf.cooler_boost.address;

	else if (attr == &dev_attr_available_shift_modes.attr ||
		 attr == &dev_attr_shift_mode.attr)
		address = conf.shift_mode.address;

	else if (attr == &dev_attr_super_battery.attr)
		address = conf.super_battery.address;

	else if (attr == &dev_attr_available_fan_modes.attr ||
		 attr == &dev_attr_fan_mode.attr)
		address = conf.fan_mode.address;

	/* cpu group */
	else if (attr == &dev_attr_cpu_realtime_temperature.attr)
		address = conf.cpu.rt_temp_address;

	else if (attr == &dev_attr_cpu_realtime_fan_speed.attr)
		address = conf.cpu.rt_fan_speed_address;

	/* gpu group */
	else if (attr == &dev_attr_gpu_realtime_temperature.attr)
		address = conf.gpu.rt_temp_address;

	else if (attr == &dev_attr_gpu_realtime_fan_speed.attr)
		address = conf.gpu.rt_fan_speed_address;

	/* default */
	else
		return attr->mode;

	return address == MSI_EC_ADDR_UNSUPP ? 0 : attr->mode;
}

static struct attribute_group msi_root_group = {
	.is_visible = msi_ec_is_visible,
	.attrs = msi_root_attrs,
};

static struct attribute_group msi_cpu_group = {
	.name = "cpu",
	.is_visible = msi_ec_is_visible,
	.attrs = msi_cpu_attrs,
};
static struct attribute_group msi_gpu_group = {
	.name = "gpu",
	.is_visible = msi_ec_is_visible,
	.attrs = msi_gpu_attrs,
};

static const struct attribute_group msi_debug_group = {
	.name = "debug",
	.attrs = msi_debug_attrs,
};

/* the debug group is created separately if needed */
static const struct attribute_group *msi_platform_groups[] = {
	&msi_root_group, &msi_cpu_group, &msi_gpu_group, NULL
};

static int __init msi_platform_probe(struct platform_device *pdev)
{
	if (debug) {
		int result =
			sysfs_create_group(&pdev->dev.kobj, &msi_debug_group);
		if (result < 0)
			return result;
	}

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0))
static void msi_platform_remove(struct platform_device *pdev)
#else
static int msi_platform_remove(struct platform_device *pdev)
#endif
{
	if (debug)
		sysfs_remove_group(&pdev->dev.kobj, &msi_debug_group);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0))
	return 0;
#endif
}

static struct platform_device *msi_platform_device;

static struct platform_driver msi_platform_driver = {
	.driver = {
		.name = MSI_EC_DRIVER_NAME,
		.dev_groups = msi_platform_groups,
	},
	.remove = msi_platform_remove,
};

// ============================================================ //
// Module load/unload
// ============================================================ //

// must be called before msi_platform_probe()
static int __init load_configuration(void)
{
	int result;

	char *ver;
	char ver_by_ec[MSI_EC_FW_VERSION_LENGTH +
		       1]; // to store version read from EC

	if (firmware) {
		// use fw version passed as a parameter
		ver = firmware;
	} else {
		// get fw version from EC
		result = ec_get_firmware_version(ver_by_ec);
		if (result < 0)
			return result;

		ver = ver_by_ec;
	}

	// load the suitable configuration, if exists
	for (int i = 0; CONFIGURATIONS[i]; i++) {
		if (match_string(CONFIGURATIONS[i]->allowed_fw, -1, ver) !=
		    -EINVAL) {
			memcpy(&conf, CONFIGURATIONS[i],
			       sizeof(struct msi_ec_conf));
			conf.allowed_fw = NULL;
			conf_loaded = true;
			return 0;
		}
	}

	// debug mode works regardless of whether the firmware is supported
	if (debug)
		return 0;

	pr_err("Your firmware version is not supported!\n");
	return -EOPNOTSUPP;
}

static int __init msi_ec_init(void)
{
	int result;

	result = load_configuration();
	if (result < 0)
		return result;

	msi_platform_device = platform_create_bundle(
		&msi_platform_driver, msi_platform_probe, NULL, 0, NULL, 0);
	if (IS_ERR(msi_platform_device))
		return PTR_ERR(msi_platform_device);

	pr_info("module_init\n");
	if (!conf_loaded)
		return 0;

	/*
	 * Additional check: battery thresholds are supported only if
	 * the 7th bit is set.
	 */
	if (conf.charge_control_address != MSI_EC_ADDR_UNSUPP) {
		result = ec_check_bit(conf.charge_control_address, 7,
				      &charge_control_supported);
		if (result < 0)
			return result;
	}

	if (charge_control_supported)
		battery_hook_register(&battery_hook);

	// register LED classdevs
	if (conf.leds.micmute_led_address != MSI_EC_ADDR_UNSUPP)
		led_classdev_register(&msi_platform_device->dev,
				      &micmute_led_cdev);

	if (conf.leds.mute_led_address != MSI_EC_ADDR_UNSUPP)
		led_classdev_register(&msi_platform_device->dev,
				      &mute_led_cdev);

	if (conf.kbd_bl.bl_state_address != MSI_EC_ADDR_UNSUPP)
		led_classdev_register(&msi_platform_device->dev,
				      &msiacpi_led_kbdlight);

	return 0;
}

static void __exit msi_ec_exit(void)
{
	if (conf_loaded) {
		// unregister LED classdevs
		if (conf.leds.micmute_led_address != MSI_EC_ADDR_UNSUPP)
			led_classdev_unregister(&micmute_led_cdev);

		if (conf.leds.mute_led_address != MSI_EC_ADDR_UNSUPP)
			led_classdev_unregister(&mute_led_cdev);

		if (conf.kbd_bl.bl_state_address != MSI_EC_ADDR_UNSUPP)
			led_classdev_unregister(&msiacpi_led_kbdlight);

		if (charge_control_supported)
			battery_hook_unregister(&battery_hook);
	}

	platform_device_unregister(msi_platform_device);
	platform_driver_unregister(&msi_platform_driver);

	pr_info("module_exit\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jose Angel Pastrana <japp0005@red.ujaen.es>");
MODULE_AUTHOR("Aakash Singh <mail@singhaakash.dev>");
MODULE_AUTHOR("Nikita Kravets <teackot@gmail.com>");
MODULE_DESCRIPTION("MSI Embedded Controller");
MODULE_VERSION("0.09");

module_init(msi_ec_init);
module_exit(msi_ec_exit);
