# Quickstart: Five-Lens Audit Remediation

How to build, test, and verify each remediation. Assumes the existing host
toolchain (CMake + Unity + ctest) and the Pico SDK cross-compile already work
(see `scripts/check_build_matrix.sh`).

## Build & test (host)

```bash
# Configure + build the host test build
cmake -S . -B build -DMUC_OPCUA_PROFILE=full
cmake --build build -j

# Full suite must stay green throughout
ctest --test-dir build --output-on-failure
```

## Per-story verification

### Stories 1–2 + username nonce (security)

```bash
# RED first: these fixtures fail before the fix, pass after
ctest --test-dir build -R 'activate_session_signature|certificate_validity|username_nonce' --output-on-failure
```
- Bad/replayed/tampered ClientSignature → rejected; correctly-signed → activates.
- Expired / untrusted / no-trust-list cert on a secured policy → rejected.
- Username token without encryption → still nonce-checked (constant-time).

### Story 3 (RP2040 adapter + handshake stack)

```bash
# Entropy/time adapter double (host-side test of the hook contract)
ctest --test-dir build -R 'pico_adapter' --output-on-failure
# Stack budget on the secured handlers
scripts/check_stack_usage.sh          # CreateSession/ActivateSession within budget
# Cross-compile still builds
scripts/check_build_matrix.sh          # includes the RP2040 target
```
- Two entropy reads differ and are non-zero; time advances; no >1 KB stack local
  on the CreateSession/ActivateSession path.

### Story 4 (address-space cap)

```bash
ctest --test-dir build -R 'address_space_cap|query_large_space' --output-on-failure
```
- >64-node space → loud diagnostic (not silent linear fallback); supported space
  → correct Read/Browse and non-empty Query.

### Story 5 (integer deadband)

```bash
ctest --test-dir build -R 'deadband_integer_equivalence' --output-on-failure
# Confirm soft-float dropped from the integer branch on the ARM build
scripts/measure_size.sh embedded        # expect .text flat or reduced
```
- Integer fixtures: identical report/suppress decisions vs the prior double impl.

### Story 6 (feature guards)

```bash
scripts/check_build_matrix.sh           # all four presets build unchanged
# Build-negative: illegal custom combo must fail to compile
cmake -S . -B build-bad -DMUC_OPCUA_PROFILE=custom \
      -DMUC_OPCUA_SUBSCRIPTIONS_STANDARD=ON -DMUC_OPCUA_SUBSCRIPTIONS=OFF
cmake --build build-bad 2>&1 | grep -q 'requires MUC_OPCUA_SUBSCRIPTIONS' && echo "guard OK"
```

### Story 7 (residual: decoder defects + refactors)

```bash
ctest --test-dir build -R 'history_negative_length|query_operand_init' --output-on-failure
# Refactor parity: byte-identical responses + size/speed within gates
ctest --test-dir build -R 'response_regression' --output-on-failure
make speed-compare                      # normalized throughput >= 0.85x baseline
scripts/measure_size.sh all             # .text +3% / RAM +5% gates hold
```

## Global gates (every change)

```bash
ctest --test-dir build --output-on-failure      # all green
scripts/measure_size.sh all                      # .text <= +3%, data+bss <= +5%, no new heap
make speed-compare                               # throughput >= 0.85x baseline
scripts/check_stack_usage.sh                      # stack within budget
```

## Definition of done

- All acceptance scenarios in spec.md pass; SC-001…SC-008 met.
- Every wire-visible change cites its OPC UA section in code comments and tests.
- No profile exceeds the resource gates; no new heap; core stays heap-free.
- The three security fixes reject previously-accepted invalid inputs and leave
  valid-message encoding byte-identical.
