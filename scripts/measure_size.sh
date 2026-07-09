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
    # Spec 056: the profile string is the only capacity input. capacities.h
    # resolves every MU_INTERN_* from the MUC_OPCUA_PROFILE_<NAME> marker CMake
    # emits, so no per-capacity -D is needed here. (An integrator measuring an
    # override would add -DMU_MAX_*=<n>, which still wins via the cascade.)
    local profile="$1"
    case "$profile" in
        full-featured) profile="full" ;;
    esac
    echo "-DMUC_OPCUA_PROFILE=${profile}"
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
    # LTO is the default build mode (MUC_OPCUA_LTO=ON). -ffat-lto-objects keeps the
    # archive .text measurable (per-object machine code) while -flto lets the final
    # ELF link do cross-module optimization -- the elf_text column is the real,
    # smallest deployed server size; archive .text is the conservative all-code figure.
    local cflags="-mcpu=cortex-m0plus -mthumb -flto -ffat-lto-objects"

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

# LTO is baked into the default cflags above, so a single measurement per profile
# already reflects the shipped (smallest) build. The legacy --lto flag is accepted
# for back-compat but is now a no-op.
for profile in $profiles; do
    build_and_measure "$profile" false
done

if $json_flag && [ -n "$json_entries" ]; then
    timestamp="$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || date -u '+%Y-%m-%dT%H:%M:%SZ')"
    printf '{"timestamp":"%s","profiles":[%s]}\n' "$timestamp" "$json_entries" > "$json_file"
    echo "size-report written to ${json_file}" >&2
fi
