#ifndef __MSI_EC_CONSTANTS__
#define __MSI_EC_CONSTANTS__

#include <linux/types.h>

#define MSI_DRIVER_NAME "msi-ec"

// Firmware info addresses are universal
#define MSI_EC_FW_VERSION_ADDRESS 0xa0
#define MSI_EC_FW_DATE_ADDRESS    0xac
#define MSI_EC_FW_TIME_ADDRESS    0xb4
#define MSI_EC_FW_VERSION_LENGTH  12
#define MSI_EC_FW_DATE_LENGTH     8
#define MSI_EC_FW_TIME_LENGTH     8

struct msi_ec_charge_control_conf {
	int address;
	int offset_start;
	int offset_end;
	int range_min;
	int range_max;
};

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

struct msi_ec_cooler_boost_conf {
	int address;
	int bit;
};

struct msi_ec_mode {
	const char *name;
	int value;
};

#define MSI_EC_SHIFT_MODE_NAME_LIMIT 20
struct msi_ec_shift_mode_conf {
	int address;
	struct msi_ec_mode modes[5]; // fixed size for easier hard coding
	int modes_count;
};

struct msi_ec_fan_mode_conf {
	int address;
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

struct msi_ec_led_conf {
	int micmute_led_address;
	int mute_led_address;
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
	const char **allowed_fw;

	struct msi_ec_charge_control_conf charge_control;
	struct msi_ec_webcam_conf         webcam;
	struct msi_ec_fn_win_swap_conf    fn_win_swap;
	struct msi_ec_battery_mode_conf   battery_mode;
	struct msi_ec_power_status_conf   power_status;
	struct msi_ec_cooler_boost_conf   cooler_boost;
	struct msi_ec_shift_mode_conf     shift_mode;
	struct msi_ec_fan_mode_conf       fan_mode;
	struct msi_ec_cpu_conf            cpu;
	struct msi_ec_gpu_conf            gpu;
	struct msi_ec_led_conf            leds;
	struct msi_ec_kbd_bl_conf         kbd_bl;
};

#endif // __MSI_EC_CONSTANTS__
