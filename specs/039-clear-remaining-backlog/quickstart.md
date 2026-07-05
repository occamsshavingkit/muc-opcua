# Quickstart: Clear Remaining Backlog

## Overview

This is a backlog-clearance round. All work modifies existing code; there are
no new features to set up. The quickstart covers how to verify each category
of fix.

## Prerequisites

Same as the project baseline:

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DMUC_OPCUA_SANITIZERS=ON
cmake --build .
ctest --output-on-failure
```

## Verification by Category

### US1: Bug Fixes
```bash
# After fixes, run specific tests:
ctest -R test_tcp_connection
ctest -R test_service_dispatch
# Or run the full suite to check no regressions
ctest --output-on-failure
```

### US2: Hot-Path Fixes
```bash
# Memory leak check (HP1):
ASAN_OPTIONS=detect_leaks=1 ctest -R test_variant

# Read cache (HP2) and tick batching (HP3):
# No specific test yet — verify via profiler or add instrumentation
ctest --output-on-failure

# HP4-HP13: Full test suite covers affected code paths
ctest --output-on-failure
```

### US3: Code Quality & UB
```bash
# UB fixes (UB3-UB7): Build with strict flags
cmake .. -DCMAKE_C_FLAGS="-fstrict-aliasing -Wconversion -Wpedantic"
cmake --build .
ctest --output-on-failure

# Portable CTZ (CQ6): Compile on MSVC or with -DNO_BUILTIN_CTZ to test fallback
```

### US4: Security
```bash
# SE1-SE4: Run security-sensitive tests
ctest -R test_security
ctest -R test_trust_list
ctest -R test_event_filter
```

### US5: Tests & Interop
```bash
# Placeholder replacement verification:
# Check that no test file contains only placeholder assertions
grep -r "TEST_IGNORE\|test_ignore\|// TODO.*placeholder" tests/

# Interop tests (requires Python asyncua):
pip install asyncua
python tests/interop/interop_smoke.py
```

### US6: Documentation
```bash
# Verify ADRs exist:
ls docs/adr/

# Verify spec grounding comments:
grep -r "OPC-10000" src/encoding/binary_nodeid.c
grep -r "OPC-10000" src/encoding/binary_datavalue.c
grep -r "OPC-10000" src/core/uasc.c
grep -r "OPC-10000" include/muc_opcua/encoding.h
grep -r "OPC-10000" include/muc_opcua/server.h
```

## Full Verification

```bash
# Complete build and test with all profiles:
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DMUC_OPCUA_SANITIZERS=ON \
  -DMUC_OPCUA_PROFILE=standard
cmake --build .
ctest --output-on-failure

# Verify no regressions vs baseline commit
git stash
ctest --output-on-failure | tee baseline.log
git stash pop
ctest --output-on-failure | tee current.log
diff baseline.log current.log
```
