#!/usr/bin/env bash
# =============================================================================
# msi-fan-check.sh  —  Fan & temperature consistency checker for MSI laptops
# Reads all reporting channels simultaneously and cross-validates them.
#
# CHANNELS CHECKED
#   • EC debug sysfs   /sys/devices/platform/msi-ec/debug/ec_get
#                      Direct register reads: 0x71/0x89 (fan %), 0x68/0x80 (temp)
#   • Platform sysfs   /sys/devices/platform/msi-ec/{cpu,gpu}/realtime_*
#                      Driver-cooked values: raw EC % and raw °C
#   • hwmon sysfs      /sys/class/hwmon/hwmon<N>/  (our msi_ec hwmon layer)
#                      Standard ABI: fan in RPM, temp in millidegrees
#   • sensors(1)       libsensors — reads the same hwmon sysfs
#
# CROSS-CHECKS
#   1. EC register byte        == platform raw %         (exact)
#   2. EC register byte        == platform raw °C        (exact)
#   3. platform raw °C × 1000 == hwmon millideg          (exact)
#   4. platform % × max_rpm/100== hwmon RPM              (±1 RPM)
#   5. sensors RPM             == hwmon RPM              (exact)
#   6. sensors °C integer      == hwmon millideg/1000    (exact)
#
# USAGE
#   sudo bash msi-fan-check.sh            # single snapshot
#   sudo bash msi-fan-check.sh --watch    # live refresh every 2 s (Ctrl-C to stop)
#   sudo bash msi-fan-check.sh --json     # machine-readable JSON
#   sudo bash msi-fan-check.sh --quiet    # checks only; exit 0=all-pass/NA, 1=any-fail
# =============================================================================

# ── Strict mode — but NOT set -e; we handle errors explicitly ─────────────────
set -uo pipefail

# ── Colour (suppressed when not a tty) ────────────────────────────────────────
if [[ -t 1 ]]; then
    R='\033[0;31m'  G='\033[0;32m'  Y='\033[0;33m'
    B='\033[0;34m'  C='\033[0;36m'  W='\033[1;37m'
    D='\033[2m'     N='\033[0m'
else
    R='' G='' Y='' B='' C='' W='' D='' N=''
fi

# ── Args ──────────────────────────────────────────────────────────────────────
MODE=normal
for a in "$@"; do
    case "$a" in
        --watch)  MODE=watch  ;;
        --json)   MODE=json   ;;
        --quiet)  MODE=quiet  ;;
        --help|-h) sed -n '3,22p' "$0" | sed 's/^# \?//'; exit 0 ;;
    esac
done

[[ $EUID -ne 0 ]] && { echo "Requires root (EC debug reads). Use: sudo $0 $*" >&2; exit 1; }

# ── Helpers ───────────────────────────────────────────────────────────────────
rd() {  # rd <path> — read sysfs file, return N/A on any failure
    local p="$1"
    [[ -r "$p" ]] && tr -d '\n' < "$p" 2>/dev/null || echo -n "N/A"
}

ec_rd() {  # ec_rd <hex_addr_2chars> — read one EC byte via debug interface
    local addr="$1" get="/sys/devices/platform/msi-ec/debug/ec_get"
    [[ -w "$get" ]] || { echo -n "N/A"; return; }
    printf '%s' "$addr" > "$get" 2>/dev/null || { echo -n "N/A"; return; }
    tr -d '\n' < "$get" 2>/dev/null || echo -n "N/A"
}

hex2dec() {  # hex2dec <val> — convert hex string to decimal, pass N/A through
    local v="$1"
    [[ "$v" == "N/A" ]] && { echo -n "N/A"; return; }
    printf '%d' "0x${v}" 2>/dev/null || echo -n "N/A"
}

find_msi_hwmon() {
    local d n
    for d in /sys/class/hwmon/hwmon*; do
        n=$(rd "$d/name")
        [[ "$n" == "msi_ec" ]] && { echo -n "$d"; return; }
    done
    echo -n ""
}

