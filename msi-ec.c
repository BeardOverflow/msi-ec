// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * msi-ec.c - MSI Embedded Controller for laptops support.
 *
 * This driver exports a few files in /sys/devices/platform/msi-laptop:
 *   webcam            Integrated webcam activation
 *   fn_key            Function key location
 *   win_key           Windows key location
 *   battery_mode      Battery health options
 *   cooler_boost      Cooler boost function
 *   shift_mode        CPU & GPU performance modes
 *   fan_mode          FAN performance modes
 *   fw_version        Firmware version
 *   fw_release_date   Firmware release date
 *   cpu/..            CPU related options
 *   gpu/..            GPU related options
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

#include "registers_configuration.h"

#include <acpi/battery.h>
#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static const char *ALLOWED_FW_0[] = {
	"14C1EMS1.101",
	"17F2EMS1.106",
	NULL,
};

static struct msi_ec_conf CONF0 = {
	.allowed_fw = ALLOWED_FW_0,
	.charge_control = {
		.address      = 0xef,
		.offset_start = 0x8a,
		.offset_end   = 0x80,
		.range_min    = 0x8a,
		.range_max    = 0xe4,
	},
	.webcam = {
		.address      = 0x2e,
		.hard_address = 0x2f,
		.bit          = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
	},
	.battery_mode = {
		.address = 0xef,
		.modes   = { 0xbc, 0xd0, 0xe4 },
	},
	.power_status = {
		.address          = 0x30,
		.lid_open_bit     = 1,
		.ac_connected_bit = 0,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address    = 0xf2,
		.off_value  = 0x80,
		.base_value = 0xc0,
		.max_mode   = 4,
	},
	.fan_mode = {
		.address     = 0xf4,
		.mode_values = { 0x0d, 0x4d, 0x8d },
		.max_mode    = 2,
	},
	.fw = {
		.version_address = 0xa0,
		.date_address    = 0xac,
		.time_address    = 0xb4,
	},
	.cpu = {
		.rt_temp_address       = 0x68,
		.rt_fan_speed_address  = 0x71,
		.rt_fan_speed_base_min = 0x19,
		.rt_fan_speed_base_max = 0x37,
		.bs_fan_speed_address  = 0x89,
		.bs_fan_speed_base_min = 0x00,
		.bs_fan_speed_base_max = 0x0f,
	},
	.gpu = {
		.rt_temp_address      = 0x80,
		.rt_fan_speed_address = 0x89,
	},
	.leds = {
		.micmute_led_address = 0x2b,
		.mute_led_address    = 0x2c,
		.bit                 = 2,
	},
	.kbd_bl = {
		.bl_mode_address  = 0x2c, // ?
		.bl_modes         = { 0x00, 0x08 }, // ?
		.max_mode         = 1, // ?
		.bl_state_address = 0xf3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_1[] = {
	"1552EMS1.118",
	NULL,
};

static struct msi_ec_conf CONF1 = {
	.allowed_fw = ALLOWED_FW_1,
	.charge_control = {
		.address      = 0xd7,
		.offset_start = 0x8a,
		.offset_end   = 0x80,
		.range_min    = 0x8a,
		.range_max    = 0xe4,
	},
	.webcam = {
		.address      = 0x2e,
		.hard_address = 0x2f,
		.bit          = 1,
	},
	.fn_win_swap = {
		.address = 0xe8,
		.bit     = 4,
	},
	.battery_mode = {
		.address = 0xd7,
		.modes   = { 0xbc, 0xd0, 0xe4 },
	},
	.power_status = {
		.address          = 0x30,
		.lid_open_bit     = 1,
		.ac_connected_bit = 0,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address    = 0xf2,
		.off_value  = 0x80,
		.base_value = 0xc0,
		.max_mode   = 2,
	},
	.fan_mode = {
		.address     = 0xd4,
		.mode_values = { 0x1d, 0x4d, 0x8d },
		.max_mode    = 2,
	},
	.fw = {
		.version_address = 0xa0,
		.date_address    = 0xac,
		.time_address    = 0xb4,
	},
	.cpu = {
		.rt_temp_address       = 0x68,
		.rt_fan_speed_address  = 0x71,
		.rt_fan_speed_base_min = 0x19,
		.rt_fan_speed_base_max = 0x37,
		.bs_fan_speed_address  = 0x89,
		.bs_fan_speed_base_min = 0x00,
		.bs_fan_speed_base_max = 0x0f,
	},
	.gpu = {
		.rt_temp_address      = 0x80,
		.rt_fan_speed_address = 0x89,
	},
	.leds = {
		.micmute_led_address = 0x2c,
		.mute_led_address    = 0x2d,
		.bit                 = 1,
	},
	.kbd_bl = {
		.bl_mode_address  = 0x2c, // ?
		.bl_modes         = { 0x00, 0x08 }, // ?
		.max_mode         = 1, // ?
		.bl_state_address = 0xd3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static struct msi_ec_conf *CONFIGURATIONS[] = {
	&CONF0,
	&CONF1,
	NULL,
};

static struct msi_ec_conf *conf = &CONF0; // current configuration

#define streq(x, y) (strcmp(x, y) == 0 || strcmp(x, y "\n") == 0)

#define set_bit(v, b)   (v |= (1 << b))
#define unset_bit(v, b) (v &= ~(1 << b))
#define check_bit(v, b) ((bool)((v >> b) & 1))

static int ec_read_seq(u8 addr, u8 *buf, int len)
{
	int result;
	u8 i;
	for (i = 0; i < len; i++) {
		result = ec_read(addr + i, buf + i);
		if (result < 0)
			return result;
	}
	return 0;
}

static int ec_set_bit(u8 addr, u8 bit)
{
	int result;
	u8 stored;

	result = ec_read(addr, &stored);
	if (result < 0)
		return result;

	set_bit(stored, bit);

	return ec_write(addr, stored);
}

static int ec_unset_bit(u8 addr, u8 bit)
{
	int result;
	u8 stored;

	result = ec_read(addr, &stored);
	if (result < 0)
		return result;

	unset_bit(stored, bit);

	return ec_write(addr, stored);
}

static int ec_check_bit(u8 addr, u8 bit, bool *output)
{
	int result;
	u8 stored;

	result = ec_read(addr, &stored);
	if (result < 0)
		return result;

	*output = check_bit(stored, bit);

	return 0;
}

static int ec_get_firmware_version_common_address(u8 buf[MSI_EC_FW_VERSION_LENGTH + 1])
{
	int result;

	memset(buf, 0, MSI_EC_FW_VERSION_LENGTH + 1);
	result = ec_read_seq(MSI_EC_FW_VERSION_ADDRESS, buf,
			     MSI_EC_FW_VERSION_LENGTH);
	if (result < 0)
		return result;

	return MSI_EC_FW_VERSION_LENGTH + 1;
}

// ============================================================ //
// Sysfs power_supply subsystem
// ============================================================ //

static ssize_t charge_control_threshold_show(u8 offset, struct device *device,
					     struct device_attribute *attr,
					     char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(conf->charge_control.address, &rdata);
	if (result < 0)
		return result;

	return sprintf(buf, "%i\n", rdata - offset);
}

static ssize_t
charge_control_start_threshold_show(struct device *device,
				    struct device_attribute *attr, char *buf)
{
	return charge_control_threshold_show(conf->charge_control.offset_start,
					     device, attr, buf);
}

static ssize_t charge_control_end_threshold_show(struct device *device,
						 struct device_attribute *attr,
						 char *buf)
{
	return charge_control_threshold_show(conf->charge_control.offset_end,
					     device, attr, buf);
}

static ssize_t charge_control_threshold_store(u8 offset, struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t count)
{
	u8 wdata;
	int result;

	result = kstrtou8(buf, 10, &wdata);
	if (result < 0)
		return result;

	wdata += offset;
	if (wdata < conf->charge_control.range_min ||
	    wdata > conf->charge_control.range_max)
		return -EINVAL;

	result = ec_write(conf->charge_control.address, wdata);
	if (result < 0)
		return result;

	return count;
}

static ssize_t
charge_control_start_threshold_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	return charge_control_threshold_store(
		conf->charge_control.offset_start, dev, attr, buf, count);
}

static ssize_t charge_control_end_threshold_store(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t count)
{
	return charge_control_threshold_store(conf->charge_control.offset_end,
					      dev, attr, buf, count);
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
// Sysfs platform device attributes (root)
// ============================================================ //

static ssize_t webcam_show(struct device *device, struct device_attribute *attr,
			   char *buf)
{
	int result;
	bool bit_value;

	result = ec_check_bit(conf->webcam.address, conf->webcam.bit, &bit_value);

	if (bit_value) {
		return sprintf(buf, "%s\n", "on");
	} else {
		return sprintf(buf, "%s\n", "off");
	}
}

static ssize_t webcam_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int result = -EINVAL;

	if (streq(buf, "on"))
		result = ec_set_bit(conf->webcam.address, conf->webcam.bit);

	if (streq(buf, "off"))
		result = ec_unset_bit(conf->webcam.address, conf->webcam.bit);

	if (result < 0)
		return result;

	return count;
}

static ssize_t fn_key_show(struct device *device, struct device_attribute *attr,
			   char *buf)
{
	int result;
	bool bit_value;

	result = ec_check_bit(conf->fn_win_swap.address, conf->fn_win_swap.bit, &bit_value);

	if (bit_value) {
		return sprintf(buf, "%s\n", "right");
	} else {
		return sprintf(buf, "%s\n", "left");
	}
}

static ssize_t fn_key_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int result;

	if (streq(buf, "right")) {
		result = ec_set_bit(conf->fn_win_swap.address, conf->fn_win_swap.bit);
	} else if (streq(buf, "left")) {
		result = ec_unset_bit(conf->fn_win_swap.address, conf->fn_win_swap.bit);
	}

	if (result < 0)
		return result;

	return count;
}

static ssize_t win_key_show(struct device *device,
			    struct device_attribute *attr, char *buf)
{
	int result;
	bool bit_value;

	result = ec_check_bit(conf->fn_win_swap.address, conf->fn_win_swap.bit, &bit_value);

	if (bit_value) {
		return sprintf(buf, "%s\n", "left");
	} else {
		return sprintf(buf, "%s\n", "right");
	}
}

static ssize_t win_key_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int result;

	if (streq(buf, "right")) {
		result = ec_unset_bit(conf->fn_win_swap.address, conf->fn_win_swap.bit);
	} else if (streq(buf, "left")) {
		result = ec_set_bit(conf->fn_win_swap.address, conf->fn_win_swap.bit);
	}

	if (result < 0)
		return result;

	return count;
}

static ssize_t battery_mode_show(struct device *device,
				 struct device_attribute *attr, char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(conf->battery_mode.address, &rdata);
	if (result < 0)
		return result;

	if (rdata == conf->battery_mode.modes[2]) {
		return sprintf(buf, "%s\n", "max");
	} else if (rdata == conf->battery_mode.modes[1]) {
		return sprintf(buf, "%s\n", "medium");
	} else if (rdata == conf->battery_mode.modes[0]) {
		return sprintf(buf, "%s\n", "min");
	} else {
		return sprintf(buf, "%s (%i)\n", "unknown", rdata);
	}
}

static ssize_t battery_mode_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int result = -EINVAL;

