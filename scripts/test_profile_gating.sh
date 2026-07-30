#!/usr/bin/env bash
# scripts/test_profile_gating.sh
#
# Behavioral verification for the Kconfig-driven feature gating (CMakeLists.txt +
# /Kconfig, resolved by scripts/kconfig/gen_config.py). Checks that: every named
# profile produces its documented feature set using the 2025 OPC symbol names;
# a per-flag -DMUC_OPCUA_<X>=ON/OFF subtracts/adds against the profile default
# AND actually drops/adds the code; an override that violates a `depends on`
# CASCADES its dependents off (Kconfig's model) rather than erroring; switching
# profiles fully re-derives; the menuconfig tooling targets exist; capacity int
# symbols resolve to profile defaults; and legacy -DMU_MAX_*=N overrides
# translate into Kconfig. Feature values are read from the generated
# muc_opcua_config.cmake (they are no longer CMake cache vars). Capacity values
# are read from the generated .config. See docs/build-and-gating.md for the
# semantics this script checks.
#
# Not wired into CTest: each scenario is a full `cmake` configure. Run by hand or as a
# dedicated CI job, like scripts/measure_size.sh.
set -u
cd "$(dirname "$0")/.."

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT
PASS=0
FAIL=0

# assert_cfg <build-dir> <SYM> <ON|OFF|UNDEFINED> -- checks muc_opcua_config.cmake
assert_cfg() {
    local dir="$1" sym="$2" expected="$3" actual
    if [ "$expected" = "UNDEFINED" ]; then
        if grep -qE "^set\(MUC_OPCUA_${sym} " "$dir/muc_opcua_config.cmake" 2>/dev/null; then
            echo "  FAIL  MUC_OPCUA_$sym is defined (expected Kconfig-undefined) [$dir]"
            FAIL=$((FAIL + 1))
        else
            echo "  PASS  MUC_OPCUA_$sym is absent from Kconfig-resolved output"
            PASS=$((PASS + 1))
        fi
        return
    fi
    actual=$(grep -E "^set\(MUC_OPCUA_${sym} (ON|OFF)\)\$" "$dir/muc_opcua_config.cmake" 2>/dev/null | sed -E 's/.* (ON|OFF)\)/\1/')
    if [ "$actual" = "$expected" ]; then
        echo "  PASS  MUC_OPCUA_$sym=$actual (expected $expected)"
        PASS=$((PASS + 1))
    else
        echo "  FAIL  MUC_OPCUA_$sym=$actual (expected $expected) [$dir]"
        FAIL=$((FAIL + 1))
    fi
}

echo "### 1. 'standard' baseline: profile symbol, cascade, facets, CUs ###"
D1="$WORKDIR/g1"
cmake -S . -B "$D1" -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
# Profile choice symbol + cascade internals
assert_cfg "$D1" PROFILE_STANDARD_2025_UA_SERVER ON
assert_cfg "$D1" PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER OFF
assert_cfg "$D1" INTERN_PROFILE_STANDARD_2025_UA_SERVER ON
assert_cfg "$D1" INTERN_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER ON
# Facets: Core 2022 on for all; embedded facets on from embedded up; X509 on from standard up
assert_cfg "$D1" FACET_CORE_2022_SERVER ON
assert_cfg "$D1" FACET_EXPOSES_TYPE_SYSTEM_SERVER ON
assert_cfg "$D1" FACET_STANDARD_DATACHANGE_SUBSCRIPTION_2022_SERVER ON
assert_cfg "$D1" FACET_GLOBAL_CERTIFICATE_MANAGEMENT_SERVER ON
assert_cfg "$D1" FACET_USER_TOKEN_USER_NAME_PASSWORD_SERVER ON
assert_cfg "$D1" FACET_USER_TOKEN_X509_CERTIFICATE_SERVER ON
# CUs: Attribute Read etc. on for all; full-only CUs off
assert_cfg "$D1" CU_ATTRIBUTE_READ ON
assert_cfg "$D1" CU_VIEW_BASIC_TRANSLATEBROWSEPATH ON
assert_cfg "$D1" CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS ON
assert_cfg "$D1" CU_VIEW_REGISTERNODES ON
assert_cfg "$D1" CU_CORE_2017_ATTRIBUTE_WRITE OFF
assert_cfg "$D1" CU_HISTORICAL_ACCESS_SERVER_FACET OFF
assert_cfg "$D1" CU_NODEMANAGEMENT OFF
assert_cfg "$D1" CU_QUERY OFF
# CU: Subscription basic on (micro+); CU: Security ECC off (full only)
assert_cfg "$D1" CU_SUBSCRIPTION_BASIC ON
assert_cfg "$D1" CU_SECURITY_ECC OFF
# Secure-channel crypto gate (spec 072): on for security-capable profiles (micro+)
assert_cfg "$D1" SECURE_CHANNEL_CRYPTO ON

