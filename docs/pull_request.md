# Pull Request: hwmon Integration, Fan Spec Database & GV62 Board Support

## Summary

This PR adds three related but independent improvements to `msi-ec`:

1. **Linux hwmon subsystem integration** — standard `sensors`, `fancontrol`,
   and hwmon-aware tools now work with MSI fan and temperature data out of
   the box, without any changes to the existing platform sysfs interface.

2. **Fan specification database** (`msi_fan_specs.h`) — a new header
   providing the hardware maximum RPM for every board in the driver, sourced
   from the official MSI spare parts catalog where available.  121 entries
   covering 135 board IDs across the full MSI laptop range.

3. **Fan channel consistency checker** (`tools/msi-fan-check.sh`) — a
   diagnostic tool that reads all four reporting channels simultaneously
   (EC debug registers, platform sysfs, hwmon sysfs, `sensors`) and
   cross-validates them.  Useful for board bringup, verifying EC address
   mappings, and confirming RPM calibration.

As a concrete example of the full workflow, board support for the
**MSI GV62 7RD** (MS-16J9, `16J9EMS1.112`) is included, with all thermal
and fan control addresses confirmed through live hardware testing.

---

## Linux-Native Board Bringup — Paradigm Shift

> This is the most significant methodological contribution of this PR for
> the broader contributor community.

The upstream `docs/device_support_guide.md` presents **Windows + RWEverything
as the recommended** method for EC address discovery, with Linux listed as a
"limited" secondary option.  This PR demonstrates that assumption is wrong.

The entire GV62 7RD board — fan speeds, temperatures, cooler boost, and all
three fan mode values — was brought up to full production quality using
**only Linux tools**, specifically:

```bash
# Read a single EC register
echo 71 > /sys/devices/platform/msi-ec/debug/ec_get
cat /sys/devices/platform/msi-ec/debug/ec_get

# Write a value and observe the effect
echo "f4=1c" > /sys/devices/platform/msi-ec/debug/ec_set

# Full memory snapshot for diff-based address discovery
cat /sys/devices/platform/msi-ec/debug/ec_dump > before.hex
# ... toggle a feature ...
cat /sys/devices/platform/msi-ec/debug/ec_dump > after.hex
diff before.hex after.hex
```

The `msi-fan-check.sh` tool provides the objective cross-validation step —
six PASS/FAIL checks confirming that EC registers, platform sysfs, hwmon
sysfs, and `sensors` all agree — that RWEverything would otherwise deliver
visually through its live EC table in Windows.

**The only feature that genuinely required Windows previously** was switching
between fan modes (silent/auto/advanced) to discover the mode register values.
This is now also solvable on Linux via `ec_set` + fan speed measurement:
write a candidate value, watch the fan respond, re-read to confirm the write
landed.

**Proposed update to `docs/device_support_guide.md`**: Elevate the Linux
method from "very limited compared to the Windows method" to a first-class
bringup path.  Add a section documenting the `ec_set` + `msi-fan-check`
workflow as the verification standard.  Reserve Windows as recommended only
for users who already have it installed and want the point-and-click
RWEverything UI.



After installing `msi-ec`, users frequently ask why `sensors` shows no fan
data even though the driver works.  The answer was that the driver didn't
register with the hwmon subsystem.  This PR closes that gap permanently, and
adds the infrastructure (fan spec database) needed to do the RPM conversion
correctly for every supported board rather than only for the developer's own
hardware.

---

## What changes in `msi-ec.c`

- New includes: `<linux/hwmon.h>`, `"msi_fan_specs.h"`, `<linux/dmi.h>`
- Two new state variables for the hwmon device handle and resolved fan limits
- ~120 lines: hwmon channel configuration, read/visibility callbacks, and
  `msi_hwmon_register()` function
- Three lines in `msi_ec_init()` to call `msi_hwmon_register()` (non-fatal
  if it fails)
- `CONF_GV62_16J9` board configuration block

No existing code paths, sysfs attributes, or driver behaviours are modified.

---

## Testing

Built and validated on MSI GV62 7RD (MS-16J9), kernel 7.0.0-15-generic,
Ubuntu 26.04.

The consistency checker achieved **6/6 PASS** across two separate runs at
different fan speeds and temperatures, confirming the complete data path from
EC register through platform sysfs through hwmon sysfs through `sensors(1)`.

