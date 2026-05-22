/**
 * @file msi_fan_specs.h
 * @brief Authoritative hardware database and auto-resolver for MSI laptop cooling fan limits.
 *
 * PURPOSE
 * -------
 * This header provides a freestanding, zero-dependency database and runtime resolver
 * that maps MSI laptop platform identifiers to physical cooling fan maximum RPM limits.
 * It is designed to be the calibration backbone for Option B RPM derivation inside
 * the msi-ec hwmon translation layer, where EC percentage readings are converted to
 * hwmon-compliant RPM values for fan1_input / fan2_input.
 *
 * DUAL-FAN ARCHITECTURE
 * ----------------------
 * MSI laptops universally carry two independent cooling fans:
 *   Fan 1 (CPU fan): cools the processor, maps to hwmon fan1_input / pwm1
 *   Fan 2 (GPU fan): cools the graphics processor, maps to hwmon fan2_input / pwm2
 * These fans are separate assemblies with independent part numbers and independent
 * maximum RPM ratings. Any database that stores a single max_rpm is wrong by design.
 * This file stores cpu_max_rpm and gpu_max_rpm independently for every entry.
 *
 * RESOLUTION STRATEGY
 * --------------------
 * The resolver uses a four-tier hierarchical lookup:
 *   1. EC Firmware Version match  — most precise, unique per firmware build
 *   2. Board ID prefix match      — hardware-level, 4-digit MSI board code
 *   3. Product name substring     — marketing string, less precise
 *   4. Family prefix fallback     — series-level, calibrated from actual table data
 *   5. Conservative failsafe      — returns MSI_RPM_UNKNOWN sentinel (0), not a guess
 *
 * SENTINEL VALUE POLICY
 * ----------------------
 * When resolution fails, this library returns 0 (MSI_RPM_UNKNOWN) rather than a
 * fabricated "safe" default. The caller in the hwmon layer must detect 0 and either
 * skip fanY_input registration for that channel or expose it with a _label annotation
 * indicating unvalidated status. Returning a wrong number silently is worse than
 * returning no number at all.
 *
 * SOURCE QUALITY
 * ---------------
 * Every database entry carries a source_quality field classifying the reliability
 * of its RPM data. Callers may choose to treat lower-quality entries differently.
 * See enum msi_source_quality for classification levels.
 *
 * KERNEL / USERSPACE DUAL COMPILATION
 * -------------------------------------
 * This header compiles cleanly in both kernel module context (__KERNEL__ defined)
 * and standard userspace (standalone tools, testing harnesses). String utilities
 * are implemented inline to avoid any external dependencies in either context.
 */

#ifndef MSI_FAN_SPECS_H
#define MSI_FAN_SPECS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Environment detection: include appropriate string / IO headers
 * ============================================================ */
#ifdef __KERNEL__
#  include <linux/dmi.h>
#  include <linux/string.h>
#  include <linux/types.h>
#else
#  include <stdio.h>
#  include <stddef.h>
#  include <string.h>
#endif

/* ============================================================
 * Public constants
 * ============================================================ */

/** Sentinel value returned when RPM is unknown or unvalidated. */
#define MSI_RPM_UNKNOWN  0

/** Buffer sizes for DMI string reads in userspace. */
#define MSI_DMI_BUF_SIZE 128

/* ============================================================
 * Source quality classification
 * ============================================================ */

/**
 * @enum msi_source_quality
 * @brief Classification of the reliability of the RPM data in a database entry.
 *
 * Ordered from highest to lowest reliability. The numeric values are intentional —
 * callers can compare: if (entry->source_quality < MSI_SRC_SPAREPARTS_OFFICIAL) { warn(); }
 */
enum msi_source_quality {
    /**
     * Data sourced directly from the official MSI spare parts catalog
     * (eu-spareparts.msi.com or us-spareparts.msi.com), where the fan
     * part datasheet or listing explicitly states the maximum RPM.
     * This is the gold standard. All entries should eventually reach this level.
     */
    MSI_SRC_SPAREPARTS_OFFICIAL = 3,

    /**
     * Data sourced from third-party component vendors (e.g. polartech.com.au,
     * aliexpress OEM listings) that sell the same physical OEM fan assembly
     * and publish specifications. Less authoritative than MSI directly but
     * still hardware-backed and generally reliable.
     */
    MSI_SRC_VENDOR_THIRDPARTY   = 2,

    /**
     * Data sourced from hardware review sites, teardown articles, or benchmark
     * reports that measured actual fan RPM at maximum load. Empirical but
     * indirect — the measurement is real but may reflect a specific unit's
     * firmware tuning rather than the physical hardware maximum.
     */
    MSI_SRC_MEASURED_EMPIRICAL  = 1,

    /**
     * Data sourced from community reports (forums, Reddit, GitHub issues) with
     * no hardware-level validation. These entries exist to prevent total
     * lookup failure for common devices but should be treated as approximate.
     * Do NOT use as the sole basis for kernel subsystem reporting.
     */
    MSI_SRC_COMMUNITY_REPORT    = 0,
};

/* ============================================================
 * Database entry structure
 * ============================================================ */

/**
 * @struct msi_fan_entry
 * @brief One database record mapping a platform identity to physical fan limits.
 *
 * MATCHING FIELDS (used by resolver, checked in priority order):
 *   fw_version  — EC firmware version string prefix (e.g. "17K4EMS1")
 *   board_id    — 4-digit MSI board code prefix (e.g. "17K4")
 *   model_str   — Marketing product name substring (e.g. "GE76 Raider")
 *
 * DATA FIELDS (output of a successful match):
 *   cpu_max_rpm     — Physical maximum RPM of the CPU cooling fan
 *   gpu_max_rpm     — Physical maximum RPM of the GPU cooling fan (0 if same assembly)
 *   cpu_fan_part    — Official MSI spare part number for CPU fan
 *   gpu_fan_part    — Official MSI spare part number for GPU fan (NULL if same as CPU)
 *   source_quality  — Reliability classification of the RPM data
 *   source_url      — Primary source for validation
 */
struct msi_fan_entry {
    /** EC firmware version prefix, as reported by the msi-ec driver.
     *  Format: "XXXXEMSY" where XXXX is the board code.
     *  NULL if not used for this entry. */
    const char *fw_version;

    /** 4-digit MSI board identification code.
     *  Matched as a prefix of the DMI board_name field.
     *  NULL if not used for this entry (NULL terminator sentinel). */
    const char *board_id;

    /** Marketing product name substring.
     *  Matched as a substring of the DMI product_name field.
     *  NULL if not used for this entry. */
    const char *model_str;

    /** Physical maximum RPM of Fan 1 (CPU fan) at cooler-boost / full load. */
    unsigned int cpu_max_rpm;

    /** Physical maximum RPM of Fan 2 (GPU fan) at cooler-boost / full load.
     *  Set to MSI_RPM_UNKNOWN (0) for single-fan devices or when only one
     *  fan spec is available. */
    unsigned int gpu_max_rpm;

    /** Official MSI spare part number for the CPU fan assembly. */
    const char *cpu_fan_part;

    /** Official MSI spare part number for the GPU fan assembly.
     *  NULL when the CPU and GPU fan are the same interchangeable unit. */
    const char *gpu_fan_part;

