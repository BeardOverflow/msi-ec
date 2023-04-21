# Memory map for known models

## Disclaimer

All presented data is copy-paste from [msi-ec.c](./msi-ec.c) with minor changes in comments

## charge_control

| FW                         | address | offset_start | offset_end | range_min | range_max |
|----------------------------|---------|--------------|------------|-----------|-----------|
| 14C1EMS1.{012,101,102}     | 0xef    | 0x8a         | 0x80       | 0x8a      | 0xe4      |
| 17F2EMS1.{103,104,106,107} | 0xef    | 0x8a         | 0x80       | 0x8a      | 0xe4      |
| 1552EMS1.118               | 0xd7    | 0x8a         | 0x80       | 0x8a      | 0xe4      |
| 1592EMS1.111, E1592IMS.10C | 0xef    | 0x8a         | 0x80       | 0x8a      | 0xe4      |
| 16V4EMS1.114               | 0xd7    | 0x8a         | 0x80       | 0x8a      | 0xe4      |
| 158LEMS1.{103,105,106}     | 0xef    | 0x8a         | 0x80       | 0x8a      | 0xe4      |
| 1542EMS1.{102,104}         | 0xef    | 0x8a         | 0x80       | 0x8a      | 0xe4      |
| 17FKEMS1.{108,109,10A}     | 0xef    | 0x8a         | 0x80       | 0x8a      | 0xe4      |
| 14F1EMS1.115               | 0xd7    | 0x8a         | 0x80       | 0x8a      | 0xe4      |
| 14JKEMS1.104               | 0xef    | 0x8a         | 0x80       | 0x8a      | 0xe4      |
| 1581EMS1.107 //d7=80 ef=00 | unknown | 0x8a         | 0x80       | 0x8a      | 0xe4      |

## webcam

| FW                         | address | block_address      | bit |
|----------------------------|---------|--------------------|-----|
| 14C1EMS1.{012,101,102}     | 0x2e    | 0x2f               | 1   |
| 17F2EMS1.{103,104,106,107} | 0x2e    | 0x2f               | 1   |
| 1552EMS1.118               | 0x2e    | 0x2f               | 1   |
| 1592EMS1.111, E1592IMS.10C | 0x2e    | 0x2f               | 1   |
| 16V4EMS1.114               | 0x2e    | 0x2f               | 1   |
| 158LEMS1.{103,105,106}     | 0x2e    | 0x2f               | 1   |
| 1542EMS1.{102,104}         | 0x2e    | MSI_EC_ADDR_UNSUPP | 1   |
| 17FKEMS1.{108,109,10A}     | 0x2e    | MSI_EC_ADDR_UNSUPP | 1   |
| 14F1EMS1.115               | 0x2e    | MSI_EC_ADDR_UNSUPP | 1   |
| 14JKEMS1.104               | 0x2e    | 0x2f               | 1   |
| 1581EMS1.107 // untested   | 0x2e ?  | 0x2f or unsupp     | 1 ? |

## fn_win_swap

| FW                         | address               | bit |
|----------------------------|-----------------------|-----|
| 14C1EMS1.{012,101,102}     | 0xbf                  | 4   |
| 17F2EMS1.{103,104,106,107} | 0xbf                  | 4   |
| 1552EMS1.118               | 0xe8                  | 4   |
| 1592EMS1.111, E1592IMS.10C | 0xe8                  | 4   |
| 16V4EMS1.114               | MSI_EC_ADDR_UNKNOWN   | 4   |
| 158LEMS1.{103,105,106}     | 0xbf //todo           | 4   |
| 1542EMS1.{102,104}         | 0xbf //todo           | 4   |
| 17FKEMS1.{108,109,10A}     | 0xbf //todo           | 4   |
| 14F1EMS1.115               | 0xe8                  | 4   |
| 14JKEMS1.104               | 0xbf                  | 4   |
| 1581EMS1.107 // untested   | 0xbf (bit 4 is unset) | 4   |

## cooler_boost

| FW                         | address | bit |
|----------------------------|---------|-----|
| 14C1EMS1.{012,101,102}     | 0x98    | 7   |
| 17F2EMS1.{103,104,106,107} | 0x98    | 7   |
| 1552EMS1.118               | 0x98    | 7   |
| 1592EMS1.111, E1592IMS.10C | 0x98    | 7   |
| 16V4EMS1.114               | 0x98    | 7   |
| 158LEMS1.{103,105,106}     | 0x98    | 7   |
| 1542EMS1.{102,104}         | 0x98    | 7   |
| 17FKEMS1.{108,109,10A}     | 0x98    | 7   |
| 14F1EMS1.115               | 0x98    | 7   |
| 14JKEMS1.104               | 0x98    | 7   |
| 1581EMS1.107 // untested   | 0x98    | 7   |

## shift_mode

