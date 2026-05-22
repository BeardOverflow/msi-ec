# Fan Specification Database (`msi_fan_specs.h`)

This document describes the fan specification database introduced alongside
the hwmon integration, how to look up data for a new board, how to add or
improve entries, and the data quality tiers.

---

## Why this database exists

The Linux hwmon ABI reports fan speed in RPM.  The MSI EC reports fan speed
as a percentage (0–100).  Converting between them requires the maximum RPM
of the specific fan hardware fitted to each board model.

MSI uses different fan assemblies across product lines — a Stealth 16 fan
at 5400 RPM, a Modern 15 fan at 4600 RPM, a Katana 15 fan at 4200–4300 RPM,
and so on.  There is no programmatic way to query this from the EC; it is a
physical property of the fan hardware.  This database records it.

---

## Database location and structure

File: `msi_fan_specs.h`

The database is a C array of `struct msi_fan_entry`:

```c
struct msi_fan_entry {
    const char *fw_version;   /* NULL = match any fw on this board */
    const char *board_id;     /* 4-char DMI board name prefix, e.g. "16J9" */
    const char *model_str;    /* human-readable name for the entry */
    int cpu_max_rpm;          /* MSI_RPM_UNKNOWN (0) if not yet confirmed */
    int gpu_max_rpm;          /* MSI_RPM_UNKNOWN (0) if not yet confirmed */
    const char *cpu_part_no;  /* MSI spare parts catalog number, or NULL */
    const char *gpu_part_no;  /* MSI spare parts catalog number, or NULL */
    enum msi_fan_source_quality source_quality;
    const char *source_url;
    /* source comment follows as a C block comment */
};
```

The resolver tries matches in this priority order:

1. Exact firmware version string match (`fw_version` field)
2. Board ID match (`board_id` field, compared against `dmi_get_system_info(DMI_BOARD_NAME)`)
3. Model string match (`model_str` substring in DMI product name)
4. Family prefix match (first two characters of board ID)
5. Fallback: `MSI_RPM_UNKNOWN` — caller must handle zero RPM gracefully

---

## Data quality tiers

| Tier constant | Meaning |
|---|---|
| `MSI_SRC_SPAREPARTS_OFFICIAL` | RPM confirmed from the MSI spare parts catalog with part number and spec table |
| `MSI_SRC_MEASURED_EMPIRICAL` | Same physical fan assembly confirmed for an adjacent model in the same chassis generation |
| `MSI_SRC_VENDOR_THIRDPARTY` | Confirmed from an authoritative third-party source (OEM parts supplier, official spec sheet) |
| `MSI_SRC_COMMUNITY_REPORT` | Class-appropriate estimate; awaits official verification |

The hwmon layer displays `" (unvalidated)"` on the fan label for any entry
at `MSI_SRC_COMMUNITY_REPORT` — this tells the user that the RPM figure is
an estimate.

---

## How to find the official max RPM for a board

### Method 1 — MSI spare parts catalog (preferred)

MSI publishes full spec sheets for every laptop fan assembly on their spare
parts websites:

- EU: https://eu-spareparts.msi.com
- US: https://us-spareparts.msi.com

**Step 1**: Find your fan part number.

Open your laptop, locate the fan assembly label, and note the part number
(format: `E33-XXXXXXX-XXX` or `E32-XXXXXXX-XXX`).  Alternatively, search
the spare parts site by laptop model name.

**Step 2**: Look up the part.

Navigate to the part page.  The spec table will include a line like:

```
RPM  4350RPM
```

or sometimes

```
START/RATED VOLTAGE  2.5V/5V
RPM  5400/4700RPM   ← start / rated; use the start (higher) value as max_rpm
```

**Step 3**: Note the spec and add the entry.

Record the part number, RPM, and the URL.  Use `MSI_SRC_SPAREPARTS_OFFICIAL`
as the quality tier.

### Method 2 — Back-calculation from a live system

If the board is already supported by the driver and you have it running:

```bash
# Read the platform fan percentage and hwmon RPM simultaneously
PLAT=/sys/devices/platform/msi-ec
HWMON=$(grep -rl "^msi_ec$" /sys/class/hwmon/*/name | head -1 | xargs dirname)

echo "Platform CPU fan %: $(cat $PLAT/cpu/realtime_fan_speed)"
echo "hwmon CPU fan RPM:  $(cat $HWMON/fan1_input)"
```

With the fan running (>0%), back-calculate:

```
max_rpm = rpm × 100 / pct
```

This method is only valid when `fan1_label` does **not** contain
`(unvalidated)`, because the RPM was computed using an already-estimated
max_rpm.  If the label is clean (e.g. just `"cpu_fan"`), the board already
has an entry and you can verify the derived max_rpm against the spec.

The consistency checker (`tools/msi-fan-check.sh`) displays the derived
max_rpm in its output table.

### Method 3 — EC dump under cooler boost

Enable cooler boost, take an EC dump, and find the maximum fan percentage
reported.  This gives you the EC's commanded maximum, not the physical
hardware maximum.  Use only as a cross-check, not as the primary source.

---

## Adding or improving an entry

### Improving an existing `MSI_SRC_COMMUNITY_REPORT` entry

Every community-report entry has a `TODO:` comment pointing to the relevant
MSI spare parts search:

```c
/* TODO: Find official RPM spec on us/eu-spareparts.msi.com for MS-1541.
 * Search for part E33-0800930-MC2 or fan assembly for GE66 Raider 10SF. */
```

