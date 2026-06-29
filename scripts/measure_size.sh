#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: scripts/measure_size.sh [nano|micro|embedded|full-featured|all]

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
    nano|micro|embedded|full-featured)
        profiles="$profile_arg"
        ;;
    all)
        profiles="nano micro embedded full-featured"
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
                -DMICRO_OPCUA_SECURITY=OFF
                -DMICRO_OPCUA_SUBSCRIPTIONS=OFF
                -DMICRO_OPCUA_CUSTOM_METHODS=OFF
                -DMICRO_OPCUA_SERVER_DIAGNOSTICS=OFF
                -DMICRO_OPCUA_DYNAMIC_NODES=OFF
                -DMICRO_OPCUA_USER_AUTH=OFF
                -DMICRO_OPCUA_SERVICE_WRITE=OFF
                -DMICRO_OPCUA_MULTIPLE_CONNECTIONS=OFF
                -DMICRO_OPCUA_EVENTS=OFF
                -DMICRO_OPCUA_SERVICE_HISTORY=OFF
                -DMICRO_OPCUA_SERVICE_QUERY=OFF
                -DMICRO_OPCUA_SERVICE_NODEMANAGEMENT=OFF
                -DMICRO_OPCUA_PUBSUB=OFF
            )
            ;;
        micro)
            defs=(
                -DMICRO_OPCUA_SECURITY=OFF
                -DMICRO_OPCUA_SUBSCRIPTIONS=ON
                -DMICRO_OPCUA_CUSTOM_METHODS=OFF
                -DMICRO_OPCUA_SERVER_DIAGNOSTICS=OFF
                -DMICRO_OPCUA_DYNAMIC_NODES=OFF
                -DMICRO_OPCUA_USER_AUTH=ON
                -DMICRO_OPCUA_SERVICE_WRITE=ON
                -DMICRO_OPCUA_MULTIPLE_CONNECTIONS=OFF
                -DMICRO_OPCUA_EVENTS=OFF
                -DMICRO_OPCUA_SERVICE_HISTORY=OFF
                -DMICRO_OPCUA_SERVICE_QUERY=OFF
                -DMICRO_OPCUA_SERVICE_NODEMANAGEMENT=OFF
                -DMICRO_OPCUA_PUBSUB=OFF
            )
            ;;
        embedded)
            defs=(
                -DMICRO_OPCUA_EMBEDDED_PROFILE=ON
                -DMU_MAX_SUBSCRIPTIONS=2
                -DMU_MAX_MONITORED_ITEMS=100
                -DMU_MAX_PUBLISH_REQUESTS=5
                -DMU_MONITORED_QUEUE_DEPTH=2
                -DMU_MAX_TRIGGER_LINKS=4
                -DMICRO_OPCUA_CUSTOM_METHODS=OFF
                -DMICRO_OPCUA_SERVER_DIAGNOSTICS=OFF
                -DMICRO_OPCUA_DYNAMIC_NODES=OFF
                -DMICRO_OPCUA_USER_AUTH=ON
                -DMICRO_OPCUA_SERVICE_WRITE=ON
                -DMICRO_OPCUA_MULTIPLE_CONNECTIONS=ON
                -DMICRO_OPCUA_EVENTS=ON
            )
            ;;
        full-featured)
            defs=(
                -DMICRO_OPCUA_EMBEDDED_PROFILE=ON
                -DMU_MAX_SUBSCRIPTIONS=2
                -DMU_MAX_MONITORED_ITEMS=100
                -DMU_MAX_PUBLISH_REQUESTS=5
                -DMU_MONITORED_QUEUE_DEPTH=2
                -DMU_MAX_TRIGGER_LINKS=4
                -DMICRO_OPCUA_CUSTOM_METHODS=ON
                -DMICRO_OPCUA_SERVER_DIAGNOSTICS=ON
                -DMICRO_OPCUA_DYNAMIC_NODES=ON
                -DMICRO_OPCUA_USER_AUTH=ON
                -DMICRO_OPCUA_SERVICE_WRITE=ON
                -DMICRO_OPCUA_MULTIPLE_CONNECTIONS=ON
                -DMICRO_OPCUA_EVENTS=ON
            )
            ;;
    esac

    build_dir="$build_root/$profile"
    archive="$build_dir/src/libmicro_opcua.a"

    cmake -S . -B "$build_dir" \
        -DCMAKE_SYSTEM_NAME=Generic \
        -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_C_FLAGS="-mcpu=cortex-m0plus -mthumb" \
        -DMICRO_OPCUA_PLATFORM=arduino-skeleton \
        -DMICRO_OPCUA_OPTIMIZE_SIZE=ON \
        "${defs[@]}" >/dev/null
    cmake --build "$build_dir" --target micro_opcua >/dev/null

    arm-none-eabi-size -t "$archive" | awk -v profile="$profile" -v archive="$archive" '
        /\(TOTALS\)$/ {
            printf "%-9s %10d %8d %8d %10d %s\n", profile, $1, $2, $3, $4, archive
        }
    '
done