echo "### 2. 'nano' baseline: profile symbol, cascade, minimal facets, CUs ###"
D2="$WORKDIR/g2"
cmake -S . -B "$D2" -DMUC_OPCUA_PROFILE=nano -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
# Profile choice symbol + cascade internals
assert_cfg "$D2" PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER ON
assert_cfg "$D2" INTERN_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER ON
assert_cfg "$D2" INTERN_PROFILE_EMBEDDED_2025_UA_SERVER OFF
assert_cfg "$D2" INTERN_PROFILE_STANDARD_2025_UA_SERVER OFF
# Facets: only Core 2022 on; embedded-level facets off
assert_cfg "$D2" FACET_CORE_2022_SERVER ON
assert_cfg "$D2" FACET_EXPOSES_TYPE_SYSTEM_SERVER OFF
assert_cfg "$D2" FACET_STANDARD_DATACHANGE_SUBSCRIPTION_2022_SERVER OFF
assert_cfg "$D2" FACET_GLOBAL_CERTIFICATE_MANAGEMENT_SERVER OFF
assert_cfg "$D2" FACET_USER_TOKEN_USER_NAME_PASSWORD_SERVER OFF
assert_cfg "$D2" FACET_USER_TOKEN_X509_CERTIFICATE_SERVER OFF
# CUs: base CUs on (Attribute Read etc.); full-only off
assert_cfg "$D2" CU_ATTRIBUTE_READ ON
assert_cfg "$D2" CU_VIEW_BASIC_TRANSLATEBROWSEPATH ON
assert_cfg "$D2" CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS ON
assert_cfg "$D2" CU_VIEW_REGISTERNODES ON
assert_cfg "$D2" CU_CORE_2017_ATTRIBUTE_WRITE OFF
# CU: Subscription basic off (micro+); CU: Security ECC off (full only)
assert_cfg "$D2" CU_SUBSCRIPTION_BASIC OFF
assert_cfg "$D2" CU_SECURITY_ECC OFF
assert_cfg "$D2" CU_AUDITING OFF
# Secure-channel crypto gate (spec 072): OFF for the SecurityPolicy-None-only nano
assert_cfg "$D2" SECURE_CHANNEL_CRYPTO OFF
cmake --build "$D2" -j4 >/dev/null 2>&1
if nm "$D2/src/libmuc_opcua.a" 2>/dev/null | grep -qE \
    'mu_raise_audit_event|mu_audit_pool_store|mu_server_set_audit_callback'; then
    echo "  FAIL  audit emission/routing symbols present with CU_AUDITING=OFF"
    FAIL=$((FAIL + 1))
else
    echo "  PASS  audit emission/routing symbols absent with CU_AUDITING=OFF"
    PASS=$((PASS + 1))
fi