1. Find the official spec using Method 1 above.
2. Update `cpu_max_rpm` and `gpu_max_rpm`.
3. Set `cpu_part_no` (and `gpu_part_no` if different).
4. Change `source_quality` to `MSI_SRC_SPAREPARTS_OFFICIAL`.
5. Update `source_url` to the direct spare parts page URL.
6. Replace the `TODO:` comment with a spec citation comment.

### Adding a new board entry

Find the board ID:

```bash
cat /sys/class/dmi/id/board_name    # e.g. "16J9"
cat /sys/class/dmi/id/product_name  # e.g. "GV62 7RD"
```

Add an entry to the array (before the sentinel):

```c
{
    NULL, "XXXX", "Model Name Here",
    cpu_max_rpm, gpu_max_rpm,
    "E33-XXXXXXX-XXX",   /* cpu part number, or NULL */
    NULL,                /* gpu part number if different, or NULL */
    MSI_SRC_SPAREPARTS_OFFICIAL,  /* or appropriate tier */
    "https://eu-spareparts.msi.com/products/..."
    /* Model Name (MS-XXXX). Official EU spareparts:
     * <dimensions>, <voltage>, <current>, <RPM> RPM. */
},
```

If the CPU and GPU fans are the **same physical part**:

```c
cpu_max_rpm = 4350,
gpu_max_rpm = 4350,
cpu_part_no = "E33-0800790-MC2",
gpu_part_no = NULL,   /* same part, listed once */
```

If the CPU and GPU fans are **different** (e.g. Katana GF66):

```c
cpu_max_rpm = 4350,
gpu_max_rpm = 4200,
cpu_part_no = "E32-2500871-HH7",
gpu_part_no = "E32-2500871-HH7",  /* same cooler assembly, but asymmetric RPM */
```

---

## Current coverage

The database covers all **135 board IDs** present in the driver across the
following product families:

| Family | Boards | Notable official data |
|---|---|---|
| GF75 Thin | 17F1–17F5, 17FK | E33-0800790-MC2 → 4350 RPM |
| GF65 Thin / Creator 15M | 16W1, 16W2 | E33-0401680-AE0 → 5400 RPM |
| GS65 / P65 Creator | 16Q2–16Q4 | E33-0401290-AE0 → 4800 RPM |
| Stealth 15 | 16V1, 16V3–16V6, 15F2–15F5 | E33-0402350-AE0 → 4800 RPM |
| Stealth 16 / GS76 / GS77 | 17M1, 17M2, 17P1, 17P2 | E33-0801020-AE0 → 4850 RPM |
| GE76 / GE77 Raider | 17K1–17K5 | E33-0801580-B22 → 5000 RPM |
| Alpha / Bravo 15/17 B5 | 158K, 158L, 158M, 17LL | E33-0800980-MC2 → 4350 RPM |
| Alpha 17 C7V | 17KK | E33-0800970-MC2 → 4800 RPM |
| Katana GF66 | 1581, 1582 | E32-2500871-HH7 → 4350/4200 RPM |
| Katana 15 | 1585, 1587 | E33-0801180-MC2 → 4300 RPM |
| Modern 15 | 1551, 1552, 155L, 15HK | E33-0401550-AE0 → 4600 RPM |
| Modern 14 | 14D1–14D3, 14DK, 14DL, 14JK, 14L1 | E33-0800890-AE0 → 5300 RPM |
| Prestige 14 | 14C1, 14C4, 14C6, 14N1, 14N2 | E33-0800890-AE0 class |
| Prestige 15 | 16S3, 16S6, 16S8 | 4700 RPM class |
| Prestige 16 | 1592, 15A1, 15A3 | 5550 RPM class |
| Summit E13 Flip | 13P2, 13P3, 13P5 | 6800 RPM class |
| Stealth 14 Studio | 14K1, 14K2 | E32-2501940-A87 → 5700 RPM |
| Cyborg 15 | 15K1, 15K2 | E32-2501462-F05 → 4350 RPM |
| GF63 Thin | 16R1, 16R3–16R6 | 4350 RPM class |
| GE66 / GP66 / Vector | 1541–1544 | Parts known, RPM pending official confirmation |
| GS75 Stealth | 17G1, 17G3 | Parts known, RPM pending official confirmation |
| Titan 18 HX | 1822, 1824 | 5500 RPM class |
| GV62 7RD | 16J9 | 4800 RPM (empirically back-calculated, confirmed twice) |

---

## Entries still needing official confirmation

The following boards have `MSI_SRC_COMMUNITY_REPORT` entries with known fan
part numbers but unconfirmed RPM specs.  Help with these is especially
valuable:

| Board | Model | Fan part(s) | Where to look |
|---|---|---|---|
| 1541 | GE66 Raider 10SF/11UH | E33-0800930-MC2, E33-0401690-MC2 | eu/us-spareparts.msi.com |
| 1542 | GP66 Leopard 10UG/11UG | Same as 1541 | eu/us-spareparts.msi.com |
| 17G1 | GS75 Stealth 8SF/9SE | BS5005HS-U3I, BS5005HS-U3J | eu/us-spareparts.msi.com |
| 17G3 | GS75 Stealth 10SF/10SGS | Same as 17G1 | eu/us-spareparts.msi.com |
| 15F2 | Stealth 16 Studio A13VG | Unknown | eu/us-spareparts.msi.com |

To contribute: find the RPM on the spare parts catalog page for the listed
part number, then open a pull request updating the entry's `cpu_max_rpm`,
`source_quality`, and `source_url` fields.
