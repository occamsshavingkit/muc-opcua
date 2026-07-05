# CI Pre-Check Playbook

Run these before pushing to catch the same failures CI will report.

All commands run from the repo root.

## 1. Build & test (host, default profile)

```bash
cmake -S . -B build/test -DCMAKE_BUILD_TYPE=Debug -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_HAVE_MBEDTLS=ON -DMUC_OPCUA_HAVE_WOLFSSL=ON -DMUC_OPCUA_PUBSUB=ON
cmake --build build/test
ctest --test-dir build/test --output-on-failure
```

## 2. All profile tests

```bash
for profile in nano micro standard embedded full; do
  cmake -S . -B build/test-$profile -DCMAKE_BUILD_TYPE=Debug -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_HAVE_MBEDTLS=ON -DMUC_OPCUA_HAVE_WOLFSSL=ON -DMUC_OPCUA_PROFILE=$profile
  cmake --build build/test-$profile
  ctest --test-dir build/test-$profile --output-on-failure
done
```

## 3. Static analysis

```bash
# Format check
cmake -S . -B build/host -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_PUBSUB=ON
cmake --build build/host --target format-check

# CppCheck
cmake --build build/host --target cppcheck

# Clang-Tidy (needs clang-tidy installed)
cmake --build build/host --target clang-tidy
```

## 4. Sanitizer build

```bash
CC=clang CXX=clang++ cmake -S . -B build/asan -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_SANITIZERS="address,undefined" -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_HAVE_MBEDTLS=ON -DMUC_OPCUA_HAVE_WOLFSSL=ON -DMUC_OPCUA_PUBSUB=ON
cmake --build build/asan
ctest --test-dir build/asan --output-on-failure
```

## 5. Interop

```bash
cmake -S . -B build/host -DMUC_OPCUA_BUILD_EXAMPLES=ON -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_PUBSUB=ON
cmake --build build/host --target minimal_server
python3 -m venv /tmp/venv && /tmp/venv/bin/pip install --quiet asyncua
PATH="/tmp/venv/bin:$PATH" tests/interop/run_interop.sh
```

## 6. Freestanding ARM compile (no libc/OS)

```bash
for f in $(find src -name '*.c' -not -path 'src/platform/host_*'); do
  arm-none-eabi-gcc -ffreestanding -ffunction-sections -fdata-sections \
    -std=c11 -Iinclude -Isrc -Wall -Wextra -Werror \
    -c "$f" -o /dev/null
done
```

## 7. Profile sizes (ARM Cortex-M0+)

```bash
bash scripts/measure_size.sh all
```

## 8. All-in-one

```bash
bash scripts/measure_size.sh all && \
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_HAVE_MBEDTLS=ON -DMUC_OPCUA_HAVE_WOLFSSL=ON -DMUC_OPCUA_PUBSUB=ON && \
cmake --build build && ctest --test-dir build --output-on-failure && \
cmake --build build --target format-check
```

## What CI runs and where

| CI job | Command summary |
|--------|----------------|
| `host-build` | configure + build + ctest (host, default profile) |
| `profile-tests` | configure + build + ctest per profile (nano/micro/standard/embedded/full) |
| `static-analysis` | format-check + cppcheck + clang-tidy |
| `sanitizer-build` | configure with clang + sanitizers + ctest |
| `interop` | build minimal_server + run interop_smoke.py |
| `dotnet-interop` | build minimal_server + run .NET reference client |
| `fuzz-build` | build fuzz targets (Clang) |
| `pico-cross-compile` | cross-compile for RP2040 with Pico SDK |
| `freestanding-core` | arm-none-eabi-gcc -ffreestanding on all source files |
| `coverage` | configure with --coverage + build + ctest + gcovr |