| FW                           | address | SM_ECO_NAME | SM_COMFORT_NAME | SM_SPORT_NAME | SM_TURBO_NAME |
|------------------------------|---------|-------------|-----------------|---------------|---------------|
| 14C1EMS1.{012,101,102}       | 0xf2    | 0xc2        | 0xc1            | 0xc0          |               |
| 17F2EMS1.{103,104,106,107}   | 0xf2    | 0xc2        | 0xc1            | 0xc0          | 0xc4          |
| 1552EMS1.118                 | 0xf2    | 0xc2        | 0xc1            | 0xc0          |               |
| 1592EMS1.111, E1592IMS.10C   | 0xd2    | 0xc2        | 0xc1            | 0xc0          |               |
| 16V4EMS1.114                 | 0xd2    | 0xc2        | 0xc1            | 0xc0          |               |
| 158LEMS1.{103,105,106}       | 0xf2    | 0xc2        | 0xc1            |               | 0xc4          |
| 1542EMS1.{102,104}           | 0xf2    | 0xc2        | 0xc1            | 0xc0          | 0xc4          |
| 17FKEMS1.{108,109,10A}       | 0xf2    | 0xc2        | 0xc1            | 0xc0          | 0xc4          |
| 14F1EMS1.115                 | 0xd2    | 0xc2        | 0xc1            | 0xc0          |               |
| 14JKEMS1.104                 | 0xf2    | 0xc2        | 0xc1            | 0xc0          |               |
| 1581EMS1.107 // mode unknown | 0xd2    | 0xc2 ?      | 0xc1            | 0xc0 ?        | 0xc4 ?        |

## super_battery

| FW                         | address             | mask | extras                                   |
|----------------------------|---------------------|------|------------------------------------------|
| 14C1EMS1.{012,101,102}     | MSI_EC_ADDR_UNKNOWN |      | 0xd5?                                    |
| 17F2EMS1.{103,104,106,107} | MSI_EC_ADDR_UNKNOWN |      |                                          |
| 1552EMS1.118               | 0xeb                | 0x0f |                                          |
| 1592EMS1.111, E1592IMS.10C | 0xeb                | 0x0f |                                          |
| 16V4EMS1.114               | MSI_EC_ADDR_UNKNOWN | 0x0f | may be supported, but address is unknown |
| 158LEMS1.{103,105,106}     | MSI_EC_ADDR_UNKNOWN | 0x0f | unsupported?                             |
| 1542EMS1.{102,104}         | 0xd5                | 0x0f |                                          |
| 17FKEMS1.{108,109,10A}     | MSI_EC_ADDR_UNKNOWN | 0x0f | 0xd5 but has its own wet of modes        |
| 14F1EMS1.115               | 0xeb                | 0x0f |                                          |
| 14JKEMS1.104               | MSI_EC_ADDR_UNSUPP  |      | unsupported or enabled by ECO shift      |
| 1581EMS1.107               | MSI_EC_ADDR_UNKNOWN |      | d5=00 eb=00, may be unsupported          |

## fan_mode

> suggested enhancement: use bit masks instead of values from EC dump

> note from 14JKEMS1: changing low nibble does nothing, bit 5 unused, bit 6 zeroed by MSI app if set

| FW                           | address | FM_AUTO_NAME | FM_SILENT_NAME | FM_BASIC_NAME | FM_ADVANCED_NAME |
|------------------------------|---------|--------------|----------------|---------------|------------------|
| 14C1EMS1.{012,101,102}       | 0xf4    | 0x0d         | 0x1d           | 0x4d          | 0x8d             |
| 17F2EMS1.{103,104,106,107}   | 0xf4    | 0x0d         |                | 0x4d          | 0x8d             |
| 1552EMS1.118                 | 0xd4    | 0x0d         | 0x1d           | 0x4d          | 0x8d             |
| 1592EMS1.111, E1592IMS.10C   | 0xd4    | 0x0d         | 0x1d           | 0x4d          | 0x8d             |
| 16V4EMS1.114                 | 0xd4    | 0x0d         | 0x1d           |               | 0x8d             |
| 158LEMS1.{103,105,106}       | 0xf4    | 0x0d         | 0x1d           |               | 0x8d             |
| 1542EMS1.{102,104}           | 0xf4    | 0x0d         | 0x1d           |               | 0x8d             |
| 17FKEMS1.{108,109,10A}       | 0xf4    | 0x0d         | 0x1d           |               | 0x8d             |
| 14F1EMS1.115                 | 0xd4    | 0x0d         | 0x1d           | 0x4d          |                  |
| 14JKEMS1.104                 | 0xf4    | 0x0d         | 0x1d           |               | 0x8d             |
| 1581EMS1.107 // mode unknown | 0xd4    | 0x0d         | 0x1d ?         | 0x4d ?        | 0x8d ?           |