echo "### 3. 'embedded' baseline: profile symbol, cascade, facets, CUs ###"
D3="$WORKDIR/g3"
cmake -S . -B "$D3" -DMUC_OPCUA_PROFILE=embedded -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
# Profile choice symbol + cascade internals
assert_cfg "$D3" PROFILE_EMBEDDED_2025_UA_SERVER ON
assert_cfg "$D3" INTERN_PROFILE_EMBEDDED_2025_UA_SERVER ON
assert_cfg "$D3" INTERN_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER ON
assert_cfg "$D3" INTERN_PROFILE_STANDARD_2025_UA_SERVER OFF
# Facets: Core 2022 on; embedded facets on; X509 off (standard+)
assert_cfg "$D3" FACET_CORE_2022_SERVER ON
assert_cfg "$D3" FACET_EXPOSES_TYPE_SYSTEM_SERVER ON
assert_cfg "$D3" FACET_STANDARD_DATACHANGE_SUBSCRIPTION_2022_SERVER ON
assert_cfg "$D3" FACET_GLOBAL_CERTIFICATE_MANAGEMENT_SERVER ON
assert_cfg "$D3" FACET_USER_TOKEN_USER_NAME_PASSWORD_SERVER ON
assert_cfg "$D3" FACET_USER_TOKEN_X509_CERTIFICATE_SERVER OFF
# CUs: base CUs on; full-only off
assert_cfg "$D3" CU_ATTRIBUTE_READ ON
assert_cfg "$D3" CU_VIEW_BASIC_TRANSLATEBROWSEPATH ON
assert_cfg "$D3" CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS ON
assert_cfg "$D3" CU_VIEW_REGISTERNODES ON
assert_cfg "$D3" CU_HISTORICAL_ACCESS_SERVER_FACET OFF
assert_cfg "$D3" CU_NODEMANAGEMENT OFF
# CU: Subscription basic on (micro+); CU: Security ECC off (full only)
assert_cfg "$D3" CU_SUBSCRIPTION_BASIC ON
assert_cfg "$D3" CU_SECURITY_ECC OFF
# Secure-channel crypto gate (spec 072): on for embedded (security-capable)
assert_cfg "$D3" SECURE_CHANNEL_CRYPTO ON

echo "### 4. Subtraction: standard minus CU_SUBSCRIPTION_BASIC drops the code ###"
D4="$WORKDIR/g4"
cmake -S . -B "$D4" -DMUC_OPCUA_PROFILE=standard \
    -DMUC_OPCUA_CU_SUBSCRIPTION_BASIC=OFF \
    -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D4" CU_SUBSCRIPTION_BASIC OFF
assert_cfg "$D4" PROFILE_STANDARD_2025_UA_SERVER ON   # profile choice preserved
assert_cfg "$D4" FACET_CORE_2022_SERVER ON   # everything else stays profile-default
echo "  -- confirm the subtraction drops the code, not just the flag --"
cmake --build "$D4" -j4 >/dev/null 2>&1
if nm "$D4/src/libmuc_opcua.a" 2>/dev/null | grep -q "publish_due"; then
    echo "  FAIL  publish_due still present with CU_SUBSCRIPTION_BASIC=OFF"; FAIL=$((FAIL + 1))
else
    echo "  PASS  publish_due absent with CU_SUBSCRIPTION_BASIC=OFF"; PASS=$((PASS + 1))
fi

echo "### 5. Dependency CASCADE: turning off FACET_CORE_2022_SERVER cascades CUs off (if block) ###"
D5="$WORKDIR/g5"
printf 'MUC_OPCUA_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES=y\n# MUC_OPCUA_FACET_CORE_2022_SERVER is not set\n' > "$WORKDIR/cascade_facet.config"
cmake -S . -B "$D5" -DMUC_OPCUA_KCONFIG_CONFIG="$WORKDIR/cascade_facet.config" -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D5" FACET_CORE_2022_SERVER OFF
assert_cfg "$D5" CU_ATTRIBUTE_READ OFF
assert_cfg "$D5" CU_VIEW_BASIC_TRANSLATEBROWSEPATH OFF
assert_cfg "$D5" CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS OFF

echo "### 5b. CU_SUBSCRIPTION_STANDARD follows its CU_SUBSCRIPTION_BASIC dependency ###"
D5B="$WORKDIR/g5b"
cmake -S . -B "$D5B" -DMUC_OPCUA_PROFILE=full \
    -DMUC_OPCUA_CU_SUBSCRIPTION_BASIC=OFF \
    -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D5B" CU_SUBSCRIPTION_BASIC OFF
