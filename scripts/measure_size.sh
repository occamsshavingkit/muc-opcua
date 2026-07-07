#!/usr/bin/env bash
set -eu

usage() {
    cat <<'USAGE'
Usage: scripts/measure_size.sh [nano|micro|standard|embedded|full|all] [--lto] [--json]

Cross-compiles the portable core with arm-none-eabi-gcc for Cortex-M0+
Thumb code and reports archive + linked-ELF section totals.
Options:
  --lto    Also produce LTO (Link-Time Optimisation) ELF measurements
  --json   Accepted for compatibility; JSON output is always written
USAGE
}

if [ "$#" -eq 0 ]; then
    usage
    exit 0
fi

lto_flag=false
json_flag=true
profile_arg=""

for arg in "$@"; do
    case "$arg" in
        -h|--help) usage; exit 0 ;;
        --lto) lto_flag=true ;;
        --json) json_flag=true ;;
        *) profile_arg="$arg" ;;
    esac
done

case "$profile_arg" in
    nano|micro|standard|embedded|full-featured|full|all)
        if [ "$profile_arg" = "all" ]; then
            profiles="nano micro standard embedded full"
        else
            profiles="$profile_arg"
        fi
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

command -v cmake >/dev/null 2>&1 || {
    echo "cmake not found" >&2
    exit 127
}

if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    echo "warning: arm-none-eabi-gcc not found; skipping size measurement" >&2
    exit 0
fi
if ! command -v arm-none-eabi-size >/dev/null 2>&1; then
    echo "warning: arm-none-eabi-size not found; skipping size measurement" >&2
    exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
build_root="${BUILD_ROOT:-build/size-arm}"
case "$build_root" in
    build|build/|./build|./build/)
        echo "error: BUILD_ROOT must not be the host test build directory; use build/size-arm or another isolated subdirectory" >&2
        exit 2
        ;;
esac
mkdir -p "$build_root"

json_file="${build_root}/size-report.json"
json_entries=""
first_entry=true

printf '%-14s %10s %8s %8s %10s %10s %8s %8s %10s\n' \
    "profile" "text" "data" "bss" "dec" "elf_text" "elf_data" "elf_bss" "elf_dec"

get_profile_defs() {
    local profile="$1"
    case "$profile" in
        nano)
            echo "-DMUC_OPCUA_PROFILE=nano"
            ;;
        micro)
            printf '%s\n' "-DMUC_OPCUA_PROFILE=micro" "-DMU_MAX_MONITORED_ITEMS=8"
            ;;
        standard)
            printf '%s\n' "-DMUC_OPCUA_PROFILE=standard" "-DMUC_OPCUA_STANDARD_PROFILE=ON" \
                "-DMU_MAX_SUBSCRIPTIONS=50" "-DMU_MAX_MONITORED_ITEMS=1000" \
                "-DMU_MAX_PUBLISH_REQUESTS=50" "-DMU_MONITORED_QUEUE_DEPTH=2" \
                "-DMU_MAX_TRIGGER_LINKS=4"
            ;;
        embedded)
            printf '%s\n' "-DMUC_OPCUA_PROFILE=embedded" "-DMU_MAX_SUBSCRIPTIONS=2" \
                "-DMU_MAX_MONITORED_ITEMS=100" "-DMU_MAX_PUBLISH_REQUESTS=5" \
                "-DMU_MONITORED_QUEUE_DEPTH=2" "-DMU_MAX_TRIGGER_LINKS=4"
            ;;
        full-featured|full)
            printf '%s\n' "-DMUC_OPCUA_PROFILE=full" "-DMU_MAX_SUBSCRIPTIONS=100" \
                "-DMU_MAX_MONITORED_ITEMS=2000" "-DMU_MAX_PUBLISH_REQUESTS=100" \
                "-DMU_MONITORED_QUEUE_DEPTH=2" "-DMU_MAX_TRIGGER_LINKS=4"
            ;;
    esac
}