	if (streq(buf, "max"))
		result = ec_write(conf->battery_mode.address,
				  conf->battery_mode.modes[2]);

	else if (streq(buf, "medium"))
		result = ec_write(conf->battery_mode.address,
				  conf->battery_mode.modes[1]);

	else if (streq(buf, "min"))
		result = ec_write(conf->battery_mode.address,
				  conf->battery_mode.modes[0]);

	if (result < 0)
		return result;

	return count;
}

static ssize_t lid_status_show(struct device *device,
			       struct device_attribute *attr, char *buf)
{
	int result;
	bool bit_value;

	result = ec_check_bit(conf->power_status.address, conf->power_status.lid_open_bit, &bit_value);
	if (result < 0)
		return result;

	return sprintf(buf, "%s\n", bit_value ? "open" : "closed");
}

static ssize_t ac_connected_show(struct device *device,
			         struct device_attribute *attr, char *buf)
{
	int result;
	bool bit_value;

	result = ec_check_bit(conf->power_status.address, conf->power_status.ac_connected_bit, &bit_value);
	if (result < 0)
		return result;

	return sprintf(buf, "%s\n", bit_value ? "1" : "0");
}

static ssize_t cooler_boost_show(struct device *device,
				 struct device_attribute *attr, char *buf)
{
	int result;
	bool bit_value;