# Kconfig declares CU_SUBSCRIPTION_STANDARD depends on BASIC.
if grep -q "^set(MUC_OPCUA_CU_SUBSCRIPTION_STANDARD OFF)" "$D5B/muc_opcua_config.cmake" 2>/dev/null; then
    echo "  PASS  CU_SUBSCRIPTION_STANDARD follows its CU_SUBSCRIPTION_BASIC dependency"
    PASS=$((PASS + 1))
else
    echo "  FAIL  CU_SUBSCRIPTION_STANDARD unexpectedly cascaded OFF"
    FAIL=$((FAIL + 1))
fi

echo "### 6. Add to a lean profile: nano + FACET_EXPOSES_TYPE_SYSTEM_SERVER (via .config) ###"
D6="$WORKDIR/g6"
printf 'MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER=y\nMUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER=y\n' > "$WORKDIR/nano_addfacet.config"
cmake -S . -B "$D6" -DMUC_OPCUA_KCONFIG_CONFIG="$WORKDIR/nano_addfacet.config" -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D6" FACET_EXPOSES_TYPE_SYSTEM_SERVER ON
assert_cfg "$D6" PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER ON   # profile stays nano
assert_cfg "$D6" FACET_USER_TOKEN_X509_CERTIFICATE_SERVER OFF   # not pulled in

echo "### 7. Profile switch in an EXISTING build dir (full -> nano) fully re-derives ###"
D7="$WORKDIR/g7"
cmake -S . -B "$D7" -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D7" CU_SECURITY_ECC ON
assert_cfg "$D7" FACET_USER_TOKEN_X509_CERTIFICATE_SERVER ON
cmake -S . -B "$D7" -DMUC_OPCUA_PROFILE=nano >/dev/null 2>&1
assert_cfg "$D7" CU_SECURITY_ECC OFF
assert_cfg "$D7" FACET_USER_TOKEN_X509_CERTIFICATE_SERVER OFF
assert_cfg "$D7" FACET_EXPOSES_TYPE_SYSTEM_SERVER OFF

echo "### 8. 'custom' profile -- facets default OFF (no cascade for custom) ###"
D8="$WORKDIR/g8"
cmake -S . -B "$D8" -DMUC_OPCUA_PROFILE=custom -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D8" PROFILE_CUSTOM ON
assert_cfg "$D8" FACET_CORE_2022_SERVER OFF
assert_cfg "$D8" FACET_EXPOSES_TYPE_SYSTEM_SERVER OFF
assert_cfg "$D8" FACET_USER_TOKEN_X509_CERTIFICATE_SERVER OFF
assert_cfg "$D8" CU_SECURITY_ECC OFF
assert_cfg "$D8" CU_ATTRIBUTE_READ OFF  # no cascade, facet off

echo "### 9. menuconfig / savedefconfig tooling targets exist ###"
if cmake --build "$D1" --target help 2>/dev/null | grep -q "menuconfig"; then
    echo "  PASS  menuconfig target present"; PASS=$((PASS + 1))
else
    echo "  FAIL  menuconfig target missing"; FAIL=$((FAIL + 1))
fi

echo "### 10. Capacity default (standard profile: MAX_SESSIONS=50 in .config) ###"
assert_cap() {
    local dir="$1" sym="$2" expected="$3" actual
    actual=$(grep -E "^${sym}=" "$dir/.config" 2>/dev/null | cut -d= -f2)
    if [ "$actual" = "$expected" ]; then
        echo "  PASS  $sym=$actual (expected $expected)"
        PASS=$((PASS + 1))
    else
        echo "  FAIL  $sym=$actual (expected $expected) [$dir]"
        FAIL=$((FAIL + 1))
    fi
}
assert_cap "$D1" MAX_SESSIONS 50
assert_cap "$D1" MAX_CONNECTIONS 50

echo "### 11. Capacity override (-DMU_MAX_SESSIONS=7 overrides profile default) ###"
D11="$WORKDIR/g11"
cmake -S . -B "$D11" -DMUC_OPCUA_PROFILE=standard -DMU_MAX_SESSIONS=7 -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cap "$D11" MAX_SESSIONS 7