## cpu

| FW                         | rt_temp_address       | rt_fan_speed_address  | rt_fan_speed_base_min | rt_fan_speed_base_max      | bs_fan_speed_address | bs_fan_speed_base_min | bs_fan_speed_base_max |
|----------------------------|-----------------------|-----------------------|-----------------------|----------------------------|----------------------|-----------------------|-----------------------|
| 14C1EMS1.{012,101,102}     | 0x68                  | 0x71                  | 0x19                  | 0x37                       | 0x89                 | 0x00                  | 0x0f                  |
| 17F2EMS1.{103,104,106,107} | 0x68                  | 0x71                  | 0x19                  | 0x37                       | 0x89                 | 0x00                  | 0x0f                  |
| 1552EMS1.118               | 0x68                  | 0x71                  | 0x19                  | 0x37                       | 0x89                 | 0x00                  | 0x0f                  |
| 1592EMS1.111, E1592IMS.10C | 0x68                  | 0xc9                  | 0x19                  | 0x37                       | 0x89 // ?            | 0x00                  | 0x0f                  |
| 16V4EMS1.114               | 0x68 // needs testing | 0x71 // needs testing | 0x19                  | 0x37                       | MSI_EC_ADDR_UNKNOWN  | 0x00                  | 0x0f                  |
| 158LEMS1.{103,105,106}     | 0x68 // needs testing | 0x71 // needs testing | 0x19                  | 0x37                       | MSI_EC_ADDR_UNSUPP   | 0x00                  | 0x0f                  |
| 1542EMS1.{102,104}         | 0x68                  | 0xc9                  | 0x19                  | 0x37                       | MSI_EC_ADDR_UNSUPP   | 0x00                  | 0x0f                  |
| 17FKEMS1.{108,109,10A}     | 0x68                  | 0xc9 // needs testing | 0x19                  | 0x37                       | MSI_EC_ADDR_UNSUPP   | 0x00                  | 0x0f                  |
| 14F1EMS1.115               | 0x68                  | 0x71                  | 0x19                  | 0x37                       | MSI_EC_ADDR_UNSUPP   | 0x00                  | 0x0f                  |
| 14JKEMS1.104               | 0x68                  | 0x71                  | 0x19 // may be 0      | 0x37 // may be decimal 150 | MSI_EC_ADDR_UNSUPP   | 0x00                  | 0x0f                  |
| 1581EMS1.107 // untested   | 0x68 // default?      | 0x71 // default?      | 0x19                  | 0x37                       | unknown (0x89=2b) ?  | 0x00                  | 0x0f                  |

## gpu

| FW                         | rt_temp_address     | rt_fan_speed_address |
|----------------------------|---------------------|----------------------|
| 14C1EMS1.{012,101,102}     | 0x80                | 0x89                 |
| 17F2EMS1.{103,104,106,107} | 0x80                | 0x89                 |
| 1552EMS1.118               | 0x80                | 0x89                 |
| 1592EMS1.111, E1592IMS.10C | 0x80                | 0x89                 |
| 16V4EMS1.114               | 0x80                | MSI_EC_ADDR_UNKNOWN  |
| 158LEMS1.{103,105,106}     | MSI_EC_ADDR_UNKNOWN | MSI_EC_ADDR_UNKNOWN  |
| 1542EMS1.{102,104}         | 0x80                | MSI_EC_ADDR_UNKNOWN  |
| 17FKEMS1.{108,109,10A}     | MSI_EC_ADDR_UNKNOWN | MSI_EC_ADDR_UNKNOWN  |
| 14F1EMS1.115               | MSI_EC_ADDR_UNKNOWN | MSI_EC_ADDR_UNKNOWN  |
| 14JKEMS1.104               | MSI_EC_ADDR_UNSUPP  | MSI_EC_ADDR_UNSUPP   |
| 1581EMS1.107 // untested   | 0x80 // default?    | 0x89 // default?     |

## leds

| FW                         | micmute_led_address | mute_led_address    | bit |
|----------------------------|---------------------|---------------------|-----|
| 14C1EMS1.{012,101,102}     | 0x2b                | 0x2c                | 2   |
| 17F2EMS1.{103,104,106,107} | 0x2b                | 0x2c                | 2   |
| 1552EMS1.118               | 0x2c                | 0x2d                | 1   |
| 1592EMS1.111, E1592IMS.10C | 0x2b                | 0x2c                | 1   |
| 16V4EMS1.114               | MSI_EC_ADDR_UNKNOWN | MSI_EC_ADDR_UNKNOWN | 1   |
| 158LEMS1.{103,105,106}     | 0x2b                | 0x2c                | 2   |
| 1542EMS1.{102,104}         | MSI_EC_ADDR_UNSUPP  | MSI_EC_ADDR_UNSUPP  | 2   |
| 17FKEMS1.{108,109,10A}     | MSI_EC_ADDR_UNSUPP  | 0x2c                | 2   |
| 14F1EMS1.115               | MSI_EC_ADDR_UNSUPP  | 0x2d                | 1   |
| 14JKEMS1.104               | 0x2b                | 0x2c                | 2   |
| 1581EMS1.107 // 2b,2c=00   | MSI_EC_ADDR_UNKNOWN | MSI_EC_ADDR_UNKNOWN |     |