	result = ec_check_bit(conf->cooler_boost.address, conf->cooler_boost.bit, &bit_value);

	if (bit_value) {
		return sprintf(buf, "%s\n", "on");
	} else {
		return sprintf(buf, "%s\n", "off");
	}
}

static ssize_t cooler_boost_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int result = -EINVAL;

	if (streq(buf, "on"))
		result = ec_set_bit(conf->cooler_boost.address,
				  conf->cooler_boost.bit);

	else if (streq(buf, "off"))
		result = ec_unset_bit(conf->cooler_boost.address,
				    conf->cooler_boost.bit);

	if (result < 0)
		return result;

	return count;
}

static ssize_t shift_mode_show(struct device *device,
			       struct device_attribute *attr, char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(conf->shift_mode.address, &rdata);
	if (result < 0)
		return result;

	if (rdata == conf->shift_mode.off_value)
		return sprintf(buf, "-1\n");

	unsigned int mode = rdata - conf->shift_mode.base_value;
	if (mode > conf->shift_mode.max_mode)
		return sprintf(buf, "%s (%i)\n", "unknown", rdata);

	return sprintf(buf, "%i\n", mode);
}

static ssize_t shift_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int wdata;
	int result;

	result = kstrtoint(buf, 10, &wdata);
	if (result < 0)
		return result;

	if (wdata == -1) { // off
		result = ec_write(conf->shift_mode.address,
				  conf->shift_mode.off_value);
	} else { // on (or invalid)
		if ((unsigned int)wdata > conf->shift_mode.max_mode)
			return -EINVAL;

		result = ec_write(conf->shift_mode.address,
				  wdata + conf->shift_mode.base_value);
	}

	if (result < 0)
		return result;

	return count;
}