# ── Check helpers: output PASS / FAIL / N/A, set global FAILS counter ─────────
FAILS=0

chk_exact() {  # chk_exact <got> <expected>
    [[ "$1" == "N/A" || "$2" == "N/A" ]] && { printf 'N/A '; return; }
    if [[ "$1" == "$2" ]]; then
        printf "${G}PASS${N}"
    else
        printf "${R}FAIL${N} (got %s, want %s)" "$1" "$2"
        FAILS=$(( FAILS + 1 ))
    fi
}

chk_within1() {  # chk_within1 <int_a> <int_b>
    [[ "$1" == "N/A" || "$2" == "N/A" ]] && { printf 'N/A '; return; }
    local diff=$(( $1 - $2 ))
    [[ $diff -lt 0 ]] && diff=$(( -diff ))
    if [[ $diff -le 1 ]]; then
        printf "${G}PASS${N}"
    else
        printf "${R}FAIL${N} (diff=%d RPM)" "$diff"
        FAILS=$(( FAILS + 1 ))
    fi
}

# ── Single snapshot ────────────────────────────────────────────────────────────
snapshot() {
    FAILS=0

    # ── Locate paths ──────────────────────────────────────────────────────────
    local PLAT="/sys/devices/platform/msi-ec"
    local HWMON; HWMON=$(find_msi_hwmon)
    local HAS_PLAT=0;    [[ -d "$PLAT"   ]]                  && HAS_PLAT=1
    local HAS_HWMON=0;   [[ -n "$HWMON"  ]]                  && HAS_HWMON=1
    local HAS_DEBUG=0;   [[ -w "$PLAT/debug/ec_get" ]]       && HAS_DEBUG=1
    local HAS_SENSORS=0; command -v sensors &>/dev/null       && HAS_SENSORS=1

    # ── EC debug reads ────────────────────────────────────────────────────────
    local ec_cpu_fan_h ec_gpu_fan_h ec_cpu_tmp_h ec_gpu_tmp_h
    ec_cpu_fan_h=$(ec_rd 71)   # CPU fan %   addr 0x71
    ec_gpu_fan_h=$(ec_rd 89)   # GPU fan %   addr 0x89
    ec_cpu_tmp_h=$(ec_rd 68)   # CPU temp°C  addr 0x68
    ec_gpu_tmp_h=$(ec_rd 80)   # GPU temp°C  addr 0x80

    local ec_cpu_fan ec_gpu_fan ec_cpu_tmp ec_gpu_tmp
    ec_cpu_fan=$(hex2dec "$ec_cpu_fan_h")
    ec_gpu_fan=$(hex2dec "$ec_gpu_fan_h")
    ec_cpu_tmp=$(hex2dec "$ec_cpu_tmp_h")
    ec_gpu_tmp=$(hex2dec "$ec_gpu_tmp_h")

    # ── Platform sysfs reads ──────────────────────────────────────────────────
    local p_fw p_mode p_boost p_cpu_fan p_cpu_tmp p_gpu_fan p_gpu_tmp
    p_fw=$(rd       "$PLAT/fw_version")
    p_mode=$(rd     "$PLAT/fan_mode")
    p_boost=$(rd    "$PLAT/cooler_boost")
    p_cpu_fan=$(rd  "$PLAT/cpu/realtime_fan_speed")
    p_cpu_tmp=$(rd  "$PLAT/cpu/realtime_temperature")
    p_gpu_fan=$(rd  "$PLAT/gpu/realtime_fan_speed")
    p_gpu_tmp=$(rd  "$PLAT/gpu/realtime_temperature")

    # ── hwmon sysfs reads ─────────────────────────────────────────────────────
    local h_cpu_rpm h_cpu_lbl h_gpu_rpm h_gpu_lbl
    local h_cpu_mc  h_cpu_tlbl h_gpu_mc h_gpu_tlbl
    h_cpu_rpm=$(rd  "$HWMON/fan1_input")
    h_cpu_lbl=$(rd  "$HWMON/fan1_label")
    h_gpu_rpm=$(rd  "$HWMON/fan2_input")
    h_gpu_lbl=$(rd  "$HWMON/fan2_label")
    h_cpu_mc=$(rd   "$HWMON/temp1_input")
    h_cpu_tlbl=$(rd "$HWMON/temp1_label")
    h_gpu_mc=$(rd   "$HWMON/temp2_input")
    h_gpu_tlbl=$(rd "$HWMON/temp2_label")

    # ── sensors(1) parse ─────────────────────────────────────────────────────
    local s_cpu_rpm="N/A" s_gpu_rpm="N/A" s_cpu_c="N/A" s_gpu_c="N/A"
    if [[ $HAS_SENSORS -eq 1 && $HAS_HWMON -eq 1 ]]; then
        local in_msi=0
        while IFS= read -r line; do
            [[ "$line" =~ ^msi_ec ]] && in_msi=1
            if [[ $in_msi -eq 1 ]]; then
                [[ -z "$line" ]] && break
                [[ "$line" =~ cpu_fan:[[:space:]]+([0-9]+)[[:space:]]+RPM  ]] && s_cpu_rpm="${BASH_REMATCH[1]}"
                [[ "$line" =~ gpu_fan:[[:space:]]+([0-9]+)[[:space:]]+RPM  ]] && s_gpu_rpm="${BASH_REMATCH[1]}"
                [[ "$line" =~ cpu_temp:[[:space:]]+\+([0-9]+)\. ]] && s_cpu_c="${BASH_REMATCH[1]}"
                [[ "$line" =~ gpu_temp:[[:space:]]+\+([0-9]+)\. ]] && s_gpu_c="${BASH_REMATCH[1]}"
            fi
        done < <(sensors 2>/dev/null || true)
    fi

    # ── Derived: back-calculate max_rpm from live reading ────────────────────
    local d_cpu_max="N/A" d_gpu_max="N/A"
    local d_cpu_exp="N/A" d_gpu_exp="N/A"
    if [[ "$h_cpu_rpm" != "N/A" && "$p_cpu_fan" != "N/A" \
       && "$p_cpu_fan" -gt 0    && "$h_cpu_rpm"  -gt 0 ]]; then
        d_cpu_max=$(( h_cpu_rpm * 100 / p_cpu_fan ))
        d_cpu_exp=$(( p_cpu_fan * d_cpu_max / 100 ))
    fi
    if [[ "$h_gpu_rpm" != "N/A" && "$p_gpu_fan" != "N/A" \
       && "$p_gpu_fan" -gt 0    && "$h_gpu_rpm"  -gt 0 ]]; then
        d_gpu_max=$(( h_gpu_rpm * 100 / p_gpu_fan ))
        d_gpu_exp=$(( p_gpu_fan * d_gpu_max / 100 ))
    fi
    # Zero fan: expected hwmon RPM is also 0
    [[ "$p_cpu_fan" != "N/A" && "$p_cpu_fan" -eq 0 ]] && d_cpu_exp=0
    [[ "$p_gpu_fan" != "N/A" && "$p_gpu_fan" -eq 0 ]] && d_gpu_exp=0

    # ── Expected platform°C × 1000 for temp check ────────────────────────────
    local e_cpu_mc="N/A" e_gpu_mc="N/A"
    [[ "$p_cpu_tmp" != "N/A" ]] && e_cpu_mc=$(( p_cpu_tmp * 1000 ))
    [[ "$p_gpu_tmp" != "N/A" ]] && e_gpu_mc=$(( p_gpu_tmp * 1000 ))

    # ── hwmon millideg / 1000 for sensors int comparison ─────────────────────
    local h_cpu_c="N/A" h_gpu_c="N/A"
    [[ "$h_cpu_mc" != "N/A" ]] && h_cpu_c=$(( h_cpu_mc / 1000 ))
    [[ "$h_gpu_mc" != "N/A" ]] && h_gpu_c=$(( h_gpu_mc / 1000 ))

    # ── JSON output ───────────────────────────────────────────────────────────
    if [[ "$MODE" == "json" ]]; then
        cat <<JEOF
{
  "timestamp":    "$(date '+%Y-%m-%d %H:%M:%S')",
  "fw_version":   "$p_fw",
  "fan_mode":     "$p_mode",
  "cooler_boost": "$p_boost",
  "cpu": {
    "ec_fan_hex":     "$ec_cpu_fan_h",
    "ec_fan_pct":     "$ec_cpu_fan",
    "ec_temp_hex":    "$ec_cpu_tmp_h",
    "ec_temp_c":      "$ec_cpu_tmp",
    "platform_pct":   "$p_cpu_fan",
    "platform_c":     "$p_cpu_tmp",
    "hwmon_rpm":      "$h_cpu_rpm",
    "hwmon_label":    "$h_cpu_lbl",
    "hwmon_mc":       "$h_cpu_mc",
    "hwmon_tlabel":   "$h_cpu_tlbl",
    "sensors_rpm":    "$s_cpu_rpm",
    "sensors_c":      "$s_cpu_c",
    "derived_maxrpm": "$d_cpu_max"
  },
  "gpu": {
    "ec_fan_hex":     "$ec_gpu_fan_h",
    "ec_fan_pct":     "$ec_gpu_fan",
    "ec_temp_hex":    "$ec_gpu_tmp_h",
    "ec_temp_c":      "$ec_gpu_tmp",
    "platform_pct":   "$p_gpu_fan",
    "platform_c":     "$p_gpu_tmp",
    "hwmon_rpm":      "$h_gpu_rpm",
    "hwmon_label":    "$h_gpu_lbl",
    "hwmon_mc":       "$h_gpu_mc",
    "hwmon_tlabel":   "$h_gpu_tlbl",
    "sensors_rpm":    "$s_gpu_rpm",
    "sensors_c":      "$s_gpu_c",
    "derived_maxrpm": "$d_gpu_max"
  }
}
JEOF
        return
    fi

    # ── Pre-compute check results (strings) ───────────────────────────────────
    local ck1_cpu ck1_gpu ck2_cpu ck2_gpu ck3_cpu ck3_gpu
    local ck4_cpu ck4_gpu ck5_cpu ck5_gpu ck6_cpu ck6_gpu

    ck1_cpu=$(chk_exact   "$ec_cpu_fan"  "$p_cpu_fan")  # EC % == platform %
    ck1_gpu=$(chk_exact   "$ec_gpu_fan"  "$p_gpu_fan")
    ck2_cpu=$(chk_exact   "$ec_cpu_tmp"  "$p_cpu_tmp")  # EC °C == platform °C
    ck2_gpu=$(chk_exact   "$ec_gpu_tmp"  "$p_gpu_tmp")
    ck3_cpu=$(chk_exact   "$e_cpu_mc"    "$h_cpu_mc")   # platform°C×1000 == hwmon mc
    ck3_gpu=$(chk_exact   "$e_gpu_mc"    "$h_gpu_mc")
    ck4_cpu=$(chk_within1 "$d_cpu_exp"   "$h_cpu_rpm")  # platform%→RPM ≈ hwmon RPM
    ck4_gpu=$(chk_within1 "$d_gpu_exp"   "$h_gpu_rpm")
    ck5_cpu=$(chk_exact   "$s_cpu_rpm"   "$h_cpu_rpm")  # sensors RPM == hwmon RPM
    ck5_gpu=$(chk_exact   "$s_gpu_rpm"   "$h_gpu_rpm")
    ck6_cpu=$(chk_exact   "$s_cpu_c"     "$h_cpu_c")    # sensors °C == hwmon mc/1000
    ck6_gpu=$(chk_exact   "$s_gpu_c"     "$h_gpu_c")

    # ── Quiet mode: checks only ───────────────────────────────────────────────
    if [[ "$MODE" == "quiet" ]]; then
        local labels=(
            "EC fan reg  == platform fan %     (exact)"
            "EC temp reg == platform temp °C   (exact)"
            "platform °C×1000 == hwmon mc      (exact)"
            "platform % → RPM ≈ hwmon RPM      (±1 RPM)"
            "sensors RPM == hwmon RPM           (exact)"
            "sensors °C  == hwmon mc/1000       (exact)"
        )
        local cpu_checks=("$ck1_cpu" "$ck2_cpu" "$ck3_cpu" "$ck4_cpu" "$ck5_cpu" "$ck6_cpu")
        local gpu_checks=("$ck1_gpu" "$ck2_gpu" "$ck3_gpu" "$ck4_gpu" "$ck5_gpu" "$ck6_gpu")
        local i
        for i in 0 1 2 3 4 5; do
            printf "  %-46s  CPU: %-30s  GPU: %s\n" \
                "${labels[$i]}" "${cpu_checks[$i]}" "${gpu_checks[$i]}"
        done
        echo ""
        if [[ $FAILS -eq 0 ]]; then
            echo -e "  ${G}✔  All checks passed (or N/A).${N}"
            return 0
        else
            echo -e "  ${R}✘  $FAILS check(s) FAILED.${N}"
            return 1
        fi
    fi

    # ── Normal display ────────────────────────────────────────────────────────
    [[ "$MODE" == "watch" ]] && { tput clear 2>/dev/null || printf '\033[H\033[2J\033[3J'; }

    # Header
    printf "${W}╔══════════════════════════════════════════════════════════════════════╗${N}\n"
    printf "${W}║        MSI Fan & Temperature  —  All-Channel Consistency Check       ║${N}\n"
    printf "${W}╚══════════════════════════════════════════════════════════════════════╝${N}\n"
    printf "  ${D}%s   FW: %s   mode: %s   cooler-boost: %s${N}\n" \
           "$(date '+%H:%M:%S')" "$p_fw" "$p_mode" "$p_boost"
    echo ""

    # Channel status
    local ps hs ds ss
    [[ $HAS_PLAT    -eq 1 ]] && ps="${G}●${N}" || ps="${R}✗${N}"
    [[ $HAS_HWMON   -eq 1 ]] && hs="${G}●${N}" || hs="${R}✗${N}"
    [[ $HAS_DEBUG   -eq 1 ]] && ds="${G}●${N}" || ds="${Y}○${N}"
    [[ $HAS_SENSORS -eq 1 ]] && ss="${G}●${N}" || ss="${Y}○${N}"
    printf "  Channels: %b platform-sysfs   %b hwmon-sysfs   %b EC-debug   %b sensors(1)\n" \
           "$ps" "$hs" "$ds" "$ss"
    [[ $HAS_PLAT    -eq 0 ]] && echo "            !! platform sysfs absent — is msi-ec loaded?"
    [[ $HAS_HWMON   -eq 0 ]] && echo "            !! hwmon device absent  — reload without debug=1"
    [[ $HAS_DEBUG   -eq 0 ]] && echo "            -- EC debug absent     — reload with:  insmod msi-ec.ko debug=1"
    [[ $HAS_SENSORS -eq 0 ]] && echo "            -- sensors missing     — install: apt install lm-sensors && sensors-detect"
    echo ""

    # ── Data table ────────────────────────────────────────────────────────────
    local col=20  # column width for CPU / GPU values
    local lbl=38  # label column width
    local sep; sep=$(printf '─%.0s' $(seq 1 $lbl))
    local sep2; sep2=$(printf '─%.0s' $(seq 1 $col))

    printf "  ${C}%-${lbl}s  %-${col}s  %-${col}s${N}\n" "Channel / Attribute" "CPU" "GPU"
    printf "  %-${lbl}s  %-${col}s  %-${col}s\n" "$sep" "$sep2" "$sep2"

    row() { printf "  %-${lbl}s  %-${col}s  %-${col}s\n" "$1" "$2" "$3"; }

    row "EC debug  fan%     (0x71 / 0x89)"   \
        "0x${ec_cpu_fan_h} = ${ec_cpu_fan}%"  \
        "0x${ec_gpu_fan_h} = ${ec_gpu_fan}%"
    row "EC debug  temp°C   (0x68 / 0x80)"   \
        "0x${ec_cpu_tmp_h} = ${ec_cpu_tmp}°C" \
        "0x${ec_gpu_tmp_h} = ${ec_gpu_tmp}°C"
    row "" "" ""
    row "Platform  fan%  realtime_fan_speed"  "${p_cpu_fan}%"     "${p_gpu_fan}%"
    row "Platform  temp  realtime_temperature" "${p_cpu_tmp}°C"   "${p_gpu_tmp}°C"
    row "" "" ""
    row "hwmon     fan   fan1/fan2_input"      "${h_cpu_rpm} RPM"  "${h_gpu_rpm} RPM"
    row "hwmon     label fan1/fan2_label"      "${h_cpu_lbl}"      "${h_gpu_lbl}"
    row "hwmon     temp  temp1/2_input"        "${h_cpu_mc} m°C"   "${h_gpu_mc} m°C"
    row "hwmon     label temp1/2_label"        "${h_cpu_tlbl}"     "${h_gpu_tlbl}"
    row "" "" ""
    row "sensors(1) fan RPM"                   "${s_cpu_rpm} RPM"  "${s_gpu_rpm} RPM"
    row "sensors(1) temp °C"                   "${s_cpu_c}°C"      "${s_gpu_c}°C"
    row "" "" ""
    row "  └─ derived max_rpm  (RPM×100/pct)"  "${d_cpu_max} RPM"  "${d_gpu_max} RPM"

    printf "  %-${lbl}s  %-${col}s  %-${col}s\n" "$sep" "$sep2" "$sep2"
    echo ""

    # ── Consistency checks table ───────────────────────────────────────────────
    printf "  ${W}Consistency checks:${N}\n"
    local csep; csep=$(printf '─%.0s' $(seq 1 46))
    local csep2; csep2=$(printf '─%.0s' $(seq 1 32))
    printf "  %-46s  %-32s  %s\n" "$csep" "$csep2" "$csep2"

    chkrow() { printf "  %-46s  CPU: %-28s  GPU: %s\n" "$1" "$2" "$3"; }

    chkrow "EC fan reg  == platform fan %     (exact)"   "$ck1_cpu" "$ck1_gpu"
    chkrow "EC temp reg == platform temp °C   (exact)"   "$ck2_cpu" "$ck2_gpu"
    chkrow "platform °C × 1000 == hwmon mc    (exact)"   "$ck3_cpu" "$ck3_gpu"
    chkrow "platform % → RPM ≈ hwmon RPM      (±1 RPM)"  "$ck4_cpu" "$ck4_gpu"
    chkrow "sensors RPM == hwmon RPM           (exact)"   "$ck5_cpu" "$ck5_gpu"
    chkrow "sensors °C  == hwmon mc/1000       (exact)"   "$ck6_cpu" "$ck6_gpu"

    printf "  %-46s  %-32s  %s\n" "$csep" "$csep2" "$csep2"
    echo ""

    # ── Summary ───────────────────────────────────────────────────────────────
    if [[ $FAILS -eq 0 ]]; then
        echo -e "  ${G}✔  All channels consistent.${N}"
    else
        echo -e "  ${R}✘  $FAILS check(s) FAILED — see highlighted rows above.${N}"
        echo ""
        echo -e "  ${D}  FAIL on EC ↔ platform:   EC address mapping wrong in driver config${N}"
        echo -e "  ${D}  FAIL on platform ↔ hwmon: conversion bug in hwmon read() callback${N}"
        echo -e "  ${D}  FAIL on sensors ↔ hwmon:  libsensors reading wrong hwmon device${N}"
    fi

    [[ "$MODE" == "watch" ]] && echo -e "\n  ${D}Refreshing every 2 s — Ctrl-C to stop${N}"
    echo ""
}

# ── Entry point ───────────────────────────────────────────────────────────────
case "$MODE" in
    watch)
        while true; do snapshot; sleep 2; done
        ;;
    quiet)
        snapshot
        exit $FAILS
        ;;
    *)
        snapshot
        ;;
esac
