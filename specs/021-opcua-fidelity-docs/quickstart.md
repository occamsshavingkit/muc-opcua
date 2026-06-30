# Quickstart: OPC UA Fidelity Docs and Next Work

## Documentation Slice

1. Read `docs/getting-started.md`.
2. Verify the FreeRTOS + lwIP section explains:
   - `MICRO_OPCUA_PLATFORM=external`
   - static `MU_SERVER_STORAGE_BYTES` storage
   - static receive/send buffers
   - lwIP nonblocking TCP callbacks
   - FreeRTOS tick-based time callbacks
   - entropy callback
   - server task polling loop
   - RAM categories outside the core library
3. Run documentation/conformance checks:

```bash
cmake --build build/baseline --target test_conformance_docs
ctest --test-dir build/baseline --output-on-failure -R test_conformance_docs
```

Baseline evidence before this feature slice: `cmake -S . -B build/baseline -DMICRO_OPCUA_BUILD_TESTS=ON -DMICRO_OPCUA_OPTIMIZE_SIZE=ON && cmake --build build/baseline && ctest --test-dir build/baseline --output-on-failure` passed with 95/95 tests on 2026-06-30.

Post-doc/citation evidence: `cmake --build build/baseline --target test_conformance_docs && ctest --test-dir build/baseline --output-on-failure -R test_conformance_docs` passed with 1/1 tests on 2026-06-30 after the FreeRTOS/lwIP docs, conformance citation fixes, and claim wording cleanup.

T032 claim-wording guard evidence: Nano, Micro, Embedded, and traceability docs were reviewed for OPC-10000-7 §4.2/§4.3 claim wording. They now present profile/conformance-unit language as profile-targeting only and do not claim profile-compliant, CTT-verified, certified, or passed-CTT status without external CTT evidence; controller verification `ctest --test-dir build/baseline --output-on-failure -R test_conformance_docs` passed on 2026-06-30 after the US5 wording review.

T033a/T033b conformance-doc evidence: `ctest --test-dir build/baseline --output-on-failure -R test_conformance_docs` passed on 2026-06-30 with 1/1 tests passed, 0 failed; total real time was 0.16 sec.

External platform evidence: `cmake -S . -B build/external-doc-check -DMICRO_OPCUA_PLATFORM=external -DMICRO_OPCUA_PROFILE=nano -DMICRO_OPCUA_OPTIMIZE_SIZE=ON && cmake --build build/external-doc-check` passed on 2026-06-30 and built `src/libmicro_opcua.a` without in-tree host/Pico/Arduino adapter sources.

Full-suite evidence: `ctest --test-dir build/baseline --output-on-failure` passed with 95/95 tests on 2026-06-30.

Whitespace evidence: `git diff --check` passed on 2026-06-30.

Async-opcua comparative perf evidence: artifacts under `/home/quackdcs/scratch/opcua-localhost-bench/perf-20260630-b92d983f-4e74d40-async-*` were reviewed on 2026-06-30. Read-path hotspots were `Chunker::decode` (12.35%), `ReadRequest::decode` (9.66%), `SendBuffer::write` (11.46%), `Chunker::encode_into` (8.99%), and `join_all` (6.46%). Write-path hotspots were `Chunker::decode` (10.87%), `WriteRequest::decode` (8.91%), `SendBuffer::write` (8.13%), `dispatch_write_audit` (8.00%), and `join_all` (5.07%). `trace_locks` was negligible in this capture, so remaining resource-risk tasks should focus local measurement on chunk/request encode-decode, send/write paths, and bounded request fanout rather than lock tracing.

## Citation Slice

1. Inspect `docs/conformance/services.md`.
2. Confirm Method Call cites OPC-10000-4 §5.12.2.
3. Confirm AddNodes/DeleteNodes/AddReferences/DeleteReferences cite OPC-10000-4 §5.8.
4. Run `ctest --test-dir build/baseline --output-on-failure -R test_conformance_docs`.

Post-doc/citation evidence is recorded in the Documentation Slice section above.

## Future Protocol Slices

Before changing aggregate or subscription behavior:

1. Add the RED test named by the matching task.
2. Confirm the RED test fails for the expected reason.
3. Implement the smallest behavior change.
4. Run the focused test, full CTest, and size measurement required by `tasks.md`.