static ssize_t fan_mode_show(struct device *device,
			     struct device_attribute *attr, char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(conf->fan_mode.address, &rdata);
	if (result < 0)
		return result;

	for (int i = 0; i <= conf->fan_mode.max_mode; i++) {
		if (rdata == conf->fan_mode.mode_values[i]) {
			return sprintf(buf, "%i\n", i);
		}
	}

	return sprintf(buf, "%s (%i)\n", "unknown", rdata);
}

static ssize_t fan_mode_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	u8 wdata;
	int result;

	result = kstrtou8(buf, 10, &wdata);
	if (result < 0)
		return result;

	if (wdata > conf->fan_mode.max_mode) {
		result = -EINVAL;
	} else {
		result = ec_write(conf->fan_mode.address,
				  conf->fan_mode.mode_values[wdata]);
	}

	if (result < 0)
		return result;

	return count;
}

static ssize_t fw_version_show(struct device *device,
			       struct device_attribute *attr, char *buf)
{
	u8 rdata[MSI_EC_FW_VERSION_LENGTH + 1];
	int result;

	memset(rdata, 0, MSI_EC_FW_VERSION_LENGTH + 1);
	result = ec_read_seq(conf->fw.version_address, rdata,
			     MSI_EC_FW_VERSION_LENGTH);
	if (result < 0)
		return result;

	return sprintf(buf, "%s\n", rdata);
}

static ssize_t fw_release_date_show(struct device *device,
				    struct device_attribute *attr, char *buf)
{
	u8 rdate[MSI_EC_FW_DATE_LENGTH + 1];
	u8 rtime[MSI_EC_FW_TIME_LENGTH + 1];
	int result;
	int year, month, day, hour, minute, second;

	memset(rdate, 0, MSI_EC_FW_DATE_LENGTH + 1);
	result = ec_read_seq(conf->fw.date_address, rdate,
			     MSI_EC_FW_DATE_LENGTH);
	if (result < 0)
		return result;
	sscanf(rdate, "%02d%02d%04d", &month, &day, &year);

	memset(rtime, 0, MSI_EC_FW_TIME_LENGTH + 1);
	result = ec_read_seq(conf->fw.time_address, rtime,
			     MSI_EC_FW_TIME_LENGTH);
	if (result < 0)
		return result;
	sscanf(rtime, "%02d:%02d:%02d", &hour, &minute, &second);

	return sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d\n", year, month, day,
		       hour, minute, second);
}