echo "### 12. Kconfig .config capacity override reaches compile definitions ###"
D12="$WORKDIR/g12"
printf 'MUC_OPCUA_PROFILE_STANDARD_2025_UA_SERVER=y\nMAX_SESSIONS=42\n' > "$WORKDIR/custom_cap.config"
cmake -S . -B "$D12" -DMUC_OPCUA_KCONFIG_CONFIG="$WORKDIR/custom_cap.config" -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cap "$D12" MAX_SESSIONS 42
echo "  -- confirm the resolved value reaches the compiler command --"
compile_out=$(cmake --build "$D12" --target muc_opcua -- VERBOSE=1 -j1 2>&1)
if echo "$compile_out" | grep -q -- "-DMU_MAX_SESSIONS=42"; then
    echo "  PASS  -DMU_MAX_SESSIONS=42 present in compile command"
    PASS=$((PASS + 1))
else
    echo "  FAIL  -DMU_MAX_SESSIONS=42 absent from compile command"
    FAIL=$((FAIL + 1))
fi

echo "### 13. Unimplemented OPC items appear in Kconfig but cannot be selected ###"
D13="$WORKDIR/g13"
cmake -S . -B "$D13" -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
echo "  -- at least one 'NOT IMPLEMENTED' comment directive exists in Kconfig --"
if grep -q 'comment ".*NOT IMPLEMENTED' Kconfig; then
    echo "  PASS  Kconfig contains unimplemented-item comment directive(s)"
    PASS=$((PASS + 1))
else
    echo "  FAIL  Kconfig has no 'NOT IMPLEMENTED' comment directive"
    FAIL=$((FAIL + 1))
fi
echo "  -- the NOT IMPLEMENTED comment is rendered in .config (visible in menuconfig) --"
if grep -q '# .*NOT IMPLEMENTED' "$D13/.config" 2>/dev/null; then
    echo "  PASS  .config renders unimplemented-item comment(s)"
    PASS=$((PASS + 1))
else
    echo "  FAIL  .config missing 'NOT IMPLEMENTED' comment rendering"
    FAIL=$((FAIL + 1))
fi
echo "  -- the unimplemented item has no settable symbol in .config --"
if grep -qE '^MUC_OPCUA_OPC_[A-Z_]*=y' "$D13/.config" 2>/dev/null; then
    echo "  FAIL  an unimplemented OPC item appears as =y in .config (should not be selectable)"
    FAIL=$((FAIL + 1))
else
    echo "  PASS  no unimplemented OPC item is selectable in .config"
    PASS=$((PASS + 1))
fi

echo "### 14. Cross-profile activation: nano + enable a standard-only facet ###"
echo "    User can enable facets from profiles they did not select."
D14="$WORKDIR/g14"
printf 'MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER=y\nMUC_OPCUA_FACET_USER_TOKEN_X509_CERTIFICATE_SERVER=y\n' > "$WORKDIR/nano_x509.config"
cmake -S . -B "$D14" -DMUC_OPCUA_KCONFIG_CONFIG="$WORKDIR/nano_x509.config" -DMUC_OPCUA_PLATFORM=host >/dev/null 2>&1
assert_cfg "$D14" FACET_USER_TOKEN_X509_CERTIFICATE_SERVER ON
assert_cfg "$D14" PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER ON   # profile stays nano
assert_cfg "$D14" CU_CORE_2017_ATTRIBUTE_WRITE OFF   # unrelated full-only CU stays OFF

echo "### 15. Internal cascade symbols resolve correctly per profile ###"
echo "    INTERN_PROFILE_NANO is ON for all named profiles via cascade."
if grep -q "^MUC_OPCUA_INTERN_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER=y" "$D1/.config" 2>/dev/null; then
    echo "  PASS  INTERN_PROFILE_NANO=y under standard (cascade)"
    PASS=$((PASS + 1))
else
    echo "  FAIL  INTERN_PROFILE_NANO not y under standard"
    FAIL=$((FAIL + 1))
