# Quickstart: Conformance-Test Hardening

## Per-profile build & test (US1)

```bash
for p in nano micro embedded full; do
  cmake -S . -B build/$p -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_PROFILE=$p \
        -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_HAVE_MBEDTLS=ON -DMUC_OPCUA_HAVE_WOLFSSL=ON
  cmake --build build/$p -j
  ctest --test-dir build/$p --output-on-failure
done
# or:
make test-nano && make test-micro && make test-embedded && make test-full
```

## Verify the gates are live (US1)

- Confirm a SUBSCRIPTIONS-only test runs in `micro`/`embedded`/`full` and is absent
  in `nano` (test counts differ).
- Break it on purpose: remove a backing test for a mapped claim → that profile's
  build fails naming the unmapped unit. Restore it.

**Measured gate-liveness (2026-07-02, host build, mbedtls+wolfssl on):** the
registered/passing test count rises monotonically with profile breadth, proving the
`RUN_TEST` gates fire per profile:

| Profile | Tests run | Result |
|---------|-----------|--------|
| nano | 65 | 100% pass |
| micro | 73 | 100% pass |
| embedded | 90 | 100% pass |
| full | 99 | 100% pass |

Note: an explicit `-DMUC_OPCUA_PROFILE=<p>` with `-DMUC_OPCUA_BUILD_TESTS=ON` is
already honored (the "force full" resolution only rewrites the *default* profile),
so T004 needed no code change — the gap the audit found was purely that CI/Makefile
never passed an explicit profile. `make test-nano|test-micro|test-embedded|test-full`
and the `profile-tests` CI matrix now do.

## Verify the tiers

```bash
# US2 Browse fix
ctest --test-dir build/full -R browse --output-on-failure   # overflow -> Bad_TooManyOperations
# US3 real-crypto security (needs a crypto backend; skips cleanly otherwise)
ctest --test-dir build/embedded -R 'user_auth|secure_handshake|certificate|trust' --output-on-failure
# US4 forcing tests
ctest --test-dir build/full -R 'too_many_sessions|request_too_large|read.*overflow|close_secure_channel' --output-on-failure
# US5 encoding
ctest --test-dir build/full -R 'expanded_nodeid|endpoint_url|nodeid|primitives|datavalue|qualified|localized' --output-on-failure
```

## Global gates (every change)

```bash
# all four profiles green
for p in nano micro embedded full; do ctest --test-dir build/$p || exit 1; done
# ASAN/UBSan
CC=clang cmake -S . -B build/asan -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_SANITIZERS="address,undefined" \
   -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_HAVE_MBEDTLS=ON -DMUC_OPCUA_HAVE_WOLFSSL=ON -DMUC_OPCUA_PUBSUB=ON
cmake --build build/asan -j && ctest --test-dir build/asan --output-on-failure
# size within gates, four ARM profiles
scripts/measure_size.sh all
```

## Definition of done

- SC-001..SC-007: suite runs under all four profiles with live gates; every claim
  has a profile-runnable backing test (enforced); the audited defects are fixed and
  false claims reconciled; the six US3 security behaviors pass accept+reject on real
  crypto; US4/US5 forcing/coverage tests exist; docs contain no over-claim and stay
  profile-targeting; full suite + ASAN + four ARM profiles green within gates.