link_elf() {
    local build_dir="$1" cflags="$2" elf_out="$3"
    local ss="$SCRIPT_DIR"
    arm-none-eabi-gcc $cflags \
        -Wl,--gc-sections \
        -T "$ss/size_measure.ld" \
        "$ss/size_measure_startup.c" \
        "$ss/size_measure_main.c" \
        -L "$build_dir/src" -lmuc_opcua \
        -I "$ss/../include" \
        -o "$elf_out" >/dev/null 2>&1
}

build_and_measure() {
    local profile="$1" lto="$2"
    local suffix=""
    if $lto; then suffix="-lto"; fi
    local build_dir="${build_root}/${profile}${suffix}"
    local archive="${build_dir}/src/libmuc_opcua.a"
    local cflags="-mcpu=cortex-m0plus -mthumb"
    if $lto; then
        cflags="$cflags -flto -ffat-lto-objects"
    fi

    local line
    readarray -t defs < <(get_profile_defs "$profile")

    cmake -S . -B "$build_dir" \
        -DCMAKE_SYSTEM_NAME=Generic \
        -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_C_FLAGS="$cflags" \
        -DMUC_OPCUA_PLATFORM=arduino-skeleton \
        -DMUC_OPCUA_OPTIMIZE_SIZE=ON \
        "${defs[@]}" >/dev/null 2>&1
    cmake --build "$build_dir" --target muc_opcua >/dev/null 2>&1

    local arch_text=0 arch_data=0 arch_bss=0 arch_dec=0
    line="$(arm-none-eabi-size -t "$archive" 2>/dev/null | awk '/\(TOTALS\)$/ { print $1, $2, $3, $4 }')"
    if [ -n "$line" ]; then
        read arch_text arch_data arch_bss arch_dec <<< "$line"
    fi

    local elf_text=0 elf_data=0 elf_bss=0 elf_dec=0
    local elf_out="${build_dir}/size_measure.elf"
    if link_elf "$build_dir" "$cflags" "$elf_out"; then
        line="$(arm-none-eabi-size -t "$elf_out" 2>/dev/null | awk '/\(TOTALS\)$/ { print $1, $2, $3, $4 }')"
        if [ -n "$line" ]; then
            read elf_text elf_data elf_bss elf_dec <<< "$line"
        fi
        printf '%-14s %10d %8d %8d %10d %10d %8d %8d %10d\n' \
            "${profile}${suffix}" "$arch_text" "$arch_data" "$arch_bss" \
            "$arch_dec" "$elf_text" "$elf_data" "$elf_bss" "$elf_dec"
    else
        printf '%-14s %10d %8d %8d %10d %10s %8s %8s %10s\n' \
            "${profile}${suffix}" "$arch_text" "$arch_data" "$arch_bss" \
            "$arch_dec" "-" "-" "-" "-"
    fi

    if $json_flag; then
        local entry
        entry="{\"profile\":\"${profile}${suffix}\",\"archive\":{\"text\":${arch_text},\"data\":${arch_data},\"bss\":${arch_bss},\"dec\":${arch_dec}},\"elf\":{\"text\":${elf_text},\"data\":${elf_data},\"bss\":${elf_bss},\"dec\":${elf_dec},\"path\":\"${elf_out}\"}"
        if $lto; then
            entry="${entry},\"lto_elf\":{\"text\":${elf_text},\"data\":${elf_data},\"bss\":${elf_bss},\"dec\":${elf_dec},\"path\":\"${elf_out}\"}"
        fi
        entry="${entry}}"
        if $first_entry; then
            json_entries="$entry"
            first_entry=false
        else
            json_entries="${json_entries},${entry}"
        fi
    fi
}

for profile in $profiles; do
    build_and_measure "$profile" false
    if $lto_flag; then
        build_and_measure "$profile" true
    fi
done

if $json_flag && [ -n "$json_entries" ]; then
    timestamp="$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || date -u '+%Y-%m-%dT%H:%M:%SZ')"
    printf '{"timestamp":"%s","profiles":[%s]}\n' "$timestamp" "$json_entries" > "$json_file"
    echo "size-report written to ${json_file}" >&2
fi
