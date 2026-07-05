#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: scripts/measure_size.sh [nano|micro|standard|embedded|full|all]

Cross-compiles the portable core with arm-none-eabi-gcc for RP2040/Cortex-M0+
Thumb code and reports the archive section totals used by the size ledger.
Run with no arguments for a host-test smoke check that does not require the ARM
toolchain.
USAGE
}

if [ "$#" -eq 0 ]; then
    usage
    exit 0
fi

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    usage
    exit 0
fi

profile_arg="$1"
case "$profile_arg" in
    nano|micro|standard|embedded|full-featured|full)
        profiles="$profile_arg"
        ;;
    all)
        profiles="nano micro standard embedded full"
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
command -v arm-none-eabi-gcc >/dev/null 2>&1 || {
    echo "arm-none-eabi-gcc not found" >&2
    exit 127
}
command -v arm-none-eabi-size >/dev/null 2>&1 || {
    echo "arm-none-eabi-size not found" >&2
    exit 127
}

build_root="${BUILD_ROOT:-build/size-arm}"

printf '%-14s %10s %8s %8s %10s %s\n' "profile" "text" "data" "bss" "dec" "archive"

for profile in $profiles; do
    case "$profile" in
        nano)
            defs=(
                -DMUC_OPCUA_PROFILE=nano
            )
            ;;
        micro)
            defs=(
                -DMUC_OPCUA_PROFILE=micro
                -DMU_MAX_MONITORED_ITEMS=8
            )
            ;;
        standard)
            defs=(
                -DMUC_OPCUA_PROFILE=standard
                -DMUC_OPCUA_STANDARD_PROFILE=ON
                -DMU_MAX_SUBSCRIPTIONS=50
                -DMU_MAX_MONITORED_ITEMS=1000
                -DMU_MAX_PUBLISH_REQUESTS=50
                -DMU_MONITORED_QUEUE_DEPTH=2
                -DMU_MAX_TRIGGER_LINKS=4
            )
            ;;
        embedded)
            defs=(
                -DMUC_OPCUA_PROFILE=embedded
                -DMU_MAX_SUBSCRIPTIONS=2
                -DMU_MAX_MONITORED_ITEMS=100
                -DMU_MAX_PUBLISH_REQUESTS=5
                -DMU_MONITORED_QUEUE_DEPTH=2
                -DMU_MAX_TRIGGER_LINKS=4
            )
            ;;
        full-featured|full)
            defs=(
                -DMUC_OPCUA_PROFILE=full
                -DMU_MAX_SUBSCRIPTIONS=100
                -DMU_MAX_MONITORED_ITEMS=2000
                -DMU_MAX_PUBLISH_REQUESTS=100
                -DMU_MONITORED_QUEUE_DEPTH=2
                -DMU_MAX_TRIGGER_LINKS=4
            )
            ;;
    esac

    build_dir="$build_root/$profile"
    archive="$build_dir/src/libmuc_opcua.a"

    cmake -S . -B "$build_dir" \
        -DCMAKE_SYSTEM_NAME=Generic \
        -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_C_FLAGS="-mcpu=cortex-m0plus -mthumb" \
        -DMUC_OPCUA_PLATFORM=arduino-skeleton \
        -DMUC_OPCUA_OPTIMIZE_SIZE=ON \
        "${defs[@]}" >/dev/null
    cmake --build "$build_dir" --target muc_opcua >/dev/null

    arm-none-eabi-size -t "$archive" | awk -v profile="$profile" -v archive="$archive" '
        /\(TOTALS\)$/ {
            printf "%-9s %10d %8d %8d %10d %s\n", profile, $1, $2, $3, $4, archive
        }
    '
done
