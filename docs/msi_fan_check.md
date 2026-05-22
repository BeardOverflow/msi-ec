# `msi-fan-check` — Fan & Temperature Consistency Checker

`msi-fan-check.sh` is a diagnostic tool that reads MSI laptop fan and
temperature data from **all four reporting channels simultaneously** and
cross-validates them against each other.

It is the definitive tool for:
- Verifying that a new board's EC address mapping is correct
- Confirming that the hwmon RPM conversion formula is accurate for a board
- Debugging discrepancies between what `sensors` reports and what the EC
  actually contains
- Back-calculating the hardware maximum RPM from a live system

---

## Prerequisites

| Requirement | Notes |
|---|---|
| `msi-ec` driver loaded | Any version with EC debug support |
| Root / sudo | Required for EC debug register reads |
| `bash` ≥ 4.0 | Standard on all modern Linux distributions |
| `lm-sensors` | Optional; enables checks 5 and 6 |
| `sensors-detect` run | Optional; needed for `sensors` to recognise the device |

---

## Installation

```bash
# Copy to driver directory
cp msi-fan-check.sh ~/msi-ec-installation/

# Or run directly from the repository
sudo bash tools/msi-fan-check.sh
```

---

## Usage

```
sudo bash msi-fan-check.sh [--watch | --json | --quiet]
```

| Flag | Mode |
|---|---|
| *(none)* | Single snapshot — full decorated table with consistency checks |
| `--watch` | Continuous — clears screen and refreshes every 2 seconds; Ctrl-C to stop |
| `--json` | Machine-readable — single JSON object, all values as quoted strings |
| `--quiet` | Checks only — no table, no decoration; exits 0 if all pass/N/A, 1 if any FAIL |
| `--help` | Print usage and exit |

> [!NOTE]
> The script must be run as root because EC debug register reads require
> write access to `/sys/devices/platform/msi-ec/debug/ec_get`.

---

## Output Reference

### Normal mode

```
╔══════════════════════════════════════════════════════════════════════╗
║        MSI Fan & Temperature  —  All-Channel Consistency Check       ║
╚══════════════════════════════════════════════════════════════════════╝
  17:32:38   FW: 16J9EMS1.112   mode: auto   cooler-boost: off
  Channels: ● platform-sysfs   ● hwmon-sysfs   ● EC-debug   ● sensors(1)

  Channel / Attribute                     CPU                   GPU
  ──────────────────────────────────────  ────────────────────  ──────────────────
  EC debug  fan%     (0x71 / 0x89)        0x38 = 56%            0x36 = 54%
  EC debug  temp°C   (0x68 / 0x80)        0x3c = 60°C           0x32 = 50°C

  Platform  fan%  realtime_fan_speed       56%                   54%
  Platform  temp  realtime_temperature     60°C                  50°C

  hwmon     fan   fan1/fan2_input          2688 RPM              2592 RPM
  hwmon     label fan1/fan2_label          cpu_fan               gpu_fan
  hwmon     temp  temp1/2_input            60000 m°C             50000 m°C
  hwmon     label temp1/2_label            cpu_temp              gpu_temp

  sensors(1) fan RPM                       2688 RPM              2592 RPM
  sensors(1) temp °C                       60°C                  50°C

    └─ derived max_rpm  (RPM×100/pct)      4800 RPM              4800 RPM
  ──────────────────────────────────────  ────────────────────  ──────────────────
  Consistency checks:
  ───────────────────────────────────────────────  ─────────────────  ─────────────────
  EC fan reg  == platform fan %     (exact)        CPU: PASS          GPU: PASS
  EC temp reg == platform temp °C   (exact)        CPU: PASS          GPU: PASS
  platform °C × 1000 == hwmon mc    (exact)        CPU: PASS          GPU: PASS
  platform % → RPM ≈ hwmon RPM      (±1 RPM)       CPU: PASS          GPU: PASS
  sensors RPM == hwmon RPM           (exact)        CPU: PASS          GPU: PASS
  sensors °C  == hwmon mc/1000       (exact)        CPU: PASS          GPU: PASS
  ───────────────────────────────────────────────  ─────────────────  ─────────────────

  ✔  All channels consistent.
```

