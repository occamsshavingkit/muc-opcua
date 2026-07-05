# Quickstart: Decompose Oversized Functions

## Overview

Refactor-only: no new features, no API changes, no test changes.

## Prerequisites

```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build .
ctest --output-on-failure  # record baseline: all pass
```

## Per-Function Workflow

For each function (CX1-CX16):

1. Read the function to identify logical sub-concerns
2. Extract each sub-concern into a `static` helper above the original function
3. Replace the extracted code in the parent with a call to the helper
4. Rebuild: `cmake --build .`
5. Run tests: `ctest --output-on-failure`
6. If all pass, commit with message: `ref: decompose [function_name] into helpers`
7. If any test fails, revert with `git checkout -- src/...` and debug

## Verification

```bash
# Check all 16 functions are ≤ 100 lines:
for f in src/core/dispatch_session.c src/core/service_dispatch.c src/core/server.c src/security/asym_chunk.c src/services/subscription_publish.c src/services/read.c; do
  echo "=== $f ==="
  # manual review: no function definition exceeds 100 lines
done

# Full regression suite:
ctest --output-on-failure

# Format check:
find src -name '*.c' | xargs clang-format --dry-run --Werror
```
