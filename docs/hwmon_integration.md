# hwmon Subsystem Integration

This document describes the Linux hwmon layer added to `msi-ec`, what it
provides, how it works internally, and how to use it.

---

## Background

The Linux kernel's **hwmon** (hardware monitoring) subsystem defines a
standard sysfs ABI under `/sys/class/hwmon/` that all monitoring tools
speak.  Prior to this work, `msi-ec` exposed fan and temperature data only
through its own proprietary platform sysfs paths
(`/sys/devices/platform/msi-ec/cpu/realtime_fan_speed`, etc.).  That
interface is perfectly functional but invisible to every standard tool:

| Tool | Reads hwmon? | Read msi-ec platform sysfs? |
|---|---|---|
| `sensors` (lm-sensors) | ✅ | ❌ |
| `fancontrol` | ✅ | ❌ |
| `psensor`, `lm-sensors` GUIs | ✅ | ❌ |
| Prometheus `node_exporter` | ✅ | ❌ |
| GNOME/KDE system monitors | ✅ | ❌ |
| Custom scripts via `cat` | ✅ | ✅ |

After this change, MSI fan and temperature data is available through both
interfaces simultaneously.  The platform interface is unchanged.

---

## What Is Exposed

The hwmon device registers as `msi_ec` and creates up to four channels:

```
/sys/class/hwmon/hwmonN/name         → "msi_ec"

/sys/class/hwmon/hwmonN/fan1_input   → CPU fan speed in RPM (integer, read-only)
/sys/class/hwmon/hwmonN/fan1_label   → "cpu_fan"

/sys/class/hwmon/hwmonN/fan2_input   → GPU fan speed in RPM (integer, read-only)
/sys/class/hwmon/hwmonN/fan2_label   → "gpu_fan"

/sys/class/hwmon/hwmonN/temp1_input  → CPU temperature in millidegrees Celsius
/sys/class/hwmon/hwmonN/temp1_label  → "cpu_temp"

/sys/class/hwmon/hwmonN/temp2_input  → GPU temperature in millidegrees Celsius
/sys/class/hwmon/hwmonN/temp2_label  → "gpu_temp"
```

Channels for which the board configuration has `MSI_EC_ADDR_UNSUPP` are
hidden automatically by `msi_hwmon_is_visible()` — a board without a GPU
fan address will expose exactly 2 channels (CPU fan + CPU temp) rather than
4.

### RPM conversion

The EC reports fan speed as a percentage (0–100, sometimes 0–150 on older
boards).  Converting to RPM requires the hardware maximum RPM for the
specific fan assembly fitted to each board.  This is provided by the fan
specification database (`msi_fan_specs.h`):

```
hwmon_rpm = (ec_pct × max_rpm) / 100
```

When the database has no confirmed entry for a board, `max_rpm` is
`MSI_RPM_UNKNOWN` (zero) and the hwmon layer returns 0 RPM.  The fan label
is annotated with `" (unvalidated)"` to make the situation clear:

```
fan1_label:  cpu_fan (unvalidated)
fan1_input:  0
```

This is intentional — reporting a fabricated RPM would be worse than
reporting zero.

### Temperature conversion

The EC reports temperature as raw degrees Celsius (integer bytes). The hwmon
ABI requires millidegrees:

```
hwmon_millideg = ec_celsius × 1000
```

---

## `sensors` Output

After running `sensors-detect` once (or with `sensors -u` for raw values):

```
$ sensors
msi_ec-isa-0000
Adapter: ISA adapter
cpu_fan:  2688 RPM
gpu_fan:     0 RPM
cpu_temp: +61.0°C
gpu_temp: +44.0°C
```

The GPU fan showing 0 RPM is not an error — many MSI laptops implement a
fan-stop feature where the GPU fan is held off below a threshold temperature
(typically 50–55°C).  The EC's own fan curve table defines the threshold; the
driver faithfully reports whatever the EC commands.

---

## Verifying the hwmon Device

