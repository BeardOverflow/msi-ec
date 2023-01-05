#ifndef __MSI_EC_CONSTANTS__
#define __MSI_EC_CONSTANTS__

#include <linux/types.h>

#define MSI_DRIVER_NAME "msi-ec"

struct msi_ec_webcam_conf {
	int address;
	int hard_address;
	int bit;
};

struct msi_ec_fn_win_swap_conf {
	int address;
	int bit;
};

struct msi_ec_battery_mode_conf {
	int address;
	int modes[3]; // min, mid, max
};

struct msi_ec_power_status_conf {
	int address;
	int lid_open_bit;
	int ac_connected_bit;
};

struct msi_ec_charge_conf {
	int battery_charge_address;
	int charging_status_address;
};

struct msi_ec_cooler_boost_conf {
	int address;
	int bit;
};

struct msi_ec_shift_mode_conf {
	int address;
	int off_value;
	int base_value;
	int max_mode;
};

#define MSI_EC_FW_VERSION_LENGTH 12
#define MSI_EC_FW_DATE_LENGTH 8
#define MSI_EC_FW_TIME_LENGTH 8

struct msi_ec_fw_conf {
	int version_address;
	int date_address;
	int time_address;
};

struct msi_ec_charge_control_conf {
	int address;
	int offset_start;
	int offset_end;
	int range_min;
	int range_max;
};

struct msi_ec_cpu_conf {
	int rt_temp_address;
	int rt_fan_speed_address; // realtime
	int rt_fan_speed_base_min;
	int rt_fan_speed_base_max;
	int bs_fan_speed_address; // basic
	int bs_fan_speed_base_min;
	int bs_fan_speed_base_max;
};

struct msi_ec_gpu_conf {
	int rt_temp_address;
	int rt_fan_speed_address; // realtime
};

struct msi_ec_fan_mode_conf {
	int address;
	int mode_values[4];
	int max_mode;
};

struct msi_ec_led_conf {
	int micmute_led_address;
	int mute_led_address;
	int base_value;
	int bit;
};

#define MSI_EC_KBD_BL_STATE_MASK 0x3
struct msi_ec_kbd_bl_conf {
	int bl_mode_address;
	int bl_modes[2];
	int max_mode;

	int bl_state_address;
	int state_base_value;
	int max_state;
};

struct msi_ec_conf {
	struct msi_ec_webcam_conf         webcam;
	struct msi_ec_fn_win_swap_conf    fn_win_swap;
	struct msi_ec_battery_mode_conf   battery_mode;
	struct msi_ec_power_status_conf   power_status;
	struct msi_ec_charge_conf         charge;
	struct msi_ec_cooler_boost_conf   cooler_boost;
	struct msi_ec_shift_mode_conf     shift_mode;
	struct msi_ec_fw_conf             fw;
	struct msi_ec_charge_control_conf charge_control;
	struct msi_ec_cpu_conf            cpu;
	struct msi_ec_gpu_conf            gpu;
	struct msi_ec_fan_mode_conf       fan_mode;
	struct msi_ec_led_conf            leds;
	struct msi_ec_kbd_bl_conf         kbd_bl;
};

static struct msi_ec_conf CONFIGURATIONS[1] = {
	{
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
		.charge = {
			.battery_charge_address  = 0x42,
			.charging_status_address = 0x31,
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
		.fw = {
			.version_address = 0xa0,
			.date_address    = 0xac,
			.time_address    = 0xb4,
		},
		.charge_control = {
			.address      = 0xef,
			.offset_start = 0x8a,
			.offset_end   = 0x80,
			.range_min    = 0x8a,
			.range_max    = 0xe4,
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
		.fan_mode = {
			.address     = 0xf4,
			.mode_values = { 0x0d, 0x1d, 0x4d, 0x8d },
			.max_mode    = 3,
		},
		.leds = {
			.micmute_led_address = 0x2b,
			.mute_led_address     = 0x2c,
			.base_value           = 0x80,
			.bit                  = 2,
		},
		.kbd_bl = {
			.bl_mode_address  = 0x2c, // ?
			.bl_modes         = { 0x00, 0x08 }, // ?
			.max_mode         = 1, // ?
			.bl_state_address = 0xf3,
			.state_base_value = 0x80,
			.max_state        = 3,
		},
	},
};

static struct msi_ec_conf *conf = CONFIGURATIONS + 0; // current configuration

#endif // __MSI_EC_CONSTANTS__
