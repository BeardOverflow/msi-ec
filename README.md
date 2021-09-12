# Embedded Controller for MSI laptops

## Disclaimer

This driver might not work on other laptops produced by MSI. Use it at your own risk, I am not responsible for any damage suffered.

Also, and until future enhancements, no DMI data is used to identify your laptop model. Check the constants.h file before using.

## Installation

1. Install the following packages:
- For Debian: `build-essential linux-headers-amd64`
- For Ubuntu: `build-essential linux-headers-generic`
2. Clone this repository and cd'ed
3. Run `make`
4. Run `make install`
5. (Optional) To uninstall, run `make uninstall`

## Usage

This driver exports a few files in its own platform device, msi-ec, and is available to userspace under:

- `/sys/devices/platform/msi-ec/webcam`
  - Description: This entry allows enabling the integrated webcam.
  - Access: Read, Write
  - Valid values:
    - on: integrated webcam is enabled
    - off: integrated webcam is disabled

- `/sys/devices/platform/msi-ec/fn_key`
  - Description: This entry allows switching the position between the function key and the windows key.
  - Access: Read, Write
  - Valid values:
    - left: function key goes to the left, windows key goes to the right
    - right: function key goes to the right, windows key goes to the left

- `/sys/devices/platform/msi-ec/win_key`
  - Description: This entry allows changing the position for the function key.
  - Access: Read, Write
  - Valid values:
    - left: windows key goes to the left, function key goes to the right
    - right: windows key goes to the right, function key goes to the left

- `/sys/devices/platform/msi-ec/battery_mode`
  - Description: This entry allows changing the battery mode for health purposes.
  - Access: Read, Write
  - Valid values:
    - max: best for mobility. Charge the battery to 100% all the time
    - medium: balanced. Charge the battery when under 70%, stop at 80%
    - min: best for battery. Charge the battery when under 50%, stop at 60%

- `/sys/devices/platform/msi-ec/cooler_boost`
  - Description: This entry allows enabling the cooler boost function. It provides powerful cooling capability by boosting the airflow.
  - Access: Read, Write
  - Valid values:
    - on: cooler boost function is enabled
    - off: cooler boost function is disabled

- `/sys/devices/platform/msi-ec/shift_mode`
  - Description: This entry allows switching the shift mode. It provides a set of profiles for gaining CPU & GPU overclock/underclock.
  - Access: Read, Write
  - Valid values:
    - turbo: over-voltage and over-clock for the CPU & GPU, aka overcloking mode
    - sport: full clock frequency for the CPU & GPU, aka default desktop mode
    - comfort: dynamic clock frequency for the CPU & GPU, aka power balanced mode
    - eco: low clock frequency for the CPU & GPU, aka power saving mode
    - off: operating system decides

- `/sys/devices/platform/msi-ec/fan_mode`
  - Description: This entry allows switching the fan mode. It provides a set of profiles for adjusting the fan speed under specific criteria.
  - Access: Read, Write
  - Valid values:
    - auto: fan speed adjusts automatically
    - basic: fixed 1-level fan speed for CPU/GPU (percent)
    - advanced: fixed 6-levels fan speed for CPU/GPU (percent)

- `/sys/devices/platform/msi-ec/fw_version`
  - Description: This entry reports the firmware version of the motherboard.
  - Access: Read
  - Valid values: Represented as string

- `/sys/devices/platform/msi-ec/fw_release_date`
  - Description: This entry reports the firmware release date of the motherboard.
  - Access: Read
  - Valid values: Represented as string

- `/sys/devices/platform/msi-ec/cpu/realtime_temperature`
  - Description: This entry reports the current cpu temperature.
  - Access: Read
  - Valid values: 0 - 100 (celsius scale)

- `/sys/devices/platform/msi-ec/cpu/realtime_fan_speed`
  - Description: This entry reports the current cpu fan speed.
  - Access: Read
  - Valid values: 0 - 100 (percent)

- `/sys/devices/platform/msi-ec/gpu/realtime_temperature`
  - Description: This entry reports the current gpu temperature.
  - Access: Read
  - Valid values: 0 - 100 (celsius scale)

- `/sys/devices/platform/msi-ec/gpu/realtime_fan_speed`
  - Description: This entry reports the current gpu fan speed.
  - Access: Read
  - Valid values: 0 - 100 (percent)

In addition to these platform device attributes the driver registers itself in the Linux power_supply subsystem (Documentation/ABI/testing/sysfs-class-power) and is available to userspace under:

- `/sys/class/power_supply/<supply_name>/charge_control_start_threshold`
  - Description: Represents a battery percentage level, below which charging will begin.
  - Access: Read, Write
  - Valid values: 0 - 100 (percent)
    - 50: when min battery mode is configured
    - 70: when medium battery mode is configured
    - 90: when max battery mode is configured

- `/sys/class/power_supply/<supply_name>/charge_control_end_threshold`
  - Description: Represents a battery percentage level, above which charging will stop.
  - Access: Read, Write
  - Valid values: 0 - 100 (percent)
    - 60: when min battery mode is configured
    - 80: when medium battery mode is configured
    - 100: when max battery mode is configured


List of tested laptops:

- MSI GF75 Thin 9SC (17F2EMS1.106)