```bash
# Find the msi_ec hwmon device
for d in /sys/class/hwmon/hwmon*; do
    echo "$d: $(cat $d/name)"
done

# Read all channels directly
HWMON=$(grep -rl "^msi_ec$" /sys/class/hwmon/*/name | head -1 | xargs dirname)
cat $HWMON/fan1_input    # CPU RPM
cat $HWMON/fan2_input    # GPU RPM
cat $HWMON/temp1_input   # CPU millideg
cat $HWMON/temp2_input   # GPU millideg
cat $HWMON/fan1_label
cat $HWMON/fan2_label
```

---

## Relationship to the Platform Interface

Both interfaces read from the same EC registers at the same addresses.  They
are not cached — each sysfs read triggers a fresh EC register read.  Reading
`fan1_input` and `realtime_fan_speed` within milliseconds of each other will
return values that may differ by at most one EC poll cycle (typically ≤1%).

The consistency checker (`tools/msi-fan-check.sh`) reads both
simultaneously and validates that they agree.

---

## Architecture

```
EC hardware
    │
    │  ACPI EC interface (kernel)
    ▼
msi-ec platform driver  (msi-ec.c)
    │
    ├── /sys/devices/platform/msi-ec/   (platform sysfs — unchanged)
    │       cpu/realtime_fan_speed      raw %
    │       cpu/realtime_temperature    raw °C
    │       gpu/realtime_fan_speed      raw %
    │       gpu/realtime_temperature    raw °C
    │       fan_mode                    string
    │       cooler_boost                on/off
    │       ...
    │
    └── msi_hwmon_register()            called from msi_ec_init()
            │
            │  devm_hwmon_device_register_with_info()
            ▼
        hwmon device  (msi_ec-isa-0000)
            │
            ├── /sys/class/hwmon/hwmonN/fan1_input    RPM
            ├── /sys/class/hwmon/hwmonN/fan2_input    RPM
            ├── /sys/class/hwmon/hwmonN/temp1_input   millideg
            └── /sys/class/hwmon/hwmonN/temp2_input   millideg
                    │
                    ▼
             sensors(1), fancontrol, psensor,
             Prometheus node_exporter, ...
```

---

## Implementation Notes for Contributors

### Why `devm_hwmon_device_register_with_info`?

Using the `devm_*` variant ties the hwmon device lifetime to the platform
device.  When the module is unloaded, `devres` automatically calls
`hwmon_device_unregister()`.  No explicit cleanup path in `msi_ec_exit()` is
needed.

### Why pass `struct device *parent` to `msi_hwmon_register`?

`msi_platform_device` is declared further down in `msi-ec.c` than the hwmon
registration function.  Passing the parent device as a parameter avoids a
forward-reference compile error without restructuring the file.

### Why is hwmon failure non-fatal?

If the fan spec database has no entry for a board, `msi_resolve_fan_limits()`
returns `MSI_RPM_UNKNOWN` for both fans.  The hwmon device still registers
successfully and exposes all channels — they just return 0 RPM with
`(unvalidated)` labels.  This is preferable to refusing to load, since all
other driver functionality (battery control, fan mode, cooler boost, etc.)
is unaffected by the absence of RPM calibration data.

### Adding hwmon support for a new board

No hwmon-specific work is needed to support a new board.  The hwmon layer
reads board configuration (EC addresses) from the existing `msi_ec_conf`
struct, and fan max RPM from `msi_fan_specs.h`.  To get RPM reporting working
for a new board:

1. Confirm the CPU and GPU fan percentage addresses (see
   `docs/device_support_guide.md` for the general procedure, or
   `docs/fan_spec_database.md` for the RPM-specific path).
2. Add a `msi_fan_specs.h` entry for the board's board ID with the confirmed
   max RPM values.
3. Set `source_quality` to the appropriate tier based on how the RPM was
   obtained.

That's it.  The hwmon layer picks up the new entry automatically at driver
load time.
