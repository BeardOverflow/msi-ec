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

#define SM_ECO_NAME		"eco"
#define SM_COMFORT_NAME		"comfort"
#define SM_SPORT_NAME		"sport"
#define SM_TURBO_NAME		"turbo"

#define FM_AUTO_NAME		"auto"
#define FM_SILENT_NAME		"silent"
#define FM_BASIC_NAME		"basic"
#define FM_ADVANCED_NAME	"advanced"

/* **************** Gen 1 - WMI1 **************** */

static const char *ALLOWED_FW_G1_0[] __initconst = {
	"14C1EMS1.012", // Prestige 14 A10SC
	"14C1EMS1.101",
	"14C1EMS1.102",
	"16S3EMS1.103", // Prestige 15 A10SC
	NULL
};

static struct msi_ec_conf CONF_G1_0 __initdata = {
	.allowed_fw = ALLOWED_FW_G1_0, // legacy fw_0
	.charge_control_address = 0xef,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
		.invert  = false,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xf2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_SPORT_NAME,   0xc0 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = MSI_EC_ADDR_UNKNOWN, // 0xd5 needs testing
	},
	.fan_mode = {
		.address = 0xf4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_BASIC_NAME,    0x4d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
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
		.bl_mode_address  = 0x2c,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = 0xf3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G1_1[] __initconst = {
	"16U7EMS1.105", // GP65 / GL65 Leopard 10S
	"16U7EMS1.106",
	"16U7EMS1.504", // GL65 Leopard 9SD
	"17F2EMS1.103", // GF75 Thin 9SC
	"17F2EMS1.104",
	"17F2EMS1.106",
	"17F2EMS1.107",
	"17F3EMS1.103", // GF75 Thin 9SD
	"17F3EMS2.103", // GF75 Thin 10SER
	"17F4EMS2.100", // GF75 Thin 9SCSR
	"17F5EMS1.102", // GF75 Thin 10UEK
	"17F6EMS1.101", // GF75 Thin 10UC / 10UD / 10SC
	"17F6EMS1.103",
	"17E7EMS1.103", // GP75 Leopard 10SEK
	"17E7EMS1.106", // GL75 Leopard 10SFR
	"17E8EMS1.101", // GL75 Leopard 10SCXR
	NULL
};

static struct msi_ec_conf CONF_G1_1 __initdata = {
	.allowed_fw = ALLOWED_FW_G1_1, // legacy fw_1
	.charge_control_address = 0xef,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
		.invert  = false,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xf2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_SPORT_NAME,   0xc0 },
			{ SM_TURBO_NAME,   0xc4 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = MSI_EC_ADDR_UNKNOWN,
	},
	.fan_mode = {
		.address = 0xf4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_BASIC_NAME,    0x4d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
	},
	.gpu = {
		.rt_temp_address      = 0x80,
		.rt_fan_speed_address = 0x89,
	},
	.leds = {
		.micmute_led_address = MSI_EC_ADDR_UNSUPP,
		.mute_led_address    = MSI_EC_ADDR_UNSUPP,
		.bit                 = 2,
	},
	.kbd_bl = {
		.bl_mode_address  = 0x2c,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = 0xf3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G1_2[] __initconst = {
	"158LEMS1.103", // Alpha 15 B5EE / B5EEK
	"158LEMS1.105",
	"158LEMS1.106",
	"17LLEMS1.106", // Alpha 17 B5EEK
	"15CKEMS1.108", // Delta 15 A5EFK
	NULL
};

static struct msi_ec_conf CONF_G1_2 __initdata = {
	.allowed_fw = ALLOWED_FW_G1_2, // legacy fw_5
	.charge_control_address = 0xef,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
		.invert  = true,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xf2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 }, // super_battery = 0xa5
			{ SM_COMFORT_NAME, 0xc1 }, // silent: super_battery = 0xa4 / balanced: super_battery = 0xa1
			{ SM_TURBO_NAME,   0xc4 }, // super_battery = 0xa0
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = MSI_EC_ADDR_UNKNOWN, // known. 0xd5.
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xf4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
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
		.bl_mode_address  = MSI_EC_ADDR_UNSUPP,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = MSI_EC_ADDR_UNSUPP, // RGB
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G1_3[] __initconst = {
	"1541EMS1.113", // GE66 Raider 10SF
	"1542EMS1.101", // GP66 Leopard 10UG / 10UE / 10UH
	"1542EMS1.102",
	"1542EMS1.104",
	"16Q2EMS1.105", // GS65 Stealth Thin 8RE / 8RF
	"16Q2EMS1.106",
	"16Q2EMS1.107",
	"16Q2EMS1.T40",
	"16Q3EMS1.104", // P65 Creator 8RE - single color kb bl, but 00 val
	"16Q4EMS1.108", // GS65 Stealth 8S / 9S(D/F)
	"16Q4EMS1.109",
	"16Q4EMS1.110",
	"16V1EMS1.109", // GS66 Stealth 10SFS
	"16V1EMS1.112",
	"16V1EMS1.116",
	"16V1EMS1.118", // GS66 Stealth 10SE
	"16V3EMS1.106", // GS66 Stealth 10UE
	NULL
};

static struct msi_ec_conf CONF_G1_3 __initdata = {
	.allowed_fw = ALLOWED_FW_G1_3, // legacy fw_6
	.charge_control_address = 0xef,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
		.invert  = true,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xf2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_SPORT_NAME,   0xc0 },
			{ SM_TURBO_NAME,   0xc4 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = MSI_EC_ADDR_UNKNOWN,
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xf4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
	},
	.gpu = {
		.rt_temp_address      = 0x80,
		.rt_fan_speed_address = 0x89,
	},
	.leds = {
		.micmute_led_address = MSI_EC_ADDR_UNSUPP,
		.mute_led_address    = MSI_EC_ADDR_UNSUPP,
		.bit                 = 2,
	},
	.kbd_bl = {
		.bl_mode_address  = MSI_EC_ADDR_UNSUPP,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = MSI_EC_ADDR_UNSUPP, // RGB
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G1_4[] __initconst = {
	"17FKEMS1.108", // Bravo 17 A4DDR / A4DDK
	"17FKEMS1.109",
	"17FKEMS1.10A",
	NULL
};

static struct msi_ec_conf CONF_G1_4 __initdata = {
	.allowed_fw = ALLOWED_FW_G1_4, // legacy fw_7
	.charge_control_address = 0xef,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
		.invert  = false,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xf2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_SPORT_NAME,   0xc0 },
			{ SM_TURBO_NAME,   0xc4 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = MSI_EC_ADDR_UNKNOWN, // 0xd5 but has its own set of modes
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xf4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
	},
	.gpu = {
		.rt_temp_address      = 0x80,
		.rt_fan_speed_address = 0x89,
	},
	.leds = {
		.micmute_led_address = MSI_EC_ADDR_UNSUPP,
		.mute_led_address    = 0x2c,
		.bit                 = 2,
	},
	.kbd_bl = {
		.bl_mode_address  = MSI_EC_ADDR_UNKNOWN,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = 0xf3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G1_5[] __initconst = {
	"14JKEMS1.103", // Modern 14 C5M
	"14JKEMS1.104",
	"14JKEMS1.300", // Modern 14 C7M
	"14JKEMS1.600",
	"14JKEMS1.601",
	"1551EMS1.106", // Modern 15 A10M
	"1551EMS1.107",
	NULL
};

static struct msi_ec_conf CONF_G1_5 __initdata = {
	.allowed_fw = ALLOWED_FW_G1_5, // legacy fw_9
	.charge_control_address = 0xef,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
		.invert  = false,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xf2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_TURBO_NAME,   0xc0 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = MSI_EC_ADDR_UNSUPP,
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xf4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
	},
	.gpu = {
		.rt_temp_address      = MSI_EC_ADDR_UNSUPP,
		.rt_fan_speed_address = MSI_EC_ADDR_UNSUPP,
	},
	.leds = {
		.micmute_led_address = 0x2b,
		.mute_led_address    = 0x2c,
		.bit                 = 2,
	},
	.kbd_bl = {
		.bl_mode_address  = MSI_EC_ADDR_UNSUPP,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = 0xf3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G1_6[] __initconst = {
	"14D1EMS1.102", // Modern 14 B10MW
	"14D1EMS1.103",
	"14DKEMS1.104", // Modern 14 B4MW
	"14DKEMS1.105",
	"14DLEMS1.105", // Modern 14 B5M
	"155LEMS1.103", // Modern 15 A5M
	"155LEMS1.105",
	"155LEMS1.106",
	"15HKEMS1.102", // Modern 15 B7M
	"15HKEMS1.104",
	"15HKEMS1.500",
	NULL
};

static struct msi_ec_conf CONF_G1_6 __initdata = {
	.allowed_fw = ALLOWED_FW_G1_6, // legacy fw_16
	.charge_control_address = 0xef,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
		.invert  = true,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xf2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_TURBO_NAME,   0xc0 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = MSI_EC_ADDR_UNKNOWN, // 0xed
		.mask    = 0x0f, // a5, a4, a2
	},
	.fan_mode = {
		.address = 0xf4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
	},
	.gpu = {
		.rt_temp_address      = MSI_EC_ADDR_UNSUPP,
		.rt_fan_speed_address = MSI_EC_ADDR_UNSUPP,
	},
	.leds = {
		.micmute_led_address = 0x2b,
		.mute_led_address    = 0x2c,
		.bit                 = 2,
	},
	.kbd_bl = {
		.bl_mode_address  = MSI_EC_ADDR_UNKNOWN,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = 0xf3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G1_7[] __initconst = {
	"16R1EMS1.105", // GF63 8RC-249
	"16R3EMS1.100", // GF63 Thin 9SC
	"16R3EMS1.102",
	"16R3EMS1.104",
	"16R4EMS1.101", // GF63 Thin 10SCX(R) / 10SCS(R)
	"16R4EMS1.102",
	"16R4EMS2.101", // GF63 Thin 9SCSR
	"16R4EMS2.102",
	"16R5EMS1.101", // GF63 Thin 10U(C/D) / 10SC
	"16R5EMS1.102",
	"16W1EMS1.102", // GF65 Thin 9SE(X(R)) / 9SD
	"16W1EMS1.103",
	"16W1EMS1.104",
	"16W1EMS2.103", // GF65 Thin 10SCSXR / 10SD(R) / 10SE(R)
	"16W2EMS1.101", // GF65 Thin 10UE
	NULL
};

static struct msi_ec_conf CONF_G1_7 __initdata = {
	.allowed_fw = ALLOWED_FW_G1_7, // legacy fw_21, fw_46 (G1_10)
	.charge_control_address = 0xef,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
		.invert  = true,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xf2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_SPORT_NAME,   0xc0 },
			{ SM_TURBO_NAME,   0xc4 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = MSI_EC_ADDR_UNSUPP,
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xf4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_BASIC_NAME,    0x4d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
	},
	.gpu = {
		.rt_temp_address      = 0x80,
		.rt_fan_speed_address = 0x89,
	},
	.leds = {
		.micmute_led_address = MSI_EC_ADDR_UNSUPP,
		.mute_led_address    = MSI_EC_ADDR_UNSUPP,
		.bit                 = 1,
	},
	.kbd_bl = {
		.bl_mode_address  = MSI_EC_ADDR_UNSUPP,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = 0xf3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G1_8[] __initconst = {
	"16WKEMS1.105", // Bravo 15 A4DDR
	"16S1EMS1.104", // PS63 MODERN 8RD
	NULL
};

static struct msi_ec_conf CONF_G1_8 __initdata = {
	.allowed_fw = ALLOWED_FW_G1_8, // legacy fw_23
	.charge_control_address = 0xef,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
		.invert  = true,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xf2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 }, // Super Battery
			{ SM_COMFORT_NAME, 0xc1 }, // Silent / Balanced / AI
			{ SM_TURBO_NAME,   0xc4 }, // Performance
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = MSI_EC_ADDR_UNSUPP, // enabled by "Super Battery" shift mode
	},
	.fan_mode = {
		.address = 0xf4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
	},
	.gpu = {
		.rt_temp_address      = 0x80,
		.rt_fan_speed_address = 0x89,
	},
	.leds = {
		.micmute_led_address = MSI_EC_ADDR_UNSUPP,
		.mute_led_address    = MSI_EC_ADDR_UNSUPP,
	},
	.kbd_bl = {
		.bl_mode_address  = MSI_EC_ADDR_UNSUPP,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = 0xf3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G1_9[] __initconst = {
	"17G1EMS2.106", // P75  CREATOR 9SG
	"17G1EMS1.100", // GS75 Stealth 8SF
	"17G1EMS1.102", // GS75 Stealth 9SF
	"17G1EMS1.107",
	"17G3EMS1.113", // GS75 Stealth 10SF
	"17G3EMS1.115",
	NULL
};

static struct msi_ec_conf CONF_G1_9 __initdata = {
	.allowed_fw = ALLOWED_FW_G1_9, // legacy fw_31, fw_55 (G1_12)
	.charge_control_address = 0xef,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
		.invert  = false,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xf2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 }, // super battery
			{ SM_COMFORT_NAME, 0xc1 }, // balanced
			{ SM_SPORT_NAME,   0xc0 }, // sport
			{ SM_TURBO_NAME,   0xc4 }, // extreme
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = MSI_EC_ADDR_UNSUPP,
	},
	.fan_mode = {
		.address = 0xf4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_BASIC_NAME,    0x4d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
	},
	.gpu = {
		.rt_temp_address      = 0x80,
		.rt_fan_speed_address = 0x89,
	},
	.leds = {
		.micmute_led_address = MSI_EC_ADDR_UNSUPP,
		.mute_led_address    = MSI_EC_ADDR_UNSUPP,
		.bit                 = 1,
	},
	.kbd_bl = {
		.bl_mode_address  = MSI_EC_ADDR_UNSUPP,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = MSI_EC_ADDR_UNSUPP,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G1_10[] __initconst = {
	"16P5EMS1.103", // GE63 Raider 8RE
	"1782EMS1.109", // GT72 6QE Dominator Pro
	NULL
};

static struct msi_ec_conf CONF_G1_10 __initdata = {
	.allowed_fw = ALLOWED_FW_G1_10, // new
	.charge_control_address = MSI_EC_ADDR_UNSUPP, // unsupported
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
		.invert  = false,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xf2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 }, // green
			{ SM_COMFORT_NAME, 0xc1 }, // comfort
			{ SM_SPORT_NAME,   0xc0 }, // sport
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = MSI_EC_ADDR_UNSUPP,
	},
	.fan_mode = {
		.address = 0xf4,
		.modes = {
			{ FM_AUTO_NAME,     0x0c },
			{ FM_BASIC_NAME,    0x4c },
			{ FM_ADVANCED_NAME, 0x8c },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
	},
	.gpu = {
		.rt_temp_address      = 0x80,
		.rt_fan_speed_address = 0x89,
	},
	.leds = {
		.micmute_led_address = MSI_EC_ADDR_UNSUPP,
		.mute_led_address    = MSI_EC_ADDR_UNSUPP,
		.bit                 = 1,
	},
	.kbd_bl = {
		.bl_mode_address  = MSI_EC_ADDR_UNSUPP,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = MSI_EC_ADDR_UNSUPP, // RGB
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G1_11[] __initconst = {
	"158MEMS1.100", // Bravo 15 B5ED
	"158MEMS1.101",
	"158KEMS1.104", // Bravo 15 B5DD
	"158KEMS1.106",
	"158KEMS1.107",
	"158KEMS1.108",
	"158KEMS1.109",
	"158KEMS1.111",
	NULL
};

static struct msi_ec_conf CONF_G1_11 __initdata = {
	.allowed_fw = ALLOWED_FW_G1_11, // legacy fw_51
	.charge_control_address = 0xef,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
		.invert  = true,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xf2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_TURBO_NAME,   0xc4 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = MSI_EC_ADDR_UNKNOWN, // 0xd5 (automatic switching with shift mode)
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xf4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
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
		.bl_mode_address  = MSI_EC_ADDR_UNKNOWN,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = 0xf3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G1_13[] __initconst = {
	"16V2EMS1.104", // Creator 15 A10SD
	"16V2EMS1.106", // Creator 15 A10SET
	NULL
};

static struct msi_ec_conf CONF_G1_13 __initdata = {
	.allowed_fw = ALLOWED_FW_G1_13, // legacy fw_58
	.charge_control_address = 0xef,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xbf,
		.bit     = 4,
		.invert  = true,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xf2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 }, // Super Battery
			{ SM_COMFORT_NAME, 0xc1 }, // Balanced + Silent
			{ SM_TURBO_NAME,   0xc0 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = 0xd5,
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xf4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
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
		.bl_mode_address  = MSI_EC_ADDR_UNSUPP,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = 0xF3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

/* ^^^^^^^^^^^^^^^^ Gen 1 - WMI1 ^^^^^^^^^^^^^^^^ */

/* **************** Gen 2 - WMI2 **************** */

static const char *ALLOWED_FW_G2_0[] __initconst = {
	"14D2EMS1.116", // Modern 14 B11M
	"14D3EMS1.116", // Modern 14 B11MOU
	"1552EMS1.115", // Modern 15 A11M
	"1552EMS1.118",
	"1552EMS1.119",
	"1552EMS1.120",
	"159KIMS1.107", // Prestige A16 AI+ A3HMG
	"159KIMS1.108", // Summit A16 AI+ A3HMTG
	"159KIMS1.110",
	"15H1IMS1.214", // Modern 15 B13M
	"15H5EMS1.111", // Modern 15 H AI C1MG
	NULL
};

static struct msi_ec_conf CONF_G2_0 __initdata = {
	.allowed_fw = ALLOWED_FW_G2_0, // legacy fw_2, fw_53 (G2_19), 159K - Center S app
	.charge_control_address = 0xd7,
	.webcam = { // 159K, 15H5 have no webcam control
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xe8,
		.bit     = 4,
		.invert  = true,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xd2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_TURBO_NAME,   0xc4 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = 0xeb,
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xd4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
	},
	.gpu = {
		.rt_temp_address      = MSI_EC_ADDR_UNSUPP,
		.rt_fan_speed_address = MSI_EC_ADDR_UNSUPP,
	},
	.leds = {
		.micmute_led_address = 0x2c,
		.mute_led_address    = 0x2d,
		.bit                 = 1,
	},
	.kbd_bl = {
		.bl_mode_address  = 0x2c,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = 0xd3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G2_1[] __initconst = {
	"14C4EMS1.120", // Prestige 14 A11SCX
	"14C6EMS1.109", // Prestige 14 Evo A12M
	"1581EMS1.107", // Katana GF66 11UE / 11UG
	"1582EMS1.105", // Pulse GL66 11UDK
	"1582EMS1.107", // Katana GF66 11UC / 11UD
	"1583EMS1.105", // Crosshair 15 B12UEZ / B12UGSZ
	"1583EMS1.110", // Pulse  GL66 12UGK / Katana GF66 12UG
	"1583EMS1.111",
	"1584EMS1.104", // Katana GF66 12U(C/D) (ENE)
	"1584EMS1.112",
	"1584IMS1.106", // Katana GF66 12UDO (ITE) (#467)
	"1585EMS1.111", // Creator M16 B13VF
	"1585EMS1.112", // Katana 15 B13VGK
	"1585EMS1.113",
	"1585EMS1.115", // Pulse 15 B13VGK
	"1585EMS2.109", // Katana 15 B12VFK / B12VGK
	"1585EMS2.110",
	"1585EMS2.115",
	"158NIMS1.109", // Bravo 15 C7V
	"158NIMS1.10D", // Bravo 15 C7UCX
	"158NIMS1.10E",
	"158NIMS1.30C", // Bravo 15 C7VFKP
	"158NIMS1.502", // Katana A15 AI B8V(F)
	"158NIMS1.505",
	"158PIMS1.106", // Bravo 15 B7ED
	"158PIMS1.111",
	"158PIMS1.112",
	"158PIMS1.114",
	"158PIMS1.207", // Bravo 15 B7E
	"1591EMS1.108", // Summit E16 Flip A11UCT
	"1592EMS1.111", // Summit E16 Flip A12UCT / A12MT
	"1594EMS1.109", // Prestige 16 Studio A13VE
	"1596EMS1.105", // Summit E16 AI Studio A1VETG
	"15H2IMS1.105", // Modern 15 B12HW
	"15K1IMS1.110", // Cyborg 15 A12VF
	"15K1IMS1.111", // Cyborg 15 A13VF
	"15K1IMS1.112", // Cyborg 15 A13VFK
	"15K1IMS1.113", // Cyborg 15 A13VF
	"16S6EMS1.111", // Prestige 15 A11SCX
	"16S6EMS1.114",
	"16S8EMS1.107", // Prestige 15 A12SC / A12UC
	"16V6EMS1.103", // Stealth 15 A13V
	"17L1EMS1.103", // Katana GF76 11UE
	"17L1EMS1.105", // Crosshair 17 A11UEK
	"17L1EMS1.106", // Katana GF76 11UG
	"17L1EMS1.107",
	"17L2EMS1.103", // Katana GF76 11UC / 11UD
	"17L2EMS1.106",
	"17L2EMS1.108", // Katana 17 B11UCX
	"17L3EMS1.106", // Crosshair 17 B12UGZ
	"17L3EMS1.109", // Katana GF76 12UG
	"17L4EMS1.112", // Katana GF76 12UC
	"17LNIMS1.10E", // Bravo 17 C7VE
	"17LNIMS1.505", // Katana A17 AI B8VF
	"17M1EMS2.113", // Creator 17 B11UE
	NULL
};

static struct msi_ec_conf CONF_G2_1 __initdata = {
	.allowed_fw = ALLOWED_FW_G2_1, // legacy fw_3, fw_10 (G2_4), fw_11 (G2_5), fw_14 (G2_7), fw_17 (G2_8), fw_32 (G2_12), fw_34 (G2_14)
	.charge_control_address = 0xd7,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xe8,
		.bit     = 4,
		.invert  = true,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xd2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_TURBO_NAME,   0xc4 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = 0xeb,
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xd4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
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
		.bl_mode_address  = 0x2c,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = 0xd3, // mix of single and RGB
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G2_2[] __initconst = {
	"1543EMS1.108", // GP66 Leopard 11UG
	"1543EMS1.113", // GE66 Raider 11UE
	"1543EMS1.115",
	"1544EMS1.107", // Vector GP66 12UGS
	"1544EMS1.112",
	"1545IMS1.109", // Raider GE67 HX 12U
	"16V4EMS1.114", // GS66 Stealth 11UE / 11UG
	"16V4EMS1.115",
	"16V4EMS1.116",
	"16V5EMS1.107", // Stealth GS66 12UE / 12UGS
	"16V5EMS1.108",
	"17K3EMS1.112", // GE76 Raider 11U / 11UH
	"17K3EMS1.113", // GE76 Raider 11UE
	"17K3EMS1.114",
	"17K3EMS1.115", // GP76 Leopard 11UG
	"17K4EMS1.108", // Raider GE76 12UE
	"17K4EMS1.112", // Raider GE76 12UGS / Vector GP76 12UH
	"17K5IMS1.107", // Raider GE77 HX 12UGS
	"17KKIMS1.108", // Alpha 17 C7VF / C7VG
	"17KKIMS1.109",
	"17KKIMS1.114",
	"17KKIMS1.115",
	"17M1EMS1.113", // Stealth GS76 11UG
	NULL
};

static struct msi_ec_conf CONF_G2_2 __initdata = {
	.allowed_fw = ALLOWED_FW_G2_2, // legacy fw_4, fw_47 (G2_18)
	.charge_control_address = 0xd7,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xe8,
		.bit     = 4,
		.invert  = true,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xd2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_TURBO_NAME,   0xc4 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = 0xeb,
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xd4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
	},
	.gpu = {
		.rt_temp_address      = 0x80,
		.rt_fan_speed_address = 0x89,
	},
	.leds = {
		.micmute_led_address = MSI_EC_ADDR_UNSUPP,
		.mute_led_address    = MSI_EC_ADDR_UNSUPP,
		.bit                 = 1,
	},
	.kbd_bl = {
		.bl_mode_address  = MSI_EC_ADDR_UNSUPP,
		.bl_modes         = { },
		.max_mode         = 1,
		.bl_state_address = MSI_EC_ADDR_UNSUPP,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G2_3[] __initconst = {
	"14F1EMS1.112", // Summit E14 Flip Evo A12MT
	"14F1EMS1.114", // Summit E14 Evo A12M
	"14F1EMS1.115",
	"14F1EMS1.116",
	"14F1EMS1.117",
	"14F1EMS1.118",
	"14F1EMS1.119",
	"14F1EMS1.120",
	"14F1EMS1.207", // Prestige 14 Evo B13M
	"14F1EMS1.209", // Summit E14 Flip Evo A13MT
	"14F1EMS1.211",
	"14L1EMS1.307", // Modern 14 H D13M
	"14L1EMS1.308",
	"14L1EMS1.311",
	"14J1IMS1.109", // Modern 14 C12M
	"14J1IMS1.205",
	"14J1IMS1.209",
	"14J1IMS1.215",
	"14N1EMS1.104", // Prestige 14 AI Evo C1MG
	"14N1EMS1.307", // Prestige 14 AI Evo C2HMG
	"13P5EMS1.106", // Summit 13 AI+ Evo A2VM
	"13Q2EMS1.110", // Prestige 13 AI Evo A1MG
	"13Q3EMS1.111", // Prestige 13 AI+ Evo A2VMG
	"14QKIMS1.108", // Venture A14 AI+ A3HMG
	NULL
};

static struct msi_ec_conf CONF_G2_3 __initdata = {
	.allowed_fw = ALLOWED_FW_G2_3, // legacy fw_8, fw_25, fw_42 (G2_17)
	.charge_control_address = 0xd7,
	.webcam = {          // Has no hardware webcam control: 13P5
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xe8,
		.bit     = 4,
		.invert  = false,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xd2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_TURBO_NAME,   0xc4 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = 0xeb,
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xd4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
	},
	.gpu = {
		.rt_temp_address      = MSI_EC_ADDR_UNSUPP,
		.rt_fan_speed_address = MSI_EC_ADDR_UNSUPP,
	},
	.leds = {
		.micmute_led_address = 0x2c, // not present on `14F1`
		.mute_led_address    = 0x2d, // not present on `14L1`, `14N1`, `14QK`. May require udev rule to have ALSA drive LED state on 13P5.
		.bit                 = 1,
	},
	.kbd_bl = {
		.bl_mode_address  = 0x2c,
		.bl_modes         = { 0x00, 0x08 }, // 00 - on, 08 - 10 sec auto off
		.max_mode         = 1,
		.bl_state_address = 0xd3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G2_4[] __initconst = {
	"14N2EMS1.102", // Prestige 14 AI Studio C1UDXG
	"14N2EMS1.103",
	"14P1IMS1.106", // Cyborg 14 A13VF
	NULL
};

static struct msi_ec_conf CONF_G2_4 __initdata = {
	.allowed_fw = ALLOWED_FW_G2_4, // new
	.charge_control_address = 0xd7,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xe8,
		.bit     = 4,
		.invert  = false,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xd2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_TURBO_NAME,   0xc4 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = 0xeb,
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xd4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
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
		.bl_mode_address  = 0x2c,
		.bl_modes         = { 0x00, 0x08 }, // 00 - on, 08 - 10 sec auto off
		.max_mode         = 1,
		.bl_state_address = 0xd3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G2_5[] __initconst = {
	"14K1EMS1.103", // Stealth 14 Studio A13VF
	"14K1EMS1.108",
	"14K2EMS1.104", // Stealth 14 AI Studio A1VGG / A1VFG
	"14K2EMS1.107",
	NULL
};

static struct msi_ec_conf CONF_G2_5 __initdata = {
	.allowed_fw = ALLOWED_FW_G2_5, // new
	.charge_control_address = 0xd7,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xe8,
		.bit     = 4,
		.invert  = false,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xd2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_TURBO_NAME,   0xc4 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = 0xeb,
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xd4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
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
		.bl_mode_address  = MSI_EC_ADDR_UNSUPP,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = MSI_EC_ADDR_UNSUPP,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G2_6[] __initconst = {
	"16R6EMS1.103", // GF63 Thin 11UC / 11SC
	"16R6EMS1.104",
	"16R6EMS1.106",
	"16R6EMS1.107",
	"16R7IMS1.104", // Thin GF63 12HW
	"16R8IMS1.101", // Thin GF63 12VE
	"16R8IMS1.107",
	"16R8IMS1.108", // Thin GF63 12UCX
	"16R8IMS1.111", // Thin GF63 12V(E/F)
	"16R8IMS1.117", // Thin GF63 12UC
	"16R8IMS2.111", // Thin 15 B12UCX / B12VE
	"16R8IMS2.112",
	"16R8IMS2.117",
	"16RKIMS1.110", // Thin A15 B7VF
	"16RKIMS1.111",
	"16RKIMS2.108",
	NULL
};

static struct msi_ec_conf CONF_G2_6 __initdata = {
	.allowed_fw = ALLOWED_FW_G2_6, // legacy fw_12
	.charge_control_address = 0xd7,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xe8,
		.bit     = 4,
		.invert  = true,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xd2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_TURBO_NAME,   0xc4 },
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = 0xeb,
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xd4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
	},
	.gpu = {
		.rt_temp_address      = 0x80,
		.rt_fan_speed_address = 0x89,
	},
	.leds = {
		.micmute_led_address = MSI_EC_ADDR_UNSUPP,
		.mute_led_address    = MSI_EC_ADDR_UNSUPP,
		.bit                 = 1,
	},
	.kbd_bl = {
		.bl_mode_address  = MSI_EC_ADDR_UNSUPP,
		.bl_modes         = { },
		.max_mode         = 1,
		.bl_state_address = 0xd3,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

static const char *ALLOWED_FW_G2_10[] __initconst = {
	"1562EMS1.117", // Stealth 15M A11SEK
	"1563EMS1.106", // Stealth 15M A11UEK
	"1563EMS1.111",
	"1563EMS1.115",
	"1571EMS1.106", // Creator Z16 A11UE
	"1572EMS1.106", // Creator Z16 A12U
	"1572EMS1.107",
	"1587EMS1.102", // Katana 15 HX B14WEK
	"15F2EMS1.109", // Stealth 16 Studio A13VG
	"15F4EMS1.105", // Stealth 16 AI Studio A1VFG
	"15F4EMS1.106",
	"15FKIMS1.106", // Stealth A16 AI+ A3XVFG / A3XVGG
	"15FKIMS1.109",
	"15FKIMS1.110", // Stealth A16 AI+ A3XVGG
	"15FLIMS1.107", // Stealth A16 AI+ A3XWHG
	"15K2EMS1.106", // Cyborg 15 AI A1VFK
	"15M1IMS1.109", // Vector GP68 HX 13V
	"15M1IMS1.110",
	"15M1IMS1.113", // Vector GP68 HX 12V
	"15M1IMS2.104", // Raider GE68 HX 14VIG
	"15M1IMS2.105", // Vector 16 HX A13V* / A14V*
	"15M1IMS2.111",
	"15M1IMS2.112",
	"15M2IMS2.112", // Raider GE68 HX 14VGG
	"15M2IMS1.110", // Raider GE68HX 13V(F/G)
	"15M2IMS1.112", // Vector GP68HX 13VF
	"15M2IMS1.113",
	"15M2IMS1.114",
	"15M3EMS1.105", // Vector 16 HX AI A2XWHG / A2XWIG
	"15M3EMS1.106",
	"15M3EMS1.107",
	"15M3EMS1.109",
	"15M3EMS1.110",
	"15M3EMS1.112",
	"15M3EMS1.113",
	"15P2EMS1.108", // Sword 16 HX B13V / B14V
	"15P2EMS1.110",
	"15P3EMS1.103", // Pulse 16 AI C1VGKG/C1VFKG
	"15P3EMS1.106",
	"15P3EMS1.107",
	"15P4EMS1.105", // Crosshair 16 HX AI D2XW(GKG)
	"15P4EMS1.107",
	"17L5EMS1.111", // Pulse/Katana 17 B13V/GK
	"17L5EMS1.115",
	"17L5EMS2.115", // Katana 17 B12VEK
	"17L7EMS1.102", // Katana 17 HX B14WGK
	"17N1EMS1.109", // Creator Z17 A12UGST
	"17P1EMS1.104", // Stealth GS77 12U(E/GS)
	"17P1EMS1.106",
	"17P2EMS1.111", // Stealth 17 Studio A13VI
	"17Q2IMS1.107", // Titan GT77HX 13VH
	"17Q2IMS1.10D",
	"17S1IMS1.105", // Raider GE78HX 13VI
	"17S1IMS1.113",
	"17S1IMS1.114",
	"17S1IMS2.104", // Raider GE78 HX 14VHG
	"17S1IMS2.107", // Vector 17 HX A14V
	"17S1IMS2.111", // Vector 17 HX A13VHG
	"17S1IMS2.112",
	"17S2IMS1.113", // Raider GE78 HX Smart Touchpad 13V
	"17S3EMS1.104", // Vector 17 HX AI A2XWHG
	"17T2EMS1.110", // Sword 17 HX B14VGKG
	"1822EMS1.105", // Titan 18 HX A14V
	"1822EMS1.109", // WMI 2.8
	"1822EMS1.111",
	"1822EMS1.112",
	"1822EMS1.114",
	"1822EMS1.115",
	"1824EMS1.107", // Titan 18 HX Dragon Edition
	"182LIMS1.108", // Vector A18 HX A9WHG
	"182LIMS1.111", // New ec version for Vector A18 HX A9WHG
	"182KIMS1.113", // Raider A18 HX A7VIG
	NULL
};

static struct msi_ec_conf CONF_G2_10 __initdata = {
	.allowed_fw = ALLOWED_FW_G2_10, // legacy fw_27, fw_28 (G2_11), fw_33 (G2_13) fw_35 (G2_15), fw_37 (G2_16), fw_56 (G2_20), fw_59 (G2_21)
	.charge_control_address = 0xd7,
	.webcam = {
		.address       = 0x2e,
		.block_address = 0x2f,
		.bit           = 1,
	},
	.fn_win_swap = {
		.address = 0xe8,
		.bit     = 4,
		.invert  = true,
	},
	.cooler_boost = {
		.address = 0x98,
		.bit     = 7,
	},
	.shift_mode = {
		.address = 0xd2,
		.modes = {
			{ SM_ECO_NAME,     0xc2 },
			{ SM_COMFORT_NAME, 0xc1 },
			{ SM_TURBO_NAME,   0xc4 }, // sometimes 0xc0
			MSI_EC_MODE_NULL
		},
	},
	.super_battery = {
		.address = 0xeb,
		.mask    = 0x0f,
	},
	.fan_mode = {
		.address = 0xd4,
		.modes = {
			{ FM_AUTO_NAME,     0x0d },
			{ FM_SILENT_NAME,   0x1d },
			{ FM_ADVANCED_NAME, 0x8d },
			MSI_EC_MODE_NULL
		},
	},
	.cpu = {
		.rt_temp_address      = 0x68,
		.rt_fan_speed_address = 0x71,
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
		.bl_mode_address  = MSI_EC_ADDR_UNSUPP,
		.bl_modes         = { 0x00, 0x08 },
		.max_mode         = 1,
		.bl_state_address = MSI_EC_ADDR_UNSUPP,
		.state_base_value = 0x80,
		.max_state        = 3,
	},
};

/* ^^^^^^^^^^^^^^^^ Gen 2 - WMI2 ^^^^^^^^^^^^^^^^ */

static struct msi_ec_conf *CONFIGURATIONS[] __initdata = {
	/* **** Gen 1 - WMI1 **** */
	&CONF_G1_0,
	&CONF_G1_1,
	&CONF_G1_2,
	&CONF_G1_3,
	&CONF_G1_4,
	&CONF_G1_5,
	&CONF_G1_6,
	&CONF_G1_7,
	&CONF_G1_8,
	&CONF_G1_9,
	&CONF_G1_10,
	&CONF_G1_11,
	&CONF_G1_13,

	/* **** Gen 2 - WMI2 **** */
	&CONF_G2_0,
	&CONF_G2_1,
	&CONF_G2_2,
	&CONF_G2_3,
	&CONF_G2_4,
	&CONF_G2_5,
	&CONF_G2_6,
	&CONF_G2_10,
	NULL
};

static bool conf_loaded = false;
static struct msi_ec_conf conf; // current configuration

static bool charge_control_supported = false;

static char *firmware = NULL;
module_param(firmware, charp, 0);
MODULE_PARM_DESC(firmware, "Load a configuration for a specified firmware version");

static bool debug = false;
module_param(debug, bool, 0);
MODULE_PARM_DESC(debug, "Load the driver in the debug mode, exporting the debug attributes");

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
	&dev_attr_charge_control_end_threshold.attr,
	NULL
};

ATTRIBUTE_GROUPS(msi_battery);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,2,0))
static int msi_battery_add(struct power_supply *battery,
			   struct acpi_battery_hook *hook)
#else
static int msi_battery_add(struct power_supply *battery)
#endif
{
	return device_add_groups(&battery->dev, msi_battery_groups);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,2,0))
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

static ssize_t webcam_common_store(u8 address,
				   const char *buf,
				   size_t count,
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

static ssize_t webcam_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	return webcam_common_show(conf.webcam.address, buf, false);
}

static ssize_t webcam_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	return webcam_common_store(conf.webcam.address, buf, count, false);
}

static ssize_t webcam_block_show(struct device *device,
				 struct device_attribute *attr,
				 char *buf)
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

	result = ec_check_bit(conf.fn_win_swap.address, conf.fn_win_swap.bit, &value);
	if (result < 0)
		return result;

	value ^= conf.fn_win_swap.invert; // invert the direction for some laptops
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

	value ^= conf.fn_win_swap.invert; // invert the direction for some laptops
	value = !value; // fn key position is the opposite of win key

	result = ec_set_bit(conf.fn_win_swap.address, conf.fn_win_swap.bit, value);

	if (result < 0)
		return result;

	return count;
}

static ssize_t win_key_show(struct device *device,
			    struct device_attribute *attr, char *buf)
{
	int result;
	bool value;

	result = ec_check_bit(conf.fn_win_swap.address, conf.fn_win_swap.bit, &value);
	if (result < 0)
		return result;

	value ^= conf.fn_win_swap.invert; // invert the direction for some laptops

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

	value ^= conf.fn_win_swap.invert; // invert the direction for some laptops

	result = ec_set_bit(conf.fn_win_swap.address, conf.fn_win_swap.bit, value);

	if (result < 0)
		return result;

	return count;
}

static ssize_t cooler_boost_show(struct device *device,
				 struct device_attribute *attr, char *buf)
{
	int result;
	bool value;

	result = ec_check_bit(conf.cooler_boost.address, conf.cooler_boost.bit, &value);
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

	result = ec_set_bit(conf.cooler_boost.address, conf.cooler_boost.bit, value);
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

		result = sysfs_emit_at(buf, count, "%s\n", conf.shift_mode.modes[i].name);
		if (result < 0)
			return result;
		count += result;
	}

	return count;
}

static ssize_t shift_mode_show(struct device *device,
			       struct device_attribute *attr,
			       char *buf)
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
			return sysfs_emit(buf, "%s\n", conf.shift_mode.modes[i].name);
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
				  conf.super_battery.mask,
				  &enabled);
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

		result = sysfs_emit_at(buf, count, "%s\n", conf.fan_mode.modes[i].name);
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
			return sysfs_emit(buf, "%s\n", conf.fan_mode.modes[i].name);
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

	result = sscanf(rdate, "%02d%02d%04d", &time.tm_mon, &time.tm_mday, &time.tm_year);
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

	result = sscanf(rtime, "%02d:%02d:%02d", &time.tm_hour, &time.tm_min, &time.tm_sec);
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
	&dev_attr_cpu_realtime_fan_speed.attr,
	NULL
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
	&dev_attr_gpu_realtime_fan_speed.attr,
	NULL
};

// ============================================================ //
// Sysfs platform device attributes (debug)
// ============================================================ //

// Prints an EC memory dump in form of a table
static ssize_t ec_dump_show(struct device *device,
			    struct device_attribute *attr,
			    char *buf)
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
			ascii_row[j] = isascii(rdata) && isgraph(rdata) ? rdata : '.';
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
static ssize_t ec_get_show(struct device *device,
			   struct device_attribute *attr,
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

static struct attribute *msi_debug_attrs[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_ec_dump.attr,
	&dev_attr_ec_set.attr,
	&dev_attr_ec_get.attr,
	NULL
};

// ============================================================ //
// Sysfs leds subsystem
// ============================================================ //

static int micmute_led_sysfs_set(struct led_classdev *led_cdev,
				 enum led_brightness brightness)
{
	int result;

	result = ec_set_bit(conf.leds.micmute_led_address, conf.leds.bit, brightness);

	if (result < 0)
		return result;

	return 0;
}

static int mute_led_sysfs_set(struct led_classdev *led_cdev,
			      enum led_brightness brightness)
{
	int result;

	result = ec_set_bit(conf.leds.mute_led_address, conf.leds.bit, brightness);

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

static umode_t msi_ec_is_visible(struct kobject *kobj,
				 struct attribute *attr,
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
	&msi_root_group,
	&msi_cpu_group,
	&msi_gpu_group,
	NULL
};

static int __init msi_platform_probe(struct platform_device *pdev)
{
	if (debug) {
		int result = sysfs_create_group(&pdev->dev.kobj,
						&msi_debug_group);
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
	char ver_by_ec[MSI_EC_FW_VERSION_LENGTH + 1]; // to store version read from EC

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
		if (match_string(CONFIGURATIONS[i]->allowed_fw, -1, ver) != -EINVAL) {
			memcpy(&conf,
			       CONFIGURATIONS[i],
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

	msi_platform_device = platform_create_bundle(&msi_platform_driver,
						     msi_platform_probe,
						     NULL, 0, NULL, 0);
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
MODULE_VERSION("0.13");

module_init(msi_ec_init);
module_exit(msi_ec_exit);
