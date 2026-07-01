# Quickstart: Conformance Docs and PubSub Subscriber

## 1. Validate Documentation and Conformance Guards

```bash
cmake -S . -B build/conformance-docs -DMICRO_OPCUA_BUILD_TESTS=ON -DMICRO_OPCUA_PUBSUB=ON
cmake --build build/conformance-docs --target test_conformance_docs test_traceability_docs
ctest --test-dir build/conformance-docs -R '^(test_conformance_docs|test_traceability_docs)$' --output-on-failure
```

Expected result: both tests pass, and stale profile/CTT claims, stale StatusCode values, stale aggregate NodeIds, and stale section placeholders are rejected.

## 2. Validate UADP Subscriber Decode

```bash
cmake --build build/conformance-docs --target test_uadp_encoding test_pubsub
ctest --test-dir build/conformance-docs -R '^(test_uadp_encoding|test_pubsub)$' --output-on-failure
```

Expected result: the existing publisher fixture decodes through the scoped subscriber path, and malformed/unsupported UADP inputs return the documented StatusCodes.

## 3. Validate Subscription/Status Negative Paths

```bash
cmake --build build/conformance-docs --target test_subscriptions_errors test_aggregate test_status
ctest --test-dir build/conformance-docs -R '^(test_subscriptions_errors|test_aggregate|test_status)$' --output-on-failure
```

Expected result: selected negative paths return cited StatusCodes and successful-path tests remain unchanged.

## 4. Measure Resource Impact

```bash
scripts/measure_size.sh all
```

Record any PubSub subscriber `.text`, static RAM, or stack impact in `docs/size/feature-size-ledger.md` or `docs/traceability/023-conformance-docs-subscriber.md`.

## 5. Final Validation

```bash
make test
make all-profiles
```

Expected result: host tests and profile builds pass. If full controlled benchmark validation is needed, run the existing `make speed-compare` workflow and keep generated artifacts under `docs/benchmarks/`.