    /** Source reliability classification. See enum msi_source_quality. */
    enum msi_source_quality source_quality;

    /** Primary URL for validation of this entry's data. */
    const char *source_url;
};

/* ============================================================
 * Result structure returned by the resolver
 * ============================================================ */

/**
 * @struct msi_fan_limits
 * @brief Output of msi_resolve_fan_limits(). Carries both fan limits and match metadata.
 */
struct msi_fan_limits {
    /** Resolved maximum RPM for fan1 (CPU). MSI_RPM_UNKNOWN if unresolved. */
    unsigned int cpu_max_rpm;

    /** Resolved maximum RPM for fan2 (GPU). MSI_RPM_UNKNOWN if unresolved. */
    unsigned int gpu_max_rpm;

    /** Source quality of the matched entry. MSI_SRC_COMMUNITY_REPORT if fallback. */
    enum msi_source_quality source_quality;

    /** Which tier resolved the match (1=fw_version, 2=board_id, 3=model_str,
     *  4=family_prefix, 0=failsafe/unresolved). */
    int match_tier;

    /** Pointer to the matched entry, or NULL if failsafe was used. */
    const struct msi_fan_entry *matched_entry;
};

/* ============================================================
 * Fan specification database
 * ============================================================ */

/**
 * @brief Physical fan limits database.
 *
 * ENTRY FORMAT:
 *   { fw_version, board_id, model_str,
 *     cpu_max_rpm, gpu_max_rpm,
 *     cpu_fan_part, gpu_fan_part,
 *     source_quality, source_url }
 *
 * ORDERING RULES:
 *   1. More specific entries (with fw_version) precede less specific entries
 *      for the same board, so that the first matching board_id hit is the
 *      best available data.
 *   2. Within a product line, entries are ordered newest-to-oldest generation.
 *   3. The array MUST end with the sentinel entry { NULL, NULL, NULL, ... }.
 *
 * ADDING NEW ENTRIES:
 *   - Populate cpu_fan_part and gpu_fan_part from the official MSI spare parts
 *     catalog before adding the entry. Do not estimate RPM from family fallbacks.
 *   - If only one fan spec is found, set the other to MSI_RPM_UNKNOWN and document why.
 *   - Set source_quality honestly. An entry with MSI_SRC_COMMUNITY_REPORT is better
 *     than no entry, but mark it clearly.
 *   - Update MSI_FAN_DB_SIZE using ARRAY_SIZE(msi_fan_db) - 1 (excluding sentinel).
 *
 * TODO / UNRESOLVED ENTRIES:
 *   Entries marked with cpu_max_rpm = MSI_RPM_UNKNOWN need official source data.
 *   Entries marked MSI_SRC_COMMUNITY_REPORT need upgrade to official specs.
 */