**Header line** shows firmware version, current fan mode, and cooler boost
state.

**Channel status** shows which of the four channels are available:
- `●` (green) — channel is present and readable
- `○` (yellow) — channel is available on this hardware but inactive;
  instructions for enabling it are printed below
- `✗` (red) — channel is absent; likely means the driver is not loaded

**Data table** reads all four channels simultaneously and displays raw and
converted values side by side.

**Derived max_rpm** is back-calculated as `RPM × 100 / pct`.  This is the
empirical hardware maximum RPM; compare it against the fan spec database
entry for your board to validate both.

**Consistency checks** — see the section below.

**Summary line** — `✔ All channels consistent.` or `✘ N check(s) FAILED.`
with diagnostic hints if there are failures.

### EC debug absent

When the module is loaded without `debug=1`, the EC debug channel is
unavailable:

```
  Channels: ● platform-sysfs   ● hwmon-sysfs   ○ EC-debug   ● sensors(1)
            -- EC debug absent     — reload with:  insmod msi-ec.ko debug=1
```

Checks 1 and 2 (EC register vs. platform) will report `N/A` rather than
FAIL.  All other checks still run normally.

To enable the EC debug channel:

```bash
sudo rmmod msi_ec
sudo insmod /path/to/msi-ec.ko debug=1
sudo bash msi-fan-check.sh
```

---

## Consistency Checks

Six cross-checks are performed.  Each reports `PASS`, `FAIL`, or `N/A`
(N/A means one or both values were unavailable — not a failure).

### Check 1: EC fan register == platform fan %

**Compares**: The raw byte read directly from the EC fan register
(`0x71` for CPU, `0x89` for GPU on GV62) against the value reported by
`/sys/devices/platform/msi-ec/cpu/realtime_fan_speed`.

**Tolerance**: Exact match.

**FAIL means**: The EC address configured for the fan speed register is
wrong for this board.  The platform sysfs attribute is reading a different
byte than the one that actually contains fan speed.

**Requires**: EC debug channel (`debug=1`).

### Check 2: EC temp register == platform temp °C

**Compares**: The raw byte read from the EC temperature register
(`0x68` for CPU, `0x80` for GPU on GV62) against
`/sys/devices/platform/msi-ec/cpu/realtime_temperature`.

**Tolerance**: Exact match.

**FAIL means**: The EC address for the temperature register is wrong.

**Requires**: EC debug channel.

### Check 3: platform °C × 1000 == hwmon millideg

**Compares**: `platform_temp_c × 1000` against `hwmon_temp_input`.

**Tolerance**: Exact match.

**FAIL means**: Bug in the hwmon `temp_read()` callback.  The conversion
from EC °C to millidegrees is incorrect in the driver code.

**Requires**: hwmon channel.

### Check 4: platform % → RPM round-trip (±1 RPM)

**Compares**: `(platform_fan_pct × derived_max_rpm / 100)` against
`hwmon_fan_input`.

**Tolerance**: ±1 RPM (integer division truncation).

**FAIL means**: Either the `max_rpm` value in the fan spec database is wrong
for this board, or there is a bug in the hwmon `fan_read()` callback.

When the fan is at 0% (fan-stop mode), the check verifies that
`hwmon_fan_input == 0` exactly.

**Requires**: hwmon channel and fan running at >0%.

### Check 5: sensors(1) RPM == hwmon RPM

**Compares**: RPM value parsed from `sensors` output against
`hwmon_fan_input`.

**Tolerance**: Exact match (both read from the same sysfs file).

**FAIL means**: `libsensors` is reading a different hwmon device than the
one identified as `msi_ec`, or parsing the output of a different chip.

**Requires**: `lm-sensors` installed, `sensors-detect` run at least once.

### Check 6: sensors(1) °C integer == hwmon mc/1000

**Compares**: The integer part of the temperature reported by `sensors`
against `hwmon_temp_input / 1000`.

**Tolerance**: Integer comparison (sensors reports one decimal place;
millidegree precision within 1°C is acceptable).

**FAIL means**: Same as check 5 — `libsensors` chip configuration mismatch.

**Requires**: `lm-sensors` installed.

---

## Diagnosing Specific Failures