---

## Files changed

```
msi-ec.c                          modified
msi_fan_specs.h                   new
tools/msi-fan-check.sh            new
docs/hwmon_integration.md         new
docs/fan_spec_database.md         new
docs/msi_fan_check.md             new
CHANGES.md                        new
```

---

## Checklist

- [x] Builds cleanly with `-Wall -Wextra` on kernel 7.0.0-15-generic
- [x] No existing sysfs paths, attributes, or behaviours changed
- [x] hwmon failure is non-fatal (driver loads normally on unsupported boards)
- [x] All 121 database entries compiled and array-size-validated
- [x] CHANGES.md documents every modification with rationale
- [x] Three new documentation files in `docs/`
- [ ] GV62 remaining unknowns (webcam, Fn/Win, shift mode, backlight) noted
      in CHANGES.md; separate PR to follow

---

---

# README Additions (to be merged into README.md)

The sections below should be inserted into the upstream README after the
existing "Usage" section.

---

## Fan Speed in RPM via `sensors`

The driver registers with the Linux hwmon subsystem, making fan speeds and
temperatures available to standard monitoring tools.

After installation, run:

```bash
sensors
```

Expected output:

```
msi_ec-isa-0000
Adapter: ISA adapter
cpu_fan:  2688 RPM
gpu_fan:     0 RPM
cpu_temp: +61.0°C
gpu_temp: +44.0°C
```

> [!NOTE]
> If `sensors` does not show `msi_ec-isa-0000`, run `sudo sensors-detect`
> and accept the defaults, then try again.

The GPU fan showing 0 RPM is normal when the GPU is below ~50–55°C — most
MSI laptops have a fan-stop feature that holds the GPU fan off at low
temperatures.  Under load, it will spin up automatically.

The hwmon data is also available directly:

```bash
HWMON=$(grep -rl "^msi_ec$" /sys/class/hwmon/*/name | head -1 | xargs dirname)
cat $HWMON/fan1_input    # CPU fan RPM
cat $HWMON/fan2_input    # GPU fan RPM
cat $HWMON/temp1_input   # CPU temperature in millidegrees (÷1000 for °C)
cat $HWMON/temp2_input   # GPU temperature in millidegrees
```

For technical details of the hwmon integration, see
[docs/hwmon_integration.md](docs/hwmon_integration.md).

---

## Fan RPM Calibration

The RPM conversion requires the hardware maximum RPM for your specific fan
assembly.  This is stored in a built-in database (`msi_fan_specs.h`) with
entries for every supported board.

If the fan label reads `cpu_fan (unvalidated)`, the database does not yet
have a confirmed entry for your board and RPM values are estimates.  In that
case, back-calculate the actual max RPM from a live reading:

```bash
PLAT=/sys/devices/platform/msi-ec
HWMON=$(grep -rl "^msi_ec$" /sys/class/hwmon/*/name | head -1 | xargs dirname)

PCT=$(cat $PLAT/cpu/realtime_fan_speed)
RPM=$(cat $HWMON/fan1_input)
echo "Derived max RPM: $(( RPM * 100 / PCT ))"  # only valid when PCT > 0
```

Compare the result to the fan assembly listed on your laptop's
[MSI spare parts page](https://eu-spareparts.msi.com) and contribute the
confirmed value back via a pull request.
See [docs/fan_spec_database.md](docs/fan_spec_database.md) for the process.

---

## Diagnostic Tool: `msi-fan-check`

`tools/msi-fan-check.sh` reads all reporting channels simultaneously and
cross-validates them.  It is useful for:

- Verifying a new board's EC address mapping is correct
- Confirming RPM calibration for a new database entry
- Debugging `sensors` output that doesn't match expectations

```bash
# Single snapshot (load with debug=1 for full EC cross-check)
sudo rmmod msi_ec && sudo insmod msi-ec.ko debug=1
sudo bash tools/msi-fan-check.sh

# Live monitoring
sudo bash tools/msi-fan-check.sh --watch

# Machine-readable output
sudo bash tools/msi-fan-check.sh --json

# Scripting / CI: exit 0 = all consistent, exit 1 = mismatch
sudo bash tools/msi-fan-check.sh --quiet
```

Full documentation: [docs/msi_fan_check.md](docs/msi_fan_check.md).