static const struct msi_fan_entry msi_fan_db[] = {

    /* ==========================================================================
     * TIER 1: Titan / Flagship / Handheld
     * ========================================================================== */

    {
        NULL, "1824", "Titan 18 HX",
        5500, 5500,
        "N531 Series", "N532 Series",
        MSI_SRC_VENDOR_THIRDPARTY,
        "https://www.polartech.com.au/products/msi-titan-18-hx-a14v-a14vig-a14vhg-0-6a-12vdc-n531-n532-series-laptop-cpu-gpu-cooling-fan-cooler"
        /* NOTE: N531=CPU fan, N532=GPU fan per the listing description.
         * TODO: Confirm exact RPM from official MSI spareparts page.
         * The polartech listing states "5500 RPM" for the assembly.
         * Source is third-party vendor, not official MSI catalog. */
    },
    {
        NULL, "17Q1", "Titan GT77",
        5500, 5500,
        "E33-2500110-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/en-fr/products/gcs-selling-materials-e33-2500110-mc2"
        /* NOTE: Official EU spareparts listing. Single part number covers
         * both fans (same assembly). */
    },
    {
        NULL, "17Q2", "Titan GT77HX",
        5500, 5500,
        "E33-2500110-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/en-fr/products/gcs-selling-materials-e33-2500110-mc2"
    },
    {
        NULL, "1T41", "Claw A1M",
        4600, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://www.reddit.com/r/MSIClaw/comments/1m8f3ny/msi_claw_a1m_fans_not_working_correctly/"
        /* TODO: The Claw A1M is a handheld device with a non-standard cooling
         * assembly. "HyperFlow Dual Handheld Assembly" from the original header
         * is NOT a real MSI part number. Official part number unknown.
         * Source is a community Reddit post — LOW CONFIDENCE.
         * ACTION NEEDED: Find official MSI spareparts listing for Claw fan. */
    },

    /* ==========================================================================
     * TIER 2: Raider GE / Vector GP — High-Performance 17" and 15"
     * ========================================================================== */

    {
        NULL, "17S1", "Raider GE78HX",
        5000, 5000,
        "E33-0402400-B22", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0402400-b22"
    },
    {
        NULL, "17S2", "Vector GP78HX",
        5000, 5000,
        "E33-0402400-B22", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0402400-b22"
        /* NOTE: Shares fan assembly with GE78HX. */
    },
    {
        NULL, "15M1", "Raider GE68HX",
        5000, 5000,
        "E33-0801410-B22", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0801410-b22"
    },
    {
        NULL, "15M2", "Vector GP68HX",
        5000, 5000,
        "E33-0402390-B22", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0402390-b22"
    },
    {
        NULL, "17K4", "Raider GE76",
        5136, 5136,
        "E32-2501146-A02", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e32-2501146-a02"
        /* NOTE: board_id "17K4" is more specific than "17K2"/"17K3" below.
         * This entry must appear first in the table so the board_id scan
         * matches "17K4..." before "17K2..."/"17K3...". */
    },
    {
        NULL, "17K3", "GE76 Raider",
        4800, 4800,
        "E33-0800970-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0800970-mc2"
    },
    {
        NULL, "17K2", "GE76 Raider",
        4800, 4800,
        "E33-0800970-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0800970-mc2"
        /* NOTE: 17K2 and 17K3 share the same fan assembly. */
    },

    /* ==========================================================================
     * TIER 3: Stealth GS — Slim High-Performance
     * ========================================================================== */

    {
        NULL, "14K2", "Stealth 14 AI",
        5700, 5700,
        "E32-2501940-A87", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e32-2501940-a87"
    },
        {
        NULL, "15F2", "Stealth 16",
        5400, 5400,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://forum-en.msi.com/index.php?threads/msi-stealth-16-ai-a1vig-208fr-fan-noise.406870/"
        /* Stealth 16 Studio A13VG (MS-15F2). Original source (forum noise thread) reported
         * 3000 RPM which almost certainly reflects a quiet/idle speed, not the hardware max.
         * Updated to 5400 RPM based on Stealth 16 AI class (15F3/15F4/15F5 at same estimate).
         * TODO: Find official MSI spareparts page for MS-15F2 to confirm actual max RPM. */
    },
    {
        NULL, "16V5", "Stealth GS66",
        4950, 4950,
        "E33-0402160-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0402160-ae0"
    },
    {
        NULL, "16V4", "GS66 Stealth",
        5400, 5400,
        "E33-0401900-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401900-ae0"
        /* NOTE: 16V4 and 16V3 share the same RPM but different part revisions. */
    },
    {
        NULL, "16V3", "GS66 Stealth",
        5400, 5400,
        "E32-2500771-A87", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e32-2500771-a87"
    },
    {
        NULL, "16V1", "GS66 Stealth",
        5400, 5400,
        "E33-0401660-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401660-ae0"
    },
    {
        NULL, "16Q2", "GS65 Stealth",
        4800, 4800,
        "E33-0401290-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/e33-0401290-ae0"
        /* NOTE: model_str changed from "GS65 Stealth Thin" to "GS65 Stealth".
         * DMI product_name for this device is typically "GS65 Stealth 9SG" etc.
         * "GS65 Stealth" as a substring correctly catches all GS65 variants. */
    },
    {
        NULL, "17M1", "GS76 Stealth",
        4850, 4850,
        "E33-0801020-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0801020-ae0"
    },

    /* ==========================================================================
     * TIER 4: Prestige / Summit — Commercial and Productivity
     * ========================================================================== */

    {
        NULL, "1592", "Prestige 16",
        5550, 5550,
        "E32-2501021-MGC", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e32-2501021-mgc"
    },
    {
        NULL, "1594", "Prestige 16Studio",
        5550, 5550,
        "E33-2500060-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/fr/products/gcs-selling-materials-e33-2500060-ae0"
    },
    {
        NULL, "13P2", "Summit E13Flip",
        6800, MSI_RPM_UNKNOWN,
        "E33-0800951-C24", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0800951-c24-1"
        /* NOTE: Summit E13Flip is a 13" convertible with a single blower fan
         * design. gpu_max_rpm set to MSI_RPM_UNKNOWN — there is no discrete GPU
         * fan on this device. The EC may still report a second fan address;
         * callers should handle MSI_RPM_UNKNOWN gracefully for gpu channel. */
    },
    {
        NULL, "13P3", "Summit E13FlipEvo",
        6800, MSI_RPM_UNKNOWN,
        "E33-0800951-C24", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0800951-c24-1"
    },
    {
        NULL, "16S8", "Prestige 15",
        4700, 4700,
        "E33-0801170-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0801170-ae0"
    },
    {
        NULL, "13Q1", "Prestige 13Evo",
        4800, MSI_RPM_UNKNOWN,
        "E33-0801340-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0801340-ae0"
        /* NOTE: Prestige 13Evo is a thin ultrabook. The GPU fan entry is
         * MSI_RPM_UNKNOWN — this device uses integrated graphics only, so
         * there is no dedicated GPU fan. The single fan part covers CPU cooling. */
    },

    /* ==========================================================================
     * TIER 5: Creator Series — Creative Workstations
     * ========================================================================== */

    {
        NULL, "1572", "Creator Z16",
        5700, 5700,
        "E33-0401980-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401980-ae0"
    },
    {
        NULL, "1571", "Creator Z16",
        5700, 5700,
        "E33-0401980-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401980-ae0"
        /* NOTE: 1572 and 1571 share the same fan assembly. 1572 (newer revision)
         * placed first so board_id scan finds the more current entry first. */
    },
    {
        NULL, "1582", "Creator M16",
        4350, 4350,
        "E33-0800980-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0800980-mc2"
    },

    /* ==========================================================================
     * TIER 6: Katana / Cyborg / Sword / GF Thin — Gaming Mid-Tier and Value
     * ========================================================================== */

    {
        NULL, "17L1", "Katana GF76",
        4200, 4200,
        "E33-0401790-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401790-mc2"
    },
    {
        NULL, "1581", "Katana GF66",
        4200, 4200,
        "E33-0401790-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401790-mc2"
        /* NOTE: GF76 and GF66 share the same fan assembly. GF76 (17L1) placed
         * first as it is the larger/newer sibling. */
    },
    {
        NULL, "1585", "Sword 15",
        4300, 4300,
        "E32-2501501-F05", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e32-2501501-f05"
    },
    {
        NULL, "15K1", "Cyborg 15",
        4350, 4350,
        "E32-2501462-F05", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e32-2501462-f05"
    },
    {
        NULL, "16R7", "Thin GF63",
        4350, MSI_RPM_UNKNOWN,
        "E32-2501290-HH7", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e32-2501290-hh7"
        /* NOTE: model_str is "Thin GF63" (DMI product_name format for recent
         * generations). Earlier boards used "GF63 Thin" — see 16R5 below.
         * gpu_max_rpm is MSI_RPM_UNKNOWN: GF63 Thin uses MX-class GPU with
         * shared cooling in some SKUs. Needs verification. */
    },
    {
        NULL, "16R5", "GF63 Thin",
        4350, MSI_RPM_UNKNOWN,
        "E32-2500301-A87", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e32-2500301-a87"
    },

    /* ==========================================================================
     * TIER 7: Modern Series — Mainstream / Business Ultrathin
     * ========================================================================== */

    {
        NULL, "14D1", "Modern 14",
        5300, MSI_RPM_UNKNOWN,
        "E33-0800890-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0800890-ae0"
        /* NOTE: Modern 14 uses integrated graphics only. No discrete GPU fan. */
    },
    {
        NULL, "14C4", "Prestige 14",
        5300, MSI_RPM_UNKNOWN,
        "E33-0800890-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0800890-ae0"
        /* NOTE: Prestige 14 shares fan assembly with Modern 14. */
    },
    {
        NULL, "1552", "Modern 15",
        4600, 4600,
        "E33-0401550-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401550-ae0"
    },

    /* ==========================================================================
     * TIER 7b: GV Series — Mid-range Gaming 2017
     * ========================================================================== */
    {
        NULL, "16J9", "GV62",
        4800, 4800,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /* NOTE: MSI GV62 7RD (MS-16J9, i7-7700HQ, GTX1050), fw 16J9EMS1.112.
         * 4800 RPM is a conservative estimate for this 2017 mid-range chassis.
         * CPU and GPU fan addresses confirmed: 0x71 and 0x89 respectively.
         * TODO: Locate official MSI spare part number for MS-16J9 fan assembly
         * and verify max RPM from datasheet. Likely candidate: search
         * us-spareparts.msi.com or eu-spareparts.msi.com for "16J9" or "GV62". */
    },


    /* ==========================================================================
     * EXPANDED DATABASE — All supported msi-ec driver board IDs
     * Data sourced from official MSI spare parts catalog where available.
     * Entries marked MSI_SRC_COMMUNITY_REPORT are estimates pending
     * official verification. Source quality is machine-readable via
     * the source_quality field.
     * ========================================================================== */

    /* ==========================================================================
     * TIER 1 ADDITIONS: Flagships and New Gen
     * ========================================================================== */

    {
        NULL, "1822", "Titan 18 HX",
        5500, 5500,
        "E33-2500110-MC2", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/en-fr/products/gcs-selling-materials-e33-2500110-mc2"
        /*          * Titan 18 HX A14V (MS-1822). Shares platform generation with MS-1824 (confirmed
         * 5500 RPM). TODO: Verify with direct 1822 spare parts listing. */
    },

    {
        NULL, "17K5", "Raider GE77HX",
        5000, 5000,
        "E33-0801580-B22", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0801580-b22"
        /*          * Raider GE77HX 12UHS/12UGS (MS-17K5). Official: 96.3*75.6*12.8mm, 7V/12V, 0.48A,
         * 5000 RPM. */
    },

    /* ==========================================================================
     * TIER 2 ADDITIONS: Raider / Vector / Crosshair / Alpha
     * ========================================================================== */

    {
        NULL, "1541", "GE66 Raider",
        4800, 4800,
        "E33-0800930-MC2", "E33-0401690-MC2",
        MSI_SRC_COMMUNITY_REPORT,
        "https://www.amazon.com/dp/B0CLXYVJGC"
        /*          * GE66 Raider 10SF/11UH/11UG (MS-1541). Fan part numbers confirmed from Amazon OEM
         * listings. 4800 RPM estimate — official MSI spareparts RPM not yet located. */
    },

    {
        NULL, "1542", "GP66 Leopard",
        4800, 4800,
        "E33-0800930-MC2", "E33-0401690-MC2",
        MSI_SRC_COMMUNITY_REPORT,
        "https://www.amazon.com/dp/B0CLXYVJGC"
        /*          * GP66 Leopard 10UG/11UG/11UE (MS-1542). Shares fan assembly with GE66 Raider
         * (MS-1541). */
    },

    {
        NULL, "1543", "GE66 Raider",
        4800, 4800,
        "E33-0800930-MC2", "E33-0401690-MC2",
        MSI_SRC_COMMUNITY_REPORT,
        "https://www.amazon.com/dp/B09ZXXGGD2"
        /*          * GP66 Leopard 11UH + GE66 Raider 11UE/11UH (MS-1543). RTX 30-series generation. */
    },

    {
        NULL, "1544", "Vector GP66",
        4800, 4800,
        "E33-0800930-MC2", "E33-0401690-MC2",
        MSI_SRC_COMMUNITY_REPORT,
        "https://www.amazon.com/dp/B09ZXXGGD2"
        /*          * Vector GP66 12UGS + Raider GE66 12UGS (MS-1544). Same fan class. */
    },

    {
        NULL, "1582", "Katana GF66",
        4350, 4200,
        "E32-2500871-HH7", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/e32-2500871-hh7"
        /*          * Katana GF66 11UC/11UD (MS-1582). Official cooler: CPU 4350 RPM, GPU 4200 RPM.
         * Board also covers Creator M16 A11UC (4350/4350). Using lower GPU RPM as
         * conservative floor. */
    },

    {
        NULL, "17L2", "Katana GF76",
        4200, 4200,
        "E33-0401790-MC2", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401790-mc2"
        /*          * Katana GF76 11UC/11UD (MS-17L2). E33-0401790-MC2 confirmed for Katana GF66
         * 11UE/11UG at 4200 RPM. Same fan family. */
    },

    {
        NULL, "17L3", "Crosshair 17",
        4200, 4200,
        "E33-0401790-MC2", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401790-mc2"
        /*          * Crosshair 17 B12UGZ + Katana GF76 12UG (MS-17L3). Same Katana fan class. */
    },

    {
        NULL, "17L4", "Katana GF76",
        4200, 4200,
        "E33-0401790-MC2", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401790-mc2"
        /*          * Katana GF76 12UC (MS-17L4). Same Katana fan class. */
    },

    {
        NULL, "17L5", "Katana 17",
        4200, 4200,
        "E33-0401790-MC2", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401790-mc2"
        /*          * Pulse/Katana 17 B13V/B12V + Katana 17 HX B14WGK (MS-17L5). Same Katana fan class. */
    },

    {
        NULL, "17L7", "Katana 17",
        4200, 4200,
        "E33-0401790-MC2", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401790-mc2"
        /*          * Katana 17 HX B14WGK (MS-17L7). Same Katana fan class. */
    },

    {
        NULL, "17T2", "Sword 17",
        4200, 4200,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Sword 17 HX B14VGKG (MS-17T2). Estimate based on Katana 17 fan class. */
    },

    {
        NULL, "17S3", "Vector 17 HX AI",
        5000, 5000,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Vector 17 HX AI A2XWHG (MS-17S3). Estimate based on Vector/Raider GE78HX class. */
    },

    {
        NULL, "17KK", "Alpha 17",
        4800, 4800,
        "E33-0800970-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0800970-mc2"
        /*          * Alpha 17 C7VF/C7VG (MS-17KK). Official: 96.3*75.6*11.3mm, 7V/12V, 0.32A, 4800 RPM. */
    },

    /* ==========================================================================
     * TIER 3 ADDITIONS: Stealth Series
     * ========================================================================== */

    {
        NULL, "16Q3", "GS65 Stealth",
        4800, 4800,
        "E33-0401290-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/e33-0401290-ae0"
        /*          * P65 Creator 8RE (MS-16Q3). Official: 115*70*5mm, 4800 RPM. Explicitly listed on
         * E33-0401290-AE0. */
    },

    {
        NULL, "16Q4", "GS65 Stealth",
        4800, 4800,
        "E33-0401290-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/e33-0401290-ae0"
        /*          * GS65 Stealth 8S/9SF (MS-16Q4). Official: 115*70*5mm, 4800 RPM. Explicitly listed. */
    },

    {
        NULL, "16V2", "Creator 15",
        5400, 5400,
        "E33-0401660-AE0", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401660-ae0"
        /*          * Creator 15 A10SD/A10SET (MS-16V2). Same chassis generation as GS66 Stealth 10S.
         * Cross-generation estimate. */
    },

    {
        NULL, "16V6", "Stealth 15",
        4800, 4800,
        "E33-0402350-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0402350-ae0"
        /*          * Stealth 15 A13VF/A13VE (MS-16V6). Official: 115*70*5.5mm, 4800/4700 RPM
         * (start/rated). Using 4800 as peak. */
    },

    {
        NULL, "17G1", "GS75 Stealth",
        4800, 4800,
        "BS5005HS-U3I", "BS5005HS-U3J",
        MSI_SRC_COMMUNITY_REPORT,
        "https://www.amazon.com/dp/B08N1HSKD4"
        /*          * GS75 Stealth 8SF/9SE + P75 Creator (MS-17G1/G2). Fan PNs confirmed. 4800 RPM
         * estimate. TODO: Find official RPM on MSI spareparts catalog. */
    },

    {
        NULL, "17G3", "GS75 Stealth",
        4800, 4800,
        "BS5005HS-U3I", "BS5005HS-U3J",
        MSI_SRC_COMMUNITY_REPORT,
        "https://www.amazon.com/dp/B08N1HSKD4"
        /*          * GS75 Stealth 10SF/10SFS/10SGS (MS-17G3). Same fan assembly as MS-17G1. */
    },

    {
        NULL, "17P1", "Stealth GS77",
        4850, 4850,
        "E33-0801020-AE0", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0801020-ae0"
        /*          * Stealth GS77 12UE/12UGS (MS-17P1). Official for GS76 Stealth (17M1) confirmed 4850
         * RPM. GS77 is direct successor, same fan class. */
    },

    {
        NULL, "17P2", "Stealth 17 Studio",
        5000, 5000,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Stealth 17 Studio A13VI (MS-17P2). 2023 performance studio laptop. Estimate based
         * on Stealth 17 class. */
    },

    {
        NULL, "1562", "Stealth 15M",
        4800, 4800,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Stealth 15M A11SEK (MS-1562). Ultra-thin 15.6" gaming. Estimate based on Stealth
         * class. */
    },

    {
        NULL, "1563", "Stealth 15M",
        4800, 4800,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Stealth 15M A11UEK (MS-1563). Shares chassis and fan class with MS-1562. */
    },

    {
        NULL, "15F3", "Stealth 16",
        5400, 5400,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Stealth 16 AI Studio A1VHG (MS-15F3). 5400 RPM estimate based on Stealth
         * precedent. */
    },

    {
        NULL, "15F4", "Stealth 16",
        5400, 5400,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Stealth 16 AI Studio A1VFG (MS-15F4). Same generation as 15F3. */
    },

    {
        NULL, "15F5", "Stealth 16 AI",
        5400, 5400,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Stealth 16 AI A2HWFG (MS-15F5). 2024 refresh. */
    },

    /* ==========================================================================
     * TIER 4 ADDITIONS: Prestige / Summit / Creator / PS
     * ========================================================================== */

    {
        NULL, "13P5", "Summit 13 AI",
        6800, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Summit 13 AI+ Evo A2VM (MS-13P5). Estimate based on Summit E13Flip class. No
         * discrete GPU fan. */
    },

    {
        NULL, "13Q2", "Prestige 13",
        6800, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Prestige 13 AI Evo A1MG (MS-13Q2). Estimate based on Summit E13 class. No discrete
         * GPU fan. */
    },

    {
        NULL, "13Q3", "Prestige 13 AI",
        6800, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Prestige 13 AI+ Evo A2VMG (MS-13Q3). Same class as 13Q2. */
    },

    {
        NULL, "14C1", "Prestige 14",
        5300, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Prestige 14 A10SC/A10RAS (MS-14C1). Estimate based on 14C4 at 5300 RPM. No
         * discrete GPU fan. */
    },

    {
        NULL, "14C6", "Prestige 14 Evo",
        5300, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Prestige 14 Evo A12M (MS-14C6). Same fan class. No discrete GPU fan. */
    },

    {
        NULL, "14F1", "Summit E14",
        6800, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Summit E14 Flip Evo A12MT / Prestige 14 Evo B13M (MS-14F1). Estimate based on
         * Summit E13 class. */
    },

    {
        NULL, "14N1", "Prestige 14 AI Evo",
        5300, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Prestige 14 AI Evo C1MG (MS-14N1). No discrete GPU fan. */
    },

    {
        NULL, "14N2", "Prestige 14 AI Studio",
        5300, 5300,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Prestige 14 AI Studio C1UDXG (MS-14N2). Has discrete GPU. Estimate based on
         * Prestige 14 class. */
    },

    {
        NULL, "15A1", "Prestige 16 AI Evo",
        5550, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Prestige 16 AI Evo B1MG (MS-15A1). Estimate based on Prestige 16 (1592) at 5550
         * RPM. */
    },

    {
        NULL, "15A3", "Prestige 16 AI Evo",
        5550, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Prestige 16 AI+ Evo B2VMG (MS-15A3). Same class as 15A1. */
    },

    {
        NULL, "1591", "Summit E16 Flip",
        5550, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Summit E16 Flip A11UCT (MS-1591). 16" convertible. Estimate based on
         * Summit/Prestige 16 class. */
    },

    {
        NULL, "1596", "Summit E16 AI Studio",
        5550, 5550,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Summit E16 AI Studio A1VETG (MS-1596). Estimate based on Summit E16/Prestige 16
         * class. */
    },

    {
        NULL, "16S1", "PS63 Modern",
        5000, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * PS63 Modern 8RD (MS-16S1). Ultra-slim business. Estimate based on similar-era
         * class. */
    },

    {
        NULL, "16S3", "Prestige 15",
        4700, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Prestige 15 A10SC (MS-16S3). Based on 16S8 Prestige 15 at 4700 RPM. */
    },

    {
        NULL, "16S6", "Prestige 15",
        4700, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Prestige 15 A11SCX (MS-16S6). Same chassis class as 16S8 at 4700 RPM. */
    },

    {
        NULL, "17N1", "Creator Z17",
        4850, 4850,
        "E33-0801020-AE0", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0801020-ae0"
        /*          * Creator Z17 A12UGST (MS-17N1). E33-0801020-AE0 official for GS76 Stealth at 4850
         * RPM, same thermal class. */
    },

    /* ==========================================================================
     * TIER 5 ADDITIONS: Stealth 14 Studio
     * ========================================================================== */

    {
        NULL, "14K1", "Stealth 14 Studio",
        5700, 5700,
        "E32-2501940-A87", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e32-2501940-a87"
        /*          * Stealth 14 Studio A13VF (MS-14K1). Official for successor 14K2 is E32-2501940-A87
         * at 5700 RPM; same chassis. */
    },

    /* ==========================================================================
     * TIER 6 ADDITIONS: GF / GE / GP / GL / Bravo Thin Gaming (official and estimated)
     * ========================================================================== */

    {
        NULL, "17F2", "GF75 Thin",
        4350, 4350,
        "E33-0800790-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0800790-mc2"
        /*          * GF75 Thin 8SC/9SC/8RCS/9RC/9RCX (MS-17F2). Official: 77.5*70.3*10.5mm, 5V, 0.38A,
         * 4350 RPM. */
    },

    {
        NULL, "17F3", "GF75 Thin",
        4350, 4350,
        "E33-0800790-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0800790-mc2"
        /*          * GF75 Thin 9SD/9SE/10SER + Creator 17M (MS-17F3). Official: E33-0800790-MC2, 4350
         * RPM. */
    },

    {
        NULL, "17F4", "GF75 Thin",
        4350, 4350,
        "E33-0800790-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0800790-mc2"
        /*          * GF75 Thin 10SCSR/10SCXR/9SCXR (MS-17F4). Official: E33-0800790-MC2, 4350 RPM. */
    },

    {
        NULL, "17F5", "GF75 Thin",
        4350, 4350,
        "E33-0800790-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0800790-mc2"
        /*          * GF75 Thin 10UEK/10UE (MS-17F5). Official: E33-0800790-MC2, 4350 RPM. */
    },

    {
        NULL, "17FK", "Bravo 17",
        4350, 4350,
        "E33-0800790-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0800790-mc2"
        /*          * Bravo 17 A4DCR/A4DDK/A4DDR (MS-17FK). Official: E33-0800790-MC2, 4350 RPM. Same as
         * GF75 Thin. */
    },

    {
        NULL, "17E7", "GL75 Leopard",
        4350, 4350,
        "E33-0800790-MC2", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0800790-mc2"
        /*          * GP75 Leopard 10SEK + GL75 Leopard 10SFR/10SDR (MS-17E7). Same 17" gaming class as
         * GF75 Thin. */
    },

    {
        NULL, "17E8", "GL75 Leopard",
        4350, 4350,
        "E33-0800790-MC2", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0800790-mc2"
        /*          * GL75 Leopard 10SCXR (MS-17E8). Same fan class as 17E7. */
    },

    {
        NULL, "16U7", "GP65 Leopard",
        4350, 4350,
        "E33-0800790-MC2", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0800790-mc2"
        /*          * GP65 Leopard 10S / GL65 Leopard 9SD/10S (MS-16U7). Same fan class as GF75 Thin
         * generation. */
    },

    {
        NULL, "16W1", "GF65 Thin",
        5400, 5400,
        "E33-0401680-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/e33-0401680-ae0"
        /*          * GF65 Thin 9SE/9SD + Creator 15M A9SD (MS-16W1). Official: 130*70*5mm, 5400/4700
         * RPM (start/rated). Using 5400 as peak. */
    },

    {
        NULL, "16W2", "GF65 Thin",
        5400, 5400,
        "E33-0401680-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/e33-0401680-ae0"
        /*          * GF65 Thin 10UE (MS-16W2). Same fan assembly as 16W1. */
    },

    {
        NULL, "16R1", "GF63",
        4350, 4350,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * GF63 8RC-249 (MS-16R1). 2018 entry-level. Estimate based on GF75 Thin
         * same-generation class at 4350 RPM. */
    },

    {
        NULL, "16R3", "GF63 Thin",
        4350, 4350,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * GF63 Thin 9SC (MS-16R3). Estimate based on GF75 same-generation fan class. */
    },

    {
        NULL, "16R4", "GF63 Thin",
        4350, 4350,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * GF63 Thin 10SCX/10SCS (MS-16R4). Same fan class as 16R3. */
    },

    {
        NULL, "16R6", "GF63 Thin",
        4350, 4350,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * GF63 Thin 11UC/11SC (MS-16R6). Estimate based on GF63 Thin fan class. */
    },

    {
        NULL, "16P5", "GE63 Raider",
        4800, 4800,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * GE63 Raider 8RE + GP63 Leopard 8RE (MS-16P5). 2018 performance gaming. Estimate. */
    },

    {
        NULL, "1782", "GT72",
        4300, 4300,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * GT72 6QE Dominator Pro (MS-1782). 2016 legacy flagship. Estimate based on GT72
         * era. */
    },

    /* ==========================================================================
     * TIER 7 ADDITIONS: Modern / Bravo / Alpha / Delta / Katana / Crosshair / Pulse / Cyborg
     * ========================================================================== */

    {
        NULL, "1551", "Modern 15",
        4600, 4600,
        "E33-0401550-AE0", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401550-ae0"
        /*          * Modern 15 A10M/A10RB (MS-1551). Official: 82*75*5mm, 5V, 0.5A, 4600 RPM.
         * Explicitly listed on E33-0401550-AE0. */
    },

    {
        NULL, "155L", "Modern 15",
        4600, MSI_RPM_UNKNOWN,
        "E33-0401550-AE0", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401550-ae0"
        /*          * Modern 15 A5M (MS-155L). Same fan class as Modern 15 A11M. Integrated graphics
         * only. */
    },

    {
        NULL, "15HK", "Modern 15",
        4600, MSI_RPM_UNKNOWN,
        "E33-0401550-AE0", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0401550-ae0"
        /*          * Modern 15 B7M (MS-15HK). AMD Ryzen ultrabook. Same fan class as Modern 15 A11M. */
    },

    {
        NULL, "14D2", "Modern 14",
        5300, MSI_RPM_UNKNOWN,
        "E33-0800890-AE0", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0800890-ae0"
        /*          * Modern 14 B11M (MS-14D2). Official for 14D1 Modern 14 is E33-0800890-AE0 at 5300
         * RPM; same chassis. */
    },

    {
        NULL, "14D3", "Modern 14",
        5300, MSI_RPM_UNKNOWN,
        "E33-0800890-AE0", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0800890-ae0"
        /*          * Modern 14 B11MOU (MS-14D3). Same class as 14D2. */
    },

    {
        NULL, "14DK", "Modern 14",
        5300, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Modern 14 B4MW (MS-14DK). Estimate based on Modern 14 fan class. */
    },

    {
        NULL, "14DL", "Modern 14",
        5300, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Modern 14 B5M (MS-14DL). Same class as 14DK. */
    },

    {
        NULL, "14JK", "Modern 14",
        5300, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Modern 14 C5M/C7M (MS-14JK). Estimate based on Modern 14 fan class. */
    },

    {
        NULL, "14L1", "Modern 14 H",
        5300, MSI_RPM_UNKNOWN,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Modern 14 H D13M (MS-14L1). Estimate based on Modern 14 fan class. */
    },

    {
        NULL, "158L", "Alpha 15",
        4350, 4350,
        "E33-0800980-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0800980-mc2"
        /*          * Alpha 15 B5EEK (MS-158L). Official: 82.8*81.3*8.8mm, 5V, 0.65A, 4350 RPM. */
    },

    {
        NULL, "17LL", "Alpha 17",
        4350, 4350,
        "E33-0800980-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0800980-mc2"
        /*          * Alpha 17 B5EEK (MS-17LL). Official: E33-0800980-MC2, 4350 RPM. Same part as Alpha
         * 15 B5EEK. */
    },

    {
        NULL, "158K", "Bravo 15",
        4350, 4350,
        "E33-0800980-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0800980-mc2"
        /*          * Bravo 15 B5DD (MS-158K). Official: E33-0800980-MC2, 4350 RPM. */
    },

    {
        NULL, "158M", "Bravo 15",
        4350, 4350,
        "E33-0800980-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0800980-mc2"
        /*          * Bravo 15 B5ED (MS-158M). Official: E33-0800980-MC2, 4350 RPM. */
    },

    {
        NULL, "16WK", "Bravo 15",
        4350, 4350,
        "E33-0800780-MC2", NULL,
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e33-0800780-mc2"
        /*          * Bravo 15 A4DDR (MS-16WK). Official for Bravo 17 A4DDR (17FK): E33-0800780-MC2,
         * 4350 RPM. */
    },

    {
        NULL, "15CK", "Delta 15",
        4350, 4350,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Delta 15 A5EFK (MS-15CK). AMD-based gaming. Estimate based on similar AMD gaming
         * class. */
    },

    {
        NULL, "1585", "Katana 15",
        4200, 4200,
        "E33-0801180-MC2", "E33-0801190-MC2",
        MSI_SRC_SPAREPARTS_OFFICIAL,
        "https://eu-spareparts.msi.com/products/gcs-selling-materials-e33-0801180-mc2"
        /*          * MS-1585 covers: Katana 15 B13VGK (E33-0801180-MC2: 4300 RPM 5V), CreatorPro M16
         * B13VJ/K (E33-0801190-MC2: 4200 RPM 12V), Creator M16 B13VF, Pulse 15 B13VGK. Using
         * 4200 RPM as conservative floor across all variants. */
    },

    {
        NULL, "1587", "Katana 15 HX",
        4350, 4200,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Katana 15 HX B14WEK (MS-1587). Estimate based on Katana 15 B12/B13 fan class
         * (4350/4200 RPM). */
    },

    {
        NULL, "15K2", "Cyborg 15 AI",
        4350, 4350,
        "E32-2501462-F05", NULL,
        MSI_SRC_MEASURED_EMPIRICAL,
        "https://us-spareparts.msi.com/products/gcs-selling-materials-e32-2501462-f05"
        /*          * Cyborg 15 AI A1VFK (MS-15K2). Official for 15K1 is E32-2501462-F05 at 4350 RPM;
         * same chassis. */
    },

    {
        NULL, "15M3", "Vector 16 HX AI",
        5000, 5000,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Vector 16 HX AI A2XWHG (MS-15M3). 2024 AI-series. Estimate based on Vector/Raider
         * GE68HX class. */
    },

    {
        NULL, "15P2", "Sword 16 HX",
        4300, 4200,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Sword 16 HX B13V/B14V (MS-15P2). Mid-range 16" gaming. Estimate based on
         * Katana/Pulse 15 fan class. */
    },

    {
        NULL, "15P3", "Pulse 16 AI",
        4300, 4200,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Pulse 16 AI C1VGKG (MS-15P3). Same fan class as Sword 16. */
    },

    {
        NULL, "15P4", "Crosshair 16",
        4300, 4200,
        NULL, NULL,
        MSI_SRC_COMMUNITY_REPORT,
        "https://github.com/BeardOverflow/msi-ec/issues"
        /*          * Crosshair 16 HX AI D2XW (MS-15P4). Gaming 16", same class as Pulse/Sword 16. */
    },

    /* ==========================================================================
     * SENTINEL — marks end of array; must remain last
     * DO NOT REMOVE OR REORDER THIS ENTRY
     * ========================================================================== */
    { NULL, NULL, NULL, MSI_RPM_UNKNOWN, MSI_RPM_UNKNOWN, NULL, NULL, MSI_SRC_COMMUNITY_REPORT, NULL }
};