fi
if grep -q "^MUC_OPCUA_INTERN_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES=y" "$D13/.config" 2>/dev/null; then
    echo "  PASS  INTERN_PROFILE_FULL=y under full"
    PASS=$((PASS + 1))
else
    echo "  FAIL  INTERN_PROFILE_FULL not y under full"
    FAIL=$((FAIL + 1))
fi
if grep -q "^MUC_OPCUA_INTERN_PROFILE_STANDARD_2025_UA_SERVER=y" "$D13/.config" 2>/dev/null; then
    echo "  PASS  INTERN_PROFILE_STANDARD=y under full (cascade)"
    PASS=$((PASS + 1))
else
    echo "  FAIL  INTERN_PROFILE_STANDARD not y under full (cascade)"
    FAIL=$((FAIL + 1))
fi

echo "### 16. Superseded 2022 profiles are not canonical choices ###"
if grep -q "config MUC_OPCUA_PROFILE_STANDARD_2022_UA_SERVER" Kconfig; then
    echo "  FAIL  superseded Standard 2022 profile is selectable"
    FAIL=$((FAIL + 1))
else
    echo "  PASS  superseded Standard 2022 profile is not selectable"
    PASS=$((PASS + 1))
fi

echo "### 17. Aggregate-only Discovery gate builds and runs GetEndpoints coverage ###"
D17="$WORKDIR/g17"
D17_CONFIGURE_LOG="$WORKDIR/g17-configure.log"
D17_BUILD_LOG="$WORKDIR/g17-build.log"
if cmake -S . -B "$D17" -DMUC_OPCUA_PROFILE=custom \
    -DMUC_OPCUA_FACET_CORE_2022_SERVER=ON \
    -DMUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS=ON \
    -DMUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF=OFF \
    -DMUC_OPCUA_CU_DISCOVERY_GET_ENDPOINTS=OFF \
    -DMUC_OPCUA_BUILD_TESTS=ON \
    -DMUC_OPCUA_PLATFORM=host >"$D17_CONFIGURE_LOG" 2>&1; then
    assert_cfg "$D17" CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS ON
    assert_cfg "$D17" CU_DISCOVERY_FIND_SERVERS_SELF UNDEFINED
    assert_cfg "$D17" CU_DISCOVERY_GET_ENDPOINTS OFF

    if cmake --build "$D17" --target test_get_endpoints_gate -j4 >"$D17_BUILD_LOG" 2>&1; then
        if "$D17/tests/unit/test_get_endpoints_gate"; then
            echo "  PASS  test_get_endpoints_gate passes with only the aggregate Discovery gate enabled"
            PASS=$((PASS + 1))
        else
            echo "  FAIL  test_get_endpoints_gate failed with only the aggregate Discovery gate enabled"
            FAIL=$((FAIL + 1))
        fi
    else
        echo "  FAIL  could not build test_get_endpoints_gate for the aggregate-only Discovery configuration"
        cat "$D17_BUILD_LOG"
        FAIL=$((FAIL + 1))
    fi
else
    echo "  FAIL  could not configure the aggregate-only Discovery scenario"
    cat "$D17_CONFIGURE_LOG"
    FAIL=$((FAIL + 1))
fi

echo "### 18. Reverse Connect closes its handle without Multiple Connections ###"
D18="$WORKDIR/g18"
D18_CONFIGURE_LOG="$WORKDIR/g18-configure.log"
D18_BUILD_LOG="$WORKDIR/g18-build.log"
if cmake -S . -B "$D18" -DMUC_OPCUA_PROFILE=custom \
    -DMUC_OPCUA_FACET_CORE_2022_SERVER=ON \
    -DMUC_OPCUA_CU_PROTOCOL_REVERSE_CONNECT_SERVER=ON \
    -DMUC_OPCUA_CU_MULTIPLE_CONNECTIONS=OFF \
    -DMUC_OPCUA_BUILD_TESTS=ON \
    -DMUC_OPCUA_PLATFORM=host >"$D18_CONFIGURE_LOG" 2>&1; then
    assert_cfg "$D18" FACET_CORE_2022_SERVER ON
    assert_cfg "$D18" CU_PROTOCOL_REVERSE_CONNECT_SERVER ON
    assert_cfg "$D18" CU_MULTIPLE_CONNECTIONS OFF

    if cmake --build "$D18" --target test_reverse_connect -j4 >"$D18_BUILD_LOG" 2>&1; then
        if "$D18/tests/unit/test_reverse_connect"; then
            echo "  PASS  test_reverse_connect passes without Multiple Connections"
            PASS=$((PASS + 1))
        else
            echo "  FAIL  test_reverse_connect failed without Multiple Connections"
            FAIL=$((FAIL + 1))
        fi
    else
        echo "  FAIL  could not build test_reverse_connect without Multiple Connections"
        cat "$D18_BUILD_LOG"
        FAIL=$((FAIL + 1))
    fi
