# Quickstart — Optimization Audit Remediation

How to build, verify, and validate each slice. Slices are independent and land in
priority order (US1 → US2 → US3 → US4).

## Build & test (host)

```sh
# Configure + build a profile (micro shown)
make micro            # or: cmake -S . -B build/micro -DMICRO_OPCUA_PROFILE=micro && cmake --build build/micro

# Full suite for a profile
ctest --test-dir build/micro --output-on-failure

# Sanitizers (decoders / memory safety)
cmake -S . -B build/asan -DCMAKE_C_FLAGS="-fsanitize=address,undefined -g"
cmake --build build/asan && ctest --test-dir build/asan --output-on-failure

# Fuzz a decoder target (libFuzzer)
cmake --build build/fuzz && ./build/fuzz/tests/fuzz/fuzz_message_chunk -runs=200000
```

## Per-slice verification

### US1 — Crash-safety & parser correctness (P1)
- New unit tests: ActivateSession with non-empty `clientSoftwareCertificates` + a
  populated UserIdentityToken decodes following fields correctly (OPC-10000-4
  §5.7.3.2); NodeId type-confusion returns the cited StatusCode; OPN policy/mode
  mismatch handled per §5.6.2.2.
- Stack budget: build with `-fstack-usage` (or `-Wstack-usage=<N>`) and confirm the
  secured OPN call chain ≤ 10 KiB (SC-001).
- Extend fuzz corpus with cert-array/token-bearing ActivateSession frames.

### US2 — Secret zeroization (P2)
- Run the channel-reuse test: OPN → MSG → close, then assert key storage reads zero.
- Review crypto helpers scrub stack secrets (contract: `secure-zero.md`).

### US3 — Hot-path performance (P2)
- Byte-identical regression: capture Read/Browse/subscription response bytes before
  and after; assert equal (SC-003).
- Index property test: `mu_address_space_find_node` indexed === linear for all inputs;
  comparison count grows ~log N.
- Sampling probe: zero `find_node` calls per tick (E3 cache).
- Cipher-context probe: key schedule runs once per channel (D2 stub counter).

### US4 — Footprint (P3)
- Build every profile AND every single-service-disabled permutation under
  warnings-as-errors; all compile (T17) and the disabled service is absent (`nm`).
- Compare `size` output to `docs/size/feature-size-ledger.md`; Micro core ≥ 8% smaller
  (SC-005); update the ledger.
- No-heap check still green (no new allocation, FR-019).

## Cross-compile (embedded gate)
```sh
# Pico SDK cross-compile (size + link sanity)
make pico   # or the project's documented Pico build target
```

## Definition of done (per slice)
- All existing + new tests green for the affected profile(s).
- Behavioural changes cited to OPC-10000-4/-6 sections in code comments + traceability.
- ASan/UBSan clean; no-heap check passes; size ledger updated (US4).