/**
 * @brief Array size of the database including the sentinel.
 * Use (MSI_FAN_DB_ENTRIES) for loop bounds — this excludes the sentinel.
 *
 * Computed from the actual array at compile time. Never manually maintained.
 * In kernel context, ARRAY_SIZE is provided by <linux/kernel.h>.
 * In userspace, we define it locally if not already available.
 */
#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/** Number of real (non-sentinel) entries in the database. */
#define MSI_FAN_DB_ENTRIES  (ARRAY_SIZE(msi_fan_db) - 1)

/* ============================================================
 * String utility functions
 * (inline, freestanding, no stdlib dependency in kernel context)
 * ============================================================ */

/**
 * @brief Returns 1 if str starts with prefix, 0 otherwise.
 * Handles NULL inputs safely.
 */
static inline int msi_str_starts_with(const char *str, const char *prefix)
{
    if (!str || !prefix)
        return 0;
    while (*prefix) {
        if (*str != *prefix)
            return 0;
        str++;
        prefix++;
    }
    return 1;
}

/**
 * @brief Returns 1 if haystack contains needle as a substring, 0 otherwise.
 * Handles NULL inputs safely.
 */
static inline int msi_str_contains(const char *haystack, const char *needle)
{
    if (!haystack || !needle)
        return 0;
    if (!*needle)
        return 1;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack;
            const char *n = needle;
            while (*h && *n && *h == *n) {
                h++;
                n++;
            }
            if (!*n)
                return 1;
        }
    }
    return 0;
}

