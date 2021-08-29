# MSI Embedded Controller for laptops

This driver exports a few files in its own platform device, msi-ec, and is available to userspace under:

- `/sys/devices/platform/msi-ec/webcam`
  - Description: This entry allows enabling the integrated webcam.
  - Access: Read, Write
  - Valid values:
    - on: enabled
    - off: disabled

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

- `/sys/devices/platform/msi-ec/webcam`
  - Description: This entry allows enabling the integrated webcam.
  - Access: Read, Write
  - Valid values:
    - on: integrated webcam is enabled
    - off: integrated webcam is disabled

In addition to these platform device attributes the driver registers itself in the Linux power_supply subsystem (Documentation/ABI/testing/sysfs-class-power) and is available to userspace under:

- `/sys/class/power_supply/<supply_name>/charge_control_start_threshold`
  - Description: Represents a battery percentage level, below which charging will begin.
  - Access: Read, Write
  - Valid values: 0 - 100 (percent)

- `/sys/class/power_supply/<supply_name>/charge_control_end_threshold`
  - Description: Represents a battery percentage level, above which charging will stop.
  - Access: Read, Write
  - Valid values: 0 - 100 (percent)

This driver might not work on other laptops produced by MSI. Also, and until future enhancements, no DMI data are used to identify your compatibility.
