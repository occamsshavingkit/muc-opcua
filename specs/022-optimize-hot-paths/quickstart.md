# Quickstart: Optimize Hot Paths

## 1. Confirm Active Feature

```sh
cat .specify/feature.json
```

Expected feature directory:

```json
{
  "feature_directory": "specs/022-optimize-hot-paths"
}
```

## 2. Capture Fresh Controlled Baseline

Before running benchmarks, review
[`contracts/benchmark-report.md`](contracts/benchmark-report.md) for the required
report fields and authoritative lab criteria used to accept optimization evidence.

```sh
make speed-baseline
make speed-compare
```

The resulting baseline must show CPU 11, passwordless sudo, realtime priority 80,
required affinity/realtime/isolation flags, and all supported rows passing compare.

## 3. Run Correctness Checks Before Editing a Hot Path

Choose the checks for the area being optimized:

```sh
make test
```

For parser/framing changes, also run the relevant fuzz smoke target when available.
For interop-sensitive changes, run the project interop smoke before accepting the
change.

## 4. Implement One Optimization at a Time

Keep each change scoped to one hot path:

- Binary primitive read/write.
- MessageChunk/header framing.
- Read service dispatch.
- Write service dispatch.
- Response framing/send-buffer path.

Do not broaden protocol scope or compatibility claims.

## 5. Compare After Each Candidate

```sh
make speed-compare
```

Record before/after affected rows. Reject the candidate if it changes observable
OPC UA behavior, adds heap use, exceeds ROM/static RAM gates, or causes unaccepted
throughput regression.

## 6. Final Verification

```sh
make test

cmake -S . -B build/sanitize \
  -DMICRO_OPCUA_BUILD_TESTS=ON \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build/sanitize
ctest --test-dir build/sanitize --output-on-failure

cmake -S . -B build/test -DMICRO_OPCUA_BUILD_TESTS=ON
cmake --build build/test --target format-check
cmake --build build/test --target cppcheck
cmake --build build/test --target clang-tidy

make all-profiles

cmake -S . -B build/full \
  -DMICRO_OPCUA_PROFILE=full \
  -DMICRO_OPCUA_BUILD_EXAMPLES=ON \
  -DMICRO_OPCUA_OPTIMIZE_SIZE=ON
cmake --build build/full

# Core/hot-path heap gate: platform adapters own host/crypto allocation and are
# outside the protocol runtime path covered by FR-005/SC-004.
! rg -n "\b(malloc|calloc|realloc|free)\s*\(" src/core src/encoding src/services include

cmake -S . -B build/stack \
  -DMICRO_OPCUA_BUILD_TESTS=ON \
  -DMICRO_OPCUA_STACK_USAGE=ON
cmake --build build/stack
cmake --build build/stack --target check_stack_usage

cmake -S . -B build/pico \
  -DMICRO_OPCUA_PLATFORM=pico \
  -DPICO_SDK_FETCH_FROM_GIT=ON \
  -DMICRO_OPCUA_BUILD_EXAMPLES=ON
cmake --build build/pico

make speed-compare
make speed-compare
git diff --check
```

The no-heap check is expected to produce no matches under `src` and `include`.
The two `make speed-compare` runs must agree on pass/fail outcome and record
matching scheduling and host-fingerprint fields.

Refresh the committed baseline only after the final candidate is accepted and the
new baseline represents the improved implementation.
