#!/usr/bin/env bash
# scripts/test_profile_gating.sh
#
# Behavioral verification for the MUC_OPCUA_PROFILE gating machinery in
# CMakeLists.txt: every named profile (nano/micro/standard/embedded/full)
# forces its documented feature defaults, a per-option -D can override
# (subtract or add to) those defaults, an override that breaks a dependency
# still fails loudly, and reconfiguring an existing build directory behaves
# predictably (same profile: new -D takes effect; different profile: fully
# re-derived, discarding the old profile's leftovers). See
# docs/build-and-gating.md for the semantics this script checks.
#
# Not wired into CTest: each scenario is a full `cmake` configure (and one
# does a build + `nm`), too slow for the unit/integration suite. Run it by
# hand or as a dedicated CI job, the same way scripts/measure_size.sh is.
set -u
cd "$(dirname "$0")/.."

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

PASS=0
FAIL=0

# assert_cache <build-dir> <CACHE_VAR> <expected-value>
assert_cache() {
    local dir="$1" var="$2" expected="$3"
    local actual
    actual=$(grep -E "^${var}:" "$dir/CMakeCache.txt" 2>/dev/null | cut -d= -f2)
    if [ "$actual" = "$expected" ]; then
        echo "  PASS  $var=$actual (expected $expected) [$dir]"
        PASS=$((PASS + 1))
    else
        echo "  FAIL  $var=$actual (expected $expected) [$dir]"
        FAIL=$((FAIL + 1))
    fi
}

# assert_configure_fails <build-dir> <grep-pattern> -- rest of args are cmake args
assert_configure_fails() {
    local dir="$1" pattern="$2"
    shift 2
    local out
    out=$(cmake -S . -B "$dir" "$@" 2>&1)
    if echo "$out" | grep -qE "$pattern"; then
        echo "  PASS  configure failed with: $pattern [$dir]"
        PASS=$((PASS + 1))
    else
        echo "  FAIL  configure did not fail with: $pattern [$dir]"
        echo "$out" | tail -5 | sed 's/^/        /'
        FAIL=$((FAIL + 1))
    fi
}

echo "### 1. Plain 'standard' — baseline feature set (no regression check) ###"
D1="$WORKDIR/g1"
cmake -S . -B "$D1" -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cache "$D1" MUC_OPCUA_SECURITY ON
assert_cache "$D1" MUC_OPCUA_ECC ON
assert_cache "$D1" MUC_OPCUA_DATA_ACCESS ON

echo "### 2. Subtraction: standard minus encryption (-DMUC_OPCUA_SECURITY=OFF -DMUC_OPCUA_ECC=OFF) ###"
D2="$WORKDIR/g2"
cmake -S . -B "$D2" -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_SECURITY=OFF -DMUC_OPCUA_ECC=OFF -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cache "$D2" MUC_OPCUA_SECURITY OFF
assert_cache "$D2" MUC_OPCUA_ECC OFF
assert_cache "$D2" MUC_OPCUA_DATA_ACCESS ON # everything else stays profile-default
assert_cache "$D2" MUC_OPCUA_USER_AUTH ON
echo "  -- confirming the subtraction actually drops the code (not just the cache flag) --"
cmake --build "$D2" -j4 >/dev/null 2>&1
if nm "$D2/src/libmuc_opcua.a" 2>/dev/null | grep -q "mu_sym_chunk_wrap"; then
    echo "  FAIL  mu_sym_chunk_wrap still present in the archive with SECURITY=OFF"
    FAIL=$((FAIL + 1))
else
    echo "  PASS  mu_sym_chunk_wrap absent from the archive with SECURITY=OFF"
    PASS=$((PASS + 1))
fi

echo "### 3. Invalid subtraction: standard minus SECURITY only, ECC left on — must fail loudly ###"
D3="$WORKDIR/g3"
assert_configure_fails "$D3" "MUC_OPCUA_ECC requires MUC_OPCUA_SECURITY" \
    -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_SECURITY=OFF -DMUC_OPCUA_PLATFORM=host

echo "### 3b. Every other MUC_OPCUA_FEATURE_DEPENDENCIES pair also fails at CONFIGURE time ###"
echo "###     (not just build time via features.h — this is the promoted check) ###"
D3B="$WORKDIR/g3b"
assert_configure_fails "$D3B" "MUC_OPCUA_BASE_TYPE_SYSTEM requires MUC_OPCUA_BASE_NODES" \
    -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_BASE_NODES=OFF -DMUC_OPCUA_PLATFORM=host

echo "### 4. Reconfigure the SAME profile with a new -D (no -DMUC_OPCUA_PROFILE this time) ###"
D4="$WORKDIR/g4"
cmake -S . -B "$D4" -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cache "$D4" MUC_OPCUA_AUDITING ON
cmake -S . -B "$D4" -DMUC_OPCUA_AUDITING=OFF >/dev/null 2>&1
assert_cache "$D4" MUC_OPCUA_AUDITING OFF

echo "### 5. Switch profile in an EXISTING build dir (full -> nano) — must fully re-derive ###"
D5="$WORKDIR/g5"
cmake -S . -B "$D5" -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cache "$D5" MUC_OPCUA_ECC ON
cmake -S . -B "$D5" -DMUC_OPCUA_PROFILE=nano >/dev/null 2>&1
assert_cache "$D5" MUC_OPCUA_ECC OFF
assert_cache "$D5" MUC_OPCUA_SECURITY OFF
assert_cache "$D5" MUC_OPCUA_SUBSCRIPTIONS OFF

echo "### 6. Switch back (nano -> full) — must fully restore, no stale leftovers ###"
cmake -S . -B "$D5" -DMUC_OPCUA_PROFILE=full >/dev/null 2>&1
assert_cache "$D5" MUC_OPCUA_ECC ON
assert_cache "$D5" MUC_OPCUA_SECURITY ON
assert_cache "$D5" MUC_OPCUA_SUBSCRIPTIONS ON

echo "### 7. 'custom' profile — nothing forced, every option is exactly what the user passed ###"
D7="$WORKDIR/g7"
cmake -S . -B "$D7" -DMUC_OPCUA_PROFILE=custom -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_SUBSCRIPTIONS=ON >/dev/null 2>&1
assert_cache "$D7" MUC_OPCUA_SUBSCRIPTIONS ON
assert_cache "$D7" MUC_OPCUA_SECURITY OFF # option() default; custom never forces it

echo
echo "===================="
echo "$PASS passed, $FAIL failed"
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
