#ifndef __MSI_EC_CONSTANTS__
#define __MSI_EC_CONSTANTS__

#include <linux/types.h>

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
#define MSI_EC_BATTERY_MODE_MAX_CHARGE 0xe4
#define MSI_EC_BATTERY_MODE_MEDIUM_CHARGE 0xd0
#define MSI_EC_BATTERY_MODE_MIN_CHARGE 0xbc
#define MSI_EC_COOLER_BOOST_ADDRESS 0x98
#define MSI_EC_COOLER_BOOST_ON 0x82
#define MSI_EC_COOLER_BOOST_OFF 0x02
#define MSI_EC_SHIFT_MODE_ADDRESS 0xf2
#define MSI_EC_SHIFT_MODE_TURBO 0xc4
#define MSI_EC_SHIFT_MODE_SPORT 0xc0
#define MSI_EC_SHIFT_MODE_COMFORT 0xc1
#define MSI_EC_SHIFT_MODE_ECO 0xc2
#define MSI_EC_SHIFT_MODE_OFF 0x80
#define MSI_EC_FW_VERSION_ADDRESS 0xa0
#define MSI_EC_FW_VERSION_LENGTH 12
#define MSI_EC_FW_DATE_ADDRESS 0xac
#define MSI_EC_FW_DATE_LENGTH 8
#define MSI_EC_FW_TIME_ADDRESS 0xb4
#define MSI_EC_FW_TIME_LENGTH 8
#define MSI_EC_CHARGE_CONTROL_ADDRESS 0xef
#define MSI_EC_CHARGE_CONTROL_OFFSET_START 0x8a
#define MSI_EC_CHARGE_CONTROL_OFFSET_END 0x80
#define MSI_EC_CHARGE_CONTROL_RANGE_MIN 0x8a
#define MSI_EC_CHARGE_CONTROL_RANGE_MAX 0xe4
#define MSI_EC_CPU_REALTIME_TEMPERATURE_ADDRESS 0x68
#define MSI_EC_CPU_REALTIME_FAN_SPEED_ADDRESS 0x71
#define MSI_EC_CPU_REALTIME_FAN_SPEED_BASE_MIN 0x19
#define MSI_EC_CPU_REALTIME_FAN_SPEED_BASE_MAX 0x37
#define MSI_EC_CPU_BASIC_FAN_SPEED_ADDRESS 0x89
#define MSI_EC_CPU_BASIC_FAN_SPEED_BASE_MIN 0x00
#define MSI_EC_CPU_BASIC_FAN_SPEED_BASE_MAX 0x0f
#define MSI_EC_GPU_REALTIME_TEMPERATURE_ADDRESS 0x80
#define MSI_EC_GPU_REALTIME_FAN_SPEED_ADDRESS 0x89
#define MSI_EC_FAN_MODE_ADDRESS 0xf4
#define MSI_EC_FAN_MODE_AUTO 0x0d
#define MSI_EC_FAN_MODE_BASIC 0x4d
#define MSI_EC_FAN_MODE_ADVANCED 0x8d

#define MSI_EC_LED_MICMUTE_ADDRESS 0x2b
#define MSI_EC_LED_MUTE_ADDRESS 0x2c
#define MSI_EC_LED_STATE_MASK 0x4
#define MSI_EC_LED_STATE_OFF 0x80
#define MSI_EC_LED_STATE_ON 0x84

#define MSI_EC_KBD_BL_ADDRESS 0xf3
#define MSI_EC_KBD_BL_STATE_MASK 0x3
#define MSI_EC_KBD_BL_STATE_OFF 0x80
#define MSI_EC_KBD_BL_STATE_ON 0x81
#define MSI_EC_KBD_BL_STATE_HALF 0x82
#define MSI_EC_KBD_BL_STATE_FULL 0x83
static int MSI_EC_KBD_BL_STATE[4] = {
    MSI_EC_KBD_BL_STATE_OFF,
    MSI_EC_KBD_BL_STATE_ON,
    MSI_EC_KBD_BL_STATE_HALF,
    MSI_EC_KBD_BL_STATE_FULL};

#define MSI_EC_CAMERA_ADDRESS 0x2e
#define MSI_EC_CAMERA_HARD_ADDRESS 0x2f /* hotkey has no effect if this address disables the cam*/
#define MSI_EC_CAMERA_STATE_MASK 0x2
#define MSI_EC_CAMERA_STATE_OFF 0x80
#define MSI_EC_CAMERA_STATE_ON 0x84

struct msi_ec_fn_win_swap_conf {
	int address;
	int fn_left_value;
	int fn_right_value;
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
	u8 on_value;
	u8 off_value;
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
	int mic_mute_led_address;
	int mute_led_address;
	int base_value;
	int bit;
};

struct msi_ec_kbd_bl_conf {
	int bl_mode_address;
	int bl_modes[2];
	int max_mode;

	int bl_state_address;
	int state_base_value;
	int max_state;
};

struct msi_ec_webcam_conf {
	int address;
	int hard_address;
	u8 on_value;
	u8 off_value;
};

struct msi_ec_conf {
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
	struct msi_ec_webcam_conf         webcam;
};

static struct msi_ec_conf CONFIGURATIONS[1] = {
	{
		.fn_win_swap    = { 0xbf, 0x40, 0x50 },
		.battery_mode   = { 0xef, { 0xbc, 0xd0, 0xe4 } },
		.power_status   = { 0x30, 1, 0 },
		.charge         = { 0x42, 0x31 },
		.cooler_boost   = { 0x98, 0x82, 0x02 },
		.shift_mode     = { 0xf2, 0x80, 0xc0, 4 },
		.fw             = { 0xa0, 0xac, 0xb4 },
		.charge_control = { 0xef, 0x8a, 0x80, 0x8a, 0xe4 },
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
			.mic_mute_led_address = 0x2b,
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
		.webcam = {
			.address      = 0x2e,
			.hard_address = 0x2f,
			.on_value     = 0x84,
			.off_value    = 0x80,
		},
	},
};

static struct msi_ec_conf *conf = CONFIGURATIONS + 0; // current configuration

#endif // __MSI_EC_CONSTANTS__