static DEVICE_ATTR_RW(webcam);
static DEVICE_ATTR_RW(fn_key);
static DEVICE_ATTR_RW(win_key);
static DEVICE_ATTR_RW(battery_mode);
static DEVICE_ATTR_RO(lid_status);
static DEVICE_ATTR_RO(ac_connected);
static DEVICE_ATTR_RW(cooler_boost);
static DEVICE_ATTR_RW(shift_mode);
static DEVICE_ATTR_RW(fan_mode);
static DEVICE_ATTR_RO(fw_version);
static DEVICE_ATTR_RO(fw_release_date);

static struct attribute *msi_root_attrs[] = {
	&dev_attr_webcam.attr,		&dev_attr_fn_key.attr,
	&dev_attr_win_key.attr,		&dev_attr_battery_mode.attr,
	&dev_attr_lid_status.attr,      &dev_attr_ac_connected.attr,
	&dev_attr_cooler_boost.attr,	&dev_attr_shift_mode.attr,
	&dev_attr_fan_mode.attr,	&dev_attr_fw_version.attr,
	&dev_attr_fw_release_date.attr, NULL,
};

static const struct attribute_group msi_root_group = {
	.attrs = msi_root_attrs,
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

	result = ec_read(conf->cpu.rt_temp_address, &rdata);
	if (result < 0)
		return result;

	return sprintf(buf, "%i\n", rdata);
}

static ssize_t cpu_realtime_fan_speed_show(struct device *device,
					   struct device_attribute *attr,
					   char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(conf->cpu.rt_fan_speed_address, &rdata);
	if (result < 0)
		return result;

	if ((rdata < conf->cpu.rt_fan_speed_base_min ||
	    rdata > conf->cpu.rt_fan_speed_base_max))
		return -EINVAL;

	return sprintf(buf, "%i\n",
		       100 * (rdata - conf->cpu.rt_fan_speed_base_min) /
			       (conf->cpu.rt_fan_speed_base_max -
				conf->cpu.rt_fan_speed_base_min));
}

static ssize_t cpu_basic_fan_speed_show(struct device *device,
					struct device_attribute *attr,
					char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(conf->cpu.bs_fan_speed_address, &rdata);
	if (result < 0)
		return result;

	if (rdata < conf->cpu.bs_fan_speed_base_min ||
	    rdata > conf->cpu.bs_fan_speed_base_max)
		return -EINVAL;

	return sprintf(buf, "%i\n",
		       100 * (rdata - conf->cpu.bs_fan_speed_base_min) /
			       (conf->cpu.bs_fan_speed_base_max -
				conf->cpu.bs_fan_speed_base_min));
}

static ssize_t cpu_basic_fan_speed_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	u8 wdata;
	int result;

	result = kstrtou8(buf, 10, &wdata);
	if (result < 0)
		return result;

	if (wdata > 100)
		return -EINVAL;

	result = ec_write(conf->cpu.bs_fan_speed_address,
			  (wdata * (conf->cpu.bs_fan_speed_base_max -
				    conf->cpu.bs_fan_speed_base_min) +
			   100 * conf->cpu.bs_fan_speed_base_min) /
				  100);
	if (result < 0)
		return result;

	return count;
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

static struct device_attribute dev_attr_cpu_basic_fan_speed = {
	.attr = {
		.name = "basic_fan_speed",
		.mode = 0644,
	},
	.show = cpu_basic_fan_speed_show,
	.store = cpu_basic_fan_speed_store,
};

static struct attribute *msi_cpu_attrs[] = {
	&dev_attr_cpu_realtime_temperature.attr,
	&dev_attr_cpu_realtime_fan_speed.attr,
	&dev_attr_cpu_basic_fan_speed.attr,
	NULL,
};

static const struct attribute_group msi_cpu_group = {
	.name = "cpu",
	.attrs = msi_cpu_attrs,
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

	result = ec_read(conf->gpu.rt_temp_address, &rdata);
	if (result < 0)
		return result;

	return sprintf(buf, "%i\n", rdata);
}

static ssize_t gpu_realtime_fan_speed_show(struct device *device,
					   struct device_attribute *attr,
					   char *buf)
{
	u8 rdata;
	int result;

	result = ec_read(conf->gpu.rt_fan_speed_address, &rdata);
	if (result < 0)
		return result;

	return sprintf(buf, "%i\n", rdata);
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
	&dev_attr_gpu_realtime_fan_speed.attr,
	NULL,
};

