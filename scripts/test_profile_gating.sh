#!/usr/bin/env bash
# scripts/test_profile_gating.sh
#
# Behavioral verification for the Kconfig-driven feature gating (CMakeLists.txt +
# /Kconfig, resolved by scripts/kconfig/gen_config.py). Checks that: every named
# profile produces its documented feature set; a per-flag -DMUC_OPCUA_<X>=ON/OFF
# subtracts/adds against the profile default AND actually drops/adds the code; an
# override that violates a `depends on` CASCADES its dependents off (Kconfig's model)
# rather than erroring; switching profiles fully re-derives; and the menuconfig
# tooling targets exist. Feature values are read from the generated
# muc_opcua_config.cmake (they are no longer CMake cache vars). See
# docs/build-and-gating.md for the semantics this script checks.
#
# Not wired into CTest: each scenario is a full `cmake` configure. Run by hand or as a
# dedicated CI job, like scripts/measure_size.sh.
set -u
cd "$(dirname "$0")/.."

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT
PASS=0
FAIL=0

# assert_cfg <build-dir> <SYM> <ON|OFF>   -- checks muc_opcua_config.cmake
assert_cfg() {
    local dir="$1" sym="$2" expected="$3" actual
    actual=$(grep -E "^set\(MUC_OPCUA_${sym} (ON|OFF)\)\$" "$dir/muc_opcua_config.cmake" 2>/dev/null | sed -E 's/.* (ON|OFF)\)/\1/')
    if [ "$actual" = "$expected" ]; then
        echo "  PASS  MUC_OPCUA_$sym=$actual (expected $expected)"
        PASS=$((PASS + 1))
    else
        echo "  FAIL  MUC_OPCUA_$sym=$actual (expected $expected) [$dir]"
        FAIL=$((FAIL + 1))
    fi
}

echo "### 1. 'standard' baseline feature set ###"
D1="$WORKDIR/g1"
cmake -S . -B "$D1" -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D1" SECURITY ON
assert_cfg "$D1" SUBSCRIPTIONS_STANDARD ON
assert_cfg "$D1" DATA_ACCESS OFF   # optional facet -- full-only after spec 067
assert_cfg "$D1" ECC OFF

echo "### 2. Subtraction: standard minus encryption (-DMUC_OPCUA_SECURITY=OFF) ###"
D2="$WORKDIR/g2"
cmake -S . -B "$D2" -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_SECURITY=OFF -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D2" SECURITY OFF
assert_cfg "$D2" SUBSCRIPTIONS_STANDARD ON   # everything else stays profile-default
echo "  -- confirm the subtraction drops the code, not just the flag --"
cmake --build "$D2" -j4 >/dev/null 2>&1
if nm "$D2/src/libmuc_opcua.a" 2>/dev/null | grep -q "mu_sym_chunk_wrap"; then
    echo "  FAIL  mu_sym_chunk_wrap still present with SECURITY=OFF"; FAIL=$((FAIL + 1))
else
    echo "  PASS  mu_sym_chunk_wrap absent with SECURITY=OFF"; PASS=$((PASS + 1))
fi

echo "### 3. Dependency CASCADE (Kconfig): full minus BASE_NODES drops its dependents, no error ###"
D3="$WORKDIR/g3"
cmake -S . -B "$D3" -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_BASE_NODES=OFF -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D3" BASE_NODES OFF
assert_cfg "$D3" BASE_TYPE_SYSTEM OFF
assert_cfg "$D3" DATA_ACCESS OFF
assert_cfg "$D3" NAMESPACES OFF

echo "### 3b. ECC without SECURITY cascades ECC off (ECC depends on SECURITY) ###"
D3B="$WORKDIR/g3b"
cmake -S . -B "$D3B" -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_SECURITY=OFF -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D3B" SECURITY OFF
assert_cfg "$D3B" ECC OFF

echo "### 4. Add to a lean profile: nano + subscriptions ###"
D4="$WORKDIR/g4"
cmake -S . -B "$D4" -DMUC_OPCUA_PROFILE=nano -DMUC_OPCUA_SUBSCRIPTIONS=ON -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D4" SUBSCRIPTIONS ON
assert_cfg "$D4" SECURITY OFF   # not pulled in

echo "### 5. Profile switch in an EXISTING build dir (full -> nano) fully re-derives ###"
D5="$WORKDIR/g5"
cmake -S . -B "$D5" -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D5" ECC ON
cmake -S . -B "$D5" -DMUC_OPCUA_PROFILE=nano >/dev/null 2>&1
assert_cfg "$D5" ECC OFF
assert_cfg "$D5" SUBSCRIPTIONS OFF

echo "### 6. 'custom' profile -- minimal (only the always-on core services) ###"
D6="$WORKDIR/g6"
cmake -S . -B "$D6" -DMUC_OPCUA_PROFILE=custom -DMUC_OPCUA_SUBSCRIPTIONS=ON -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D6" SUBSCRIPTIONS ON
assert_cfg "$D6" SECURITY OFF

echo "### 7. menuconfig / savedefconfig tooling targets exist ###"
if cmake --build "$D1" --target help 2>/dev/null | grep -q "menuconfig"; then
    echo "  PASS  menuconfig target present"; PASS=$((PASS + 1))
else
    echo "  FAIL  menuconfig target missing"; FAIL=$((FAIL + 1))
fi

echo
echo "===================="
echo "$PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