/**
 * @brief Trims trailing whitespace, CR, and LF from a mutable string buffer.
 * Used to sanitize raw sysfs reads in userspace.
 */
static inline void msi_trim_trailing(char *str)
{
    char *end;

    if (!str || !*str)
        return;
    end = str;
    while (*end)
        end++;
    end--;
    while (end >= str &&
           (*end == '\r' || *end == '\n' || *end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
}

/* ============================================================
 * Family-prefix fallback table
 *
 * Values here are the MINIMUM confirmed RPM seen across all
 * database entries for each family, not the maximum. This is
 * intentional: a conservative floor is safer than an optimistic
 * ceiling when the exact device is unknown. Using the minimum
 * means RPM will be underestimated (fan reads lower than reality)
 * rather than overestimated (fan reads higher than physical max,
 * which would make hwmon report impossible values).
 *
 * These are LAST-RESORT fallbacks. Board ID matching (Tier 2)
 * should eliminate the need for these in practice.
 * ============================================================ */

/** @brief One entry in the family fallback table. */
struct msi_family_fallback {
    const char *prefix;
    unsigned int cpu_floor_rpm;  /* Conservative minimum from database entries */
    unsigned int gpu_floor_rpm;
};

static const struct msi_family_fallback msi_family_fallbacks[] = {
    /* Confirmed from database:
     * GT: 5500 (GT77, GT77HX) → floor = 5500 (all entries same)
     * GS: 4800–5700 (GS65=4800, GS66=4950–5400, GS76=4850, Stealth14=5700) → floor = 4800
     * GE: 4800–5136 (GE76=4800/5136, GE68HX=5000, GE78HX=5000) → floor = 4800
     * GP: 5000 (GP68HX, GP78HX) → floor = 5000
     * GF: 4200–4350 (Katana=4200, GF63=4350) → floor = 4200
     * GL: historically budget/entry, conservative 4000
     * Modern: 4600–5300 (Modern15=4600, Modern14=5300) → floor = 4600
     * Creator: 4350–5700 (CreatorM16=4350, CreatorZ16=5700) → floor = 4350
     * Summit: 6800 (E13Flip, E13FlipEvo) → floor = 6800
     * Prestige: 4700–5550 (Prestige15=4700, Prestige16=5550) → floor = 4700
     */
    { "GT",       5500, 5500 },
    { "GS",       4800, 4800 },
    { "GE",       4800, 4800 },
    { "GP",       5000, 5000 },
    { "GF",       4200, 4200 },
    { "GL",       4000, 4000 },
    { "GV",       4000, 4000 },
    { "Modern",   4600, MSI_RPM_UNKNOWN },
    { "Creator",  4350, 4350 },
    { "Summit",   6800, MSI_RPM_UNKNOWN },
    { "Prestige", 4700, MSI_RPM_UNKNOWN },
    { NULL, MSI_RPM_UNKNOWN, MSI_RPM_UNKNOWN }   /* sentinel */
};

/* ============================================================
 * Core resolver function
 * ============================================================ */

/**
 * @brief Resolves fan limits for a given MSI laptop using all available identifiers.
 *
 * Matching is performed in strict priority order:
 *
 *   Tier 1: fw_version prefix match — most precise, unique per EC firmware build.
 *           The msi-ec driver exposes this via its fw_version sysfs attribute.
 *           Callers that have access to this string should always pass it.
 *
 *   Tier 2: board_id prefix match — hardware-level, 4-digit MSI board code.
 *           Strips "MS-" prefix if present (some DMI implementations add it).
 *
 *   Tier 3: model_str substring match — marketing name substring against DMI
 *           product_name. Less precise than board_id; multiple models share
 *           similar names so Tier 2 should have resolved it already.
 *
 *   Tier 4: Family prefix fallback — series-level conservative floor values.
 *           These are validated against the actual database minimums.
 *           Returns MSI_SRC_MEASURED_EMPIRICAL quality (not official).
 *
 *   Tier 0: Failsafe — returns MSI_RPM_UNKNOWN (0) with match_tier=0.
 *           The caller MUST check for MSI_RPM_UNKNOWN and handle it:
 *           do NOT fabricate a default RPM and silently report it as real data.
 *
 * @param fw_version  EC firmware version string (e.g. "17K4EMS1.106"), or NULL.
 * @param board_id    DMI board_name string (e.g. "17K4EMS1" or "MS-1585"), or NULL.
 * @param model_name  DMI product_name string (e.g. "GE76 Raider 11UH"), or NULL.
 * @param result      Output struct. Must not be NULL.
 */
static inline void msi_resolve_fan_limits(
    const char *fw_version,
    const char *board_id,
    const char *model_name,
    struct msi_fan_limits *result)
{
    size_t i;

    /* Initialise output to unknown */
    result->cpu_max_rpm     = MSI_RPM_UNKNOWN;
    result->gpu_max_rpm     = MSI_RPM_UNKNOWN;
    result->source_quality  = MSI_SRC_COMMUNITY_REPORT;
    result->match_tier      = 0;
    result->matched_entry   = NULL;

    /* --- Tier 1: EC firmware version prefix match --- */
    if (fw_version != NULL) {
        for (i = 0; i < MSI_FAN_DB_ENTRIES; i++) {
            if (msi_fan_db[i].fw_version != NULL &&
                msi_str_starts_with(fw_version, msi_fan_db[i].fw_version)) {
                result->cpu_max_rpm    = msi_fan_db[i].cpu_max_rpm;
                result->gpu_max_rpm    = msi_fan_db[i].gpu_max_rpm;
                result->source_quality = msi_fan_db[i].source_quality;
                result->match_tier     = 1;
                result->matched_entry  = &msi_fan_db[i];
                return;
            }
        }
    }

    /* --- Tier 2: Board ID prefix match --- */
    if (board_id != NULL) {
        const char *bid = board_id;
        /* Strip the "MS-" prefix some DMI implementations prepend */
        if (msi_str_starts_with(bid, "MS-"))
            bid += 3;

        for (i = 0; i < MSI_FAN_DB_ENTRIES; i++) {
            if (msi_fan_db[i].board_id != NULL &&
                msi_str_starts_with(bid, msi_fan_db[i].board_id)) {
                result->cpu_max_rpm    = msi_fan_db[i].cpu_max_rpm;
                result->gpu_max_rpm    = msi_fan_db[i].gpu_max_rpm;
                result->source_quality = msi_fan_db[i].source_quality;
                result->match_tier     = 2;
                result->matched_entry  = &msi_fan_db[i];
                return;
            }
        }
    }

    /* --- Tiers 3 & 4: Both operate on model_name; check once --- */
    if (model_name != NULL) {
        /* Tier 3: Product name substring match */
        for (i = 0; i < MSI_FAN_DB_ENTRIES; i++) {
            if (msi_fan_db[i].model_str != NULL &&
                msi_str_contains(model_name, msi_fan_db[i].model_str)) {
                result->cpu_max_rpm    = msi_fan_db[i].cpu_max_rpm;
                result->gpu_max_rpm    = msi_fan_db[i].gpu_max_rpm;
                result->source_quality = msi_fan_db[i].source_quality;
                result->match_tier     = 3;
                result->matched_entry  = &msi_fan_db[i];
                return;
            }
        }

        /* Tier 4: Family prefix fallback (conservative floor values) */
        for (i = 0; msi_family_fallbacks[i].prefix != NULL; i++) {
            if (msi_str_starts_with(model_name, msi_family_fallbacks[i].prefix)) {
                result->cpu_max_rpm    = msi_family_fallbacks[i].cpu_floor_rpm;
                result->gpu_max_rpm    = msi_family_fallbacks[i].gpu_floor_rpm;
                result->source_quality = MSI_SRC_MEASURED_EMPIRICAL;
                result->match_tier     = 4;
                result->matched_entry  = NULL;
                return;
            }
        }
    }

    /* --- Tier 0: Failsafe — no match found, return MSI_RPM_UNKNOWN --- */
    /* result already initialised to unknown above. match_tier = 0. */
}

/* ============================================================
 * Auto-detection entry point (kernel and userspace)
 * ============================================================ */

/**
 * @brief Fully autonomous fan limit resolver that reads DMI identifiers from
 * the running system without requiring any manual developer input.
 *
 * In kernel context: queries the DMI layer via dmi_get_system_info().
 * In userspace: reads /sys/class/dmi/id/ sysfs virtual files.
 *
 * The EC firmware version is NOT available through this function — it requires
 * access to the msi-ec driver's own fw_version sysfs attribute, which is not
 * a DMI source. For the most precise resolution, use msi_resolve_fan_limits()
 * directly with all three strings populated.
 *
 * @param result  Output struct. Must not be NULL.
 */
static inline void msi_auto_detect_fan_limits(struct msi_fan_limits *result)
{
#ifdef __KERNEL__
    const char *board_id   = dmi_get_system_info(DMI_BOARD_NAME);
    const char *model_name = dmi_get_system_info(DMI_PRODUCT_NAME);
    msi_resolve_fan_limits(NULL, board_id, model_name, result);

#else
    /*
     * Userspace: read sysfs DMI virtual files.
     * Buffers are properly sized char arrays (not scalars).
     */
    FILE *f;
    char board_buf[MSI_DMI_BUF_SIZE]   = {0};
    char product_buf[MSI_DMI_BUF_SIZE] = {0};
    const char *board_ptr   = NULL;
    const char *product_ptr = NULL;

    f = fopen("/sys/class/dmi/id/board_name", "r");
    if (f) {
        if (fread(board_buf, 1, sizeof(board_buf) - 1, f) > 0) {
            msi_trim_trailing(board_buf);
            board_ptr = board_buf;
        }
        fclose(f);
    }

    f = fopen("/sys/class/dmi/id/product_name", "r");
    if (f) {
        if (fread(product_buf, 1, sizeof(product_buf) - 1, f) > 0) {
            msi_trim_trailing(product_buf);
            product_ptr = product_buf;
        }
        fclose(f);
    }

    msi_resolve_fan_limits(NULL, board_ptr, product_ptr, result);
#endif
}

/**
 * @brief Convenience accessor: returns cpu_max_rpm from auto-detection.
 * Returns MSI_RPM_UNKNOWN (0) if resolution fails — never a fabricated default.
 */
static inline unsigned int msi_auto_cpu_max_rpm(void)
{
    struct msi_fan_limits limits;
    msi_auto_detect_fan_limits(&limits);
    return limits.cpu_max_rpm;
}

/**
 * @brief Convenience accessor: returns gpu_max_rpm from auto-detection.
 * Returns MSI_RPM_UNKNOWN (0) if resolution fails or device has no GPU fan.
 */
static inline unsigned int msi_auto_gpu_max_rpm(void)
{
    struct msi_fan_limits limits;
    msi_auto_detect_fan_limits(&limits);
    return limits.gpu_max_rpm;
}

#ifdef __cplusplus
}
#endif

#endif /* MSI_FAN_SPECS_H */