static const struct attribute_group msi_gpu_group = {
	.name = "gpu",
	.attrs = msi_gpu_attrs,
};

static const struct attribute_group *msi_platform_groups[] = {
	&msi_root_group,
	&msi_cpu_group,
	&msi_gpu_group,
	NULL,
};

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
	.probe = msi_platform_probe,
	.remove = msi_platform_remove,
};

// ============================================================ //
// Sysfs leds subsystem
// ============================================================ //

static int micmute_led_sysfs_set(struct led_classdev *led_cdev,
				 enum led_brightness brightness)
{
	int result;

	if (brightness) {
		result = ec_set_bit(conf->leds.micmute_led_address, conf->leds.bit);
	} else {
		result = ec_unset_bit(conf->leds.micmute_led_address, conf->leds.bit);
	}

	if (result < 0)
		return result;

	return 0;
}

static int mute_led_sysfs_set(struct led_classdev *led_cdev,
			      enum led_brightness brightness)
{
	int result;

	if (brightness) {
		result = ec_set_bit(conf->leds.mute_led_address, conf->leds.bit);
	} else {
		result = ec_unset_bit(conf->leds.mute_led_address, conf->leds.bit);
	}

	if (result < 0)
		return result;

	return 0;
}

static enum led_brightness kbd_bl_sysfs_get(struct led_classdev *led_cdev)
{
	u8 rdata;
	int result = ec_read(conf->kbd_bl.bl_state_address, &rdata);
	if (result < 0)
		return 0;
	return rdata & MSI_EC_KBD_BL_STATE_MASK;
}

static int kbd_bl_sysfs_set(struct led_classdev *led_cdev,
			    enum led_brightness brightness)
{
	u8 wdata;
	if (brightness < 0 || brightness > 3)
		return -1;
	wdata = conf->kbd_bl.state_base_value | brightness;
	return ec_write(conf->kbd_bl.bl_state_address, wdata);
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
// Module load/unload
// ============================================================ //

static int __init msi_ec_init(void)
{
	int result;

	if (acpi_disabled) {
		pr_err("Unable to init because ACPI needs to be enabled first!\n");
		return -ENODEV;
	}

	// get firmware version
	u8 fw_version[MSI_EC_FW_VERSION_LENGTH + 1];
	result = ec_get_firmware_version_common_address(fw_version);
	if (result < 0) {
		return result;
	}

	// check if this firmware's configuration is implemented
	bool allowed = false;
	for (int i = 0; CONFIGURATIONS[i]; i++) {
		for (int j = 0; CONFIGURATIONS[i]->allowed_fw[j]; j++) {
			if (strcmp(CONFIGURATIONS[i]->allowed_fw[j], fw_version) == 0) {
				allowed = true;
				conf = CONFIGURATIONS[i];
				break;
			}
		}
	}
	if (!allowed) {
		pr_err("Your firmware version is not supported!\n");
		return -EOPNOTSUPP;
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

	led_classdev_register(&msi_platform_device->dev, &micmute_led_cdev);
	led_classdev_register(&msi_platform_device->dev, &mute_led_cdev);
	led_classdev_register(&msi_platform_device->dev, &msiacpi_led_kbdlight);

	pr_info("msi-ec: module_init\n");
	return 0;
}

static void __exit msi_ec_exit(void)
{
	led_classdev_unregister(&mute_led_cdev);
	led_classdev_unregister(&micmute_led_cdev);
	led_classdev_unregister(&msiacpi_led_kbdlight);

	battery_hook_unregister(&battery_hook);

	platform_driver_unregister(&msi_platform_driver);
	platform_device_del(msi_platform_device);

	pr_info("msi-ec: module_exit\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jose Angel Pastrana <japp0005@red.ujaen.es>");
MODULE_AUTHOR("Aakash Singh <mail@singhaakash.dev>");
MODULE_AUTHOR("Nikita Kravets <k.qovekt@gmail.com>");
MODULE_DESCRIPTION("MSI Embedded Controller");
MODULE_VERSION("0.08");

module_init(msi_ec_init);
module_exit(msi_ec_exit);