## kbd_bl

> In the ```msi_ec_conf.kbd_bl``` structure many values are question-marked and look like they are just copy-pasted for
> compatibility. ```bl_mode_address```, ```bl_modes```, ```max_mode``` added just for reference

| FW                         | bl_mode_address     | bl_modes       | max_mode | bl_state_address                           | state_base_value | max_state |
|----------------------------|---------------------|----------------|----------|--------------------------------------------|------------------|-----------|
| 14C1EMS1.{012,101,102}     | 0x2c                | { 0x00, 0x08 } | 1        | 0xf3                                       | 0x80             | 3         |
| 17F2EMS1.{103,104,106,107} | 0x2c                | { 0x00, 0x08 } | 1        | 0xf3                                       | 0x80             | 3         |
| 1552EMS1.118               | 0x2c                | { 0x00, 0x08 } | 1        | 0xd3                                       | 0x80             | 3         |
| 1592EMS1.111, E1592IMS.10C | 0x2c                | { 0x00, 0x08 } | 1        | 0xd3                                       | 0x80             | 3         |
| 16V4EMS1.114               | MSI_EC_ADDR_UNKNOWN | { 0x00, 0x08 } | 1        | MSI_EC_ADDR_UNSUPP // 0xd3, not functional | 0x80             | 3         |
| 158LEMS1.{103,105,106}     | MSI_EC_ADDR_UNKNOWN | { 0x00, 0x08 } | 1        | MSI_EC_ADDR_UNSUPP // 0xf3, not functional | 0x80             | 3         |
| 1542EMS1.{102,104}         | MSI_EC_ADDR_UNKNOWN | { 0x00, 0x08 } | 1        | MSI_EC_ADDR_UNSUPP // 0xf3, not functional | 0x80             | 3         |
| 17FKEMS1.{108,109,10A}     | MSI_EC_ADDR_UNKNOWN | { 0x00, 0x08 } | 1        | 0xf3                                       | 0x80             | 3         |
| 14F1EMS1.115               | MSI_EC_ADDR_UNKNOWN | { 0x00, 0x08 } | 1        | MSI_EC_ADDR_UNSUPP // not functional       | 0x80             | 3         |
| 14JKEMS1.104               | MSI_EC_ADDR_UNSUPP  | { 0x00, 0x08 } | 1        | 0xf3                                       | 0x80             | 3         |
| 1581EMS1.107 // untested   | MSI_EC_ADDR_UNKNOWN | { 0x00, 0x08 } | 1        | 0xd3                                       | 0x80             | 3         |

## extras

<!--<details>
    <summary>14C1EMS1.{012,101,102}</summary>
</details>-->

<!--<details>
    <summary>17F2EMS1.{103,104,106,107}</summary>
</details>-->

<!--<details>
    <summary>1552EMS1.118</summary>
</details>-->

<!--<details>
    <summary>1592EMS1.111, E1592IMS.10C</summary>
</details>-->

<!--<details>
    <summary>16V4EMS1.114</summary>
</details>-->

<!--<details>
    <summary>158LEMS1.{103,105,106}</summary>
</details>-->

<!--<details>
    <summary>1542EMS1.{102,104}</summary>
</details>-->

<!--<details>
    <summary>17FKEMS1.{108,109,10A}</summary>
</details>-->

<!--<details>
    <summary>14F1EMS1.115</summary>
</details>-->

<details>
    <summary>14JKEMS1.104</summary>

| name             | address, bit, value          | description                                                                                                                                                         |
|------------------|------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| FN-lock get      | 0x2b bit 3                   | read only                                                                                                                                                           |
| cooling profile  | 0x72-77, 0-150% in hex       | advanced fan mode, but works in other modes                                                                                                                         |
| charging state   | 0x30-31, DANGEROUS!!!        | something related to charging, setting 0x30 bit 0 to HIGH with unplugged charger triggers charging state with powering on charging led and charging mark on windows |
| realtime fan RPM | 0xcd, around 4e-d2, inverted | value proportional to EC cooler RPM in HWINFO64 (max 6128 = 0x4e, min around 2k = 0xd2)                                                                             |
| fan enabled      | 0x33 bit 3                   |                                                                                                                                                                     |
| battery level    | 0x42                         | value in %                                                                                                                                                          |
| power-bank mode  | to be researched             |                                                                                                                                                                     |

</details>