### `FAIL` on EC fan register == platform fan %

The EC address configured for fan speed in the board's `msi_ec_conf` struct
(`rt_fan_speed_address`) is reading the wrong EC byte.  Follow the
[device support guide](device_support_guide.md) to locate the correct
address, then update the board configuration.

### `FAIL` on platform % → RPM (±1 RPM)

First check the derived max_rpm in the table.  If it differs substantially
from what the fan spec database says, update the database entry.  If the
database entry looks correct, re-examine the `msi_hwmon_read()` function for
an arithmetic error.

### `FAIL` on sensors vs. hwmon

Run:

```bash
sensors -u 2>/dev/null | grep -A 20 msi_ec
```

If `msi_ec-isa-*` does not appear, run `sudo sensors-detect` and accept the
defaults.  If it appears but shows different values from `hwmon` sysfs,
check whether multiple `msi_ec` hwmon devices exist:

```bash
grep -r "^msi_ec$" /sys/class/hwmon/*/name
```

### All checks N/A

The module is probably loaded without `debug=1` **and** `lm-sensors` is not
installed.  The platform and hwmon checks (3 and 4) should still run.  If
those are also N/A, the driver may not be loaded:

```bash
lsmod | grep msi_ec
cat /sys/devices/platform/msi-ec/fw_version
```

---

## JSON Output

`--json` produces a single JSON object suitable for ingestion by monitoring
pipelines, test harnesses, or scripts.  All values are quoted strings; `N/A`
is used where a channel is unavailable.

```json
{
  "timestamp":    "2025-08-14 17:32:38",
  "fw_version":   "16J9EMS1.112",
  "fan_mode":     "auto",
  "cooler_boost": "off",
  "cpu": {
    "ec_fan_hex":     "38",
    "ec_fan_pct":     "56",
    "ec_temp_hex":    "3c",
    "ec_temp_c":      "60",
    "platform_pct":   "56",
    "platform_c":     "60",
    "hwmon_rpm":      "2688",
    "hwmon_label":    "cpu_fan",
    "hwmon_mc":       "60000",
    "hwmon_tlabel":   "cpu_temp",
    "sensors_rpm":    "2688",
    "sensors_c":      "60",
    "derived_maxrpm": "4800"
  },
  "gpu": { ... }
}
```

---

## Quiet Mode (Scripting)

`--quiet` is intended for automated testing, CI pipelines, or systemd
service checks:

```bash
sudo bash msi-fan-check.sh --quiet
echo "Exit code: $?"   # 0 = all pass or N/A; 1 = at least one FAIL
```

Example — alert if any channel diverges:

```bash
#!/bin/bash
if ! sudo bash /usr/local/bin/msi-fan-check.sh --quiet 2>/dev/null; then
    notify-send "MSI fan check FAILED" "Run msi-fan-check.sh for details"
fi
```

---

## Watch Mode

`--watch` is intended for live monitoring during stress testing, fan curve
verification, or board bringup:

```bash
sudo bash msi-fan-check.sh --watch
```

The display refreshes every 2 seconds.  The derived max_rpm field is
particularly useful during watch mode — as the fan spins up and down, the
back-calculated max_rpm should remain stable (within ±1 RPM from integer
rounding) if the database entry is correct.

---

## Interpreting the GPU Fan at 0 RPM

Many MSI laptops implement a **fan-stop** feature for the GPU fan: the EC
keeps the GPU fan off below a temperature threshold (typically 50–55°C), then
spins it up once that threshold is crossed.

When the GPU fan is at 0%:
- EC register reads `0x00`
- Platform sysfs reports `0`
- hwmon reports `0 RPM`
- Check 4 passes (0% → 0 RPM is exact)
- Check 1 passes (0x00 == 0)

This is correct, expected behaviour and is **not a failure**.  The threshold
is stored in the EC's fan curve table — on the GV62 7RD, for example, bytes
`0x81–0x88` in the EC dump contain the temperature steps starting at 54°C.

To force the GPU fan on for testing, run a GPU workload (e.g. `glxgears`,
a render benchmark, or `nvidia-smi -pl <power_limit>`) until the GPU
temperature exceeds the threshold and watch the GPU fan come on in
`--watch` mode.
