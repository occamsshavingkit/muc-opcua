#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd "$(dirname "$0")" && pwd -P)
ROOT_DIR=$(CDPATH= cd "$SCRIPT_DIR/.." && pwd -P)
fixture_root=$(mktemp -d "${TMPDIR:-/tmp}/muc-opcua-matrix-discovery.XXXXXX")
failures=0

cleanup() {
    rm -rf "$fixture_root"
}
trap cleanup EXIT

mkdir -p "$fixture_root/scripts" "$fixture_root/cmake"
cp "$ROOT_DIR/scripts/check_build_matrix.sh" "$fixture_root/scripts/check_build_matrix.sh"

cat > "$fixture_root/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.20)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/Kconfig" _MUC_OPCUA_KCONFIG_SYMBOL_LINES REGEX "^(menu)?config[ \t]+MUC_OPCUA_(PROFILE|FACET|CU)_[A-Za-z0-9_]+$")
set(MUC_OPCUA_KCONFIG_FEATURES READ_CACHE SECURE_CHANNEL_CRYPTO)
foreach(_line IN LISTS _MUC_OPCUA_KCONFIG_SYMBOL_LINES)
    string(REGEX REPLACE "^(menu)?config[ \t]+" "" _sym "${_line}")
    list(APPEND MUC_OPCUA_KCONFIG_FEATURES "${_sym}")
endforeach()
EOF

cat > "$fixture_root/Kconfig" <<'EOF'
  config MUC_OPCUA_PROFILE_FIXTURE_SERVER
    bool "Fixture profile"

    menuconfig MUC_OPCUA_FACET_FIXTURE_SERVER
        bool "Fixture facet"

  config MUC_OPCUA_CU_FIXTURE_BEHAVIOR
    bool "Fixture CU"

    config MUC_OPCUA_MAX_SESSIONS
        int "Typed capacity"
EOF

python3 - "$fixture_root/scripts/check_build_matrix.sh" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
source = path.read_text(encoding="utf-8")
entrypoint = 'main "$@"\n'
if not source.endswith(entrypoint):
    raise SystemExit("matrix script entrypoint changed; update discovery harness")
path.write_text(
    source[: -len(entrypoint)]
    + 'discover_toggles\nprintf \'%s\\n\' "${toggles[@]}"\n',
    encoding="utf-8",
)
PY

actual=$(bash "$fixture_root/scripts/check_build_matrix.sh")
expected=$(cat <<'EOF'
MUC_OPCUA_READ_CACHE
MUC_OPCUA_SECURE_CHANNEL_CRYPTO
MUC_OPCUA_PROFILE_FIXTURE_SERVER
MUC_OPCUA_FACET_FIXTURE_SERVER
MUC_OPCUA_CU_FIXTURE_BEHAVIOR
EOF
)

if [ "$actual" != "$expected" ]; then
    printf 'expected discovered toggles:\n%s\n\nactual discovered toggles:\n%s\n' \
        "$expected" "$actual" >&2
    failures=$((failures + 1))
fi

cp "$ROOT_DIR/scripts/check_build_matrix.sh" "$fixture_root/scripts/check_build_matrix.sh"
cat > "$fixture_root/Kconfig" <<'EOF'
config MUC_OPCUA_MAX_SESSIONS
	int "Typed capacity"
EOF

set +e
zero_discovery_output=$(
    CMAKE=/bin/true NM=/bin/false BUILD_DIR="$fixture_root/build" \
        bash "$fixture_root/scripts/check_build_matrix.sh" 2>&1
)
zero_discovery_status=$?
set -e

if [ "$zero_discovery_status" -ne 2 ]; then
    printf 'zero-discovery exit status: expected 2, got %s\n' "$zero_discovery_status" >&2
    failures=$((failures + 1))
fi

case "$zero_discovery_output" in
    *"error: zero supported bool Kconfig toggles discovered"*) ;;
    *)
        echo "zero-discovery fixture did not report a fatal discovery error" >&2
        failures=$((failures + 1))
        ;;
esac

if [ "$failures" -ne 0 ]; then
    exit 1
fi

echo "build matrix discovery regression: PASS"