else
    echo "  FAIL  could not configure Reverse Connect without Multiple Connections"
    cat "$D18_CONFIGURE_LOG"
    FAIL=$((FAIL + 1))
fi

echo "### 19. Stale documented Write Value override cannot bypass canonical Kconfig gate ###"
D19="$WORKDIR/g19"
D19_CONFIGURE_LOG="$WORKDIR/g19-configure.log"
D19_BUILD_LOG="$WORKDIR/g19-build.log"
if cmake -S . -B "$D19" -DMUC_OPCUA_PROFILE=custom \
    -DMUC_OPCUA_FACET_CORE_2022_SERVER=ON \
    -DMUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE=OFF \
    -DMUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES=ON \
    -DMUC_OPCUA_PLATFORM=host \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >"$D19_CONFIGURE_LOG" 2>&1; then
    assert_cfg "$D19" CU_CORE_2017_ATTRIBUTE_WRITE OFF

    if [ ! -f "$D19/compile_commands.json" ]; then
        echo "  FAIL  stale Write Value override scenario did not produce compile_commands.json"
        FAIL=$((FAIL + 1))
    else
        if grep -q -- "-DMUC_OPCUA_SERVICE_WRITE=1" "$D19/compile_commands.json"; then
            echo "  FAIL  stale Write Value override emitted MUC_OPCUA_SERVICE_WRITE=1"
            FAIL=$((FAIL + 1))
        else
            echo "  PASS  stale Write Value override did not emit MUC_OPCUA_SERVICE_WRITE=1"
            PASS=$((PASS + 1))
        fi

        if grep -q -- "-DMUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES=1" "$D19/compile_commands.json"; then
            echo "  FAIL  stale Write Value override emitted MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES=1"
            FAIL=$((FAIL + 1))
        else
            echo "  PASS  stale Write Value override did not emit MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES=1"
            PASS=$((PASS + 1))
        fi
    fi

    if cmake --build "$D19" --target muc_opcua -j4 >"$D19_BUILD_LOG" 2>&1; then
        if [ ! -f "$D19/src/libmuc_opcua.a" ]; then
            echo "  FAIL  stale Write Value override scenario did not produce src/libmuc_opcua.a"
            FAIL=$((FAIL + 1))
        elif ! D19_NM_OUTPUT=$(nm "$D19/src/libmuc_opcua.a" 2>/dev/null); then
            echo "  FAIL  could not inspect compiled Write symbols in src/libmuc_opcua.a"
            FAIL=$((FAIL + 1))
        elif printf '%s\n' "$D19_NM_OUTPUT" | \
            grep -qE '(^|[[:space:]])(handle_write|mu_write_request_decode|mu_write_response_encode)$'; then
            echo "  FAIL  stale Write Value override compiled Write symbols"
            FAIL=$((FAIL + 1))
        else
            echo "  PASS  stale Write Value override did not compile Write symbols"
            PASS=$((PASS + 1))
        fi
    else
        echo "  FAIL  could not build stale Write Value override scenario"
        cat "$D19_BUILD_LOG"
        FAIL=$((FAIL + 1))
    fi
else
    echo "  FAIL  could not configure stale Write Value override scenario"
    cat "$D19_CONFIGURE_LOG"
    FAIL=$((FAIL + 1))
fi

echo
echo "===================="
echo "$PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
