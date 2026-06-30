# Optimize Hot Paths

## Goal
Improve micro-opcua throughput without increasing ROM, RAM, stack depth, heap use, or changing OPC UA wire-visible behavior.

## Baseline
- Source: `docs/benchmarks/speed-baseline.json`, generated 2026-06-30 on `QuackServer`.
- Lab contract: isolated CPU 11, `sudo -n chrt -f 80 taskset -c 11`, performance governor.
- Current size anchors: nano 34,286 B text / 264 B RAM; micro 47,115 B / 528 B; embedded 80,694 B / 5,984 B; full 96,398 B / 6,224 B.
- Current throughput anchors: Read 1 item ~6.75M-7.03M ops/s; Read 128/batch32 ~515K-570K ops/s; Write 128/batch32 ~506K-555K ops/s.

## Tasks
- [ ] T001: Capture a fresh strict baseline before edits with `make speed-baseline && make speed-compare` -> Verify: JSON records CPU 11, sudo, realtime, isolation, and all rows pass compare.
- [ ] T002 [ROM/RAM/throughput risk]: Optimize OPC UA TCP `MessageChunk` header parse/write in `src/core/message_chunk.c` by removing unnecessary reader/writer setup on fixed-size fields while preserving validation -> Verify: `test_message_chunk*`, `fuzz_message_chunk` smoke, and `make speed-compare`; spec: OPC-10000-6 6.7.2, 7.1.2.2.
- [ ] T003 [ROM/RAM/throughput risk]: Optimize primitive binary reader/writer hot paths in `src/encoding/binary_reader.c`, `src/encoding/binary_writer.c`, and `src/encoding/binary_le.h` without weakening bounds/status behavior -> Verify: `test_binary_*`, decoder fuzz smoke, and no `.text`/RAM growth over baseline; spec: OPC-10000-6 5.2, 5.2.5.
- [ ] T004 [throughput risk]: Reduce Read request/response dispatch overhead in `src/core/service_dispatch.c` and `src/services/read.c` for batch reads, keeping per-operation StatusCodes unchanged -> Verify: `test_read_service`, read fixtures, `hotpath_benchmark` read rows improve; spec: OPC-10000-4 5.11.2, 7.38.2.
- [ ] T005 [throughput risk]: Reduce Write dispatch overhead in `src/core/service_dispatch.c` and `src/services/write.c`, especially type checks and callback handoff for batch writes -> Verify: `test_write_*`, write fixtures, `hotpath_benchmark` write rows improve; spec: OPC-10000-4 5.11.4, 6.5.8, 7.38.2.
- [ ] T006 [ROM/RAM/throughput risk]: Audit response framing/send-buffer writes in `src/core/uasc.c`, `src/core/tcp_connection.c`, and host adapters for avoidable copies or repeated header work -> Verify: `test_server_framing`, `test_response_regression`, interop smoke, and speed compare; spec: OPC-10000-6 6.7.2, 6.7.4, 7.2.
- [ ] T007: After each optimization, record before/after numbers for the affected rows and run `make test && make speed-compare` -> Verify: no functional regressions, `.text` <= 1.03x baseline, RAM <= 1.05x baseline, normalized throughput >= 0.85x baseline.
- [ ] T008: Refresh `docs/benchmarks/speed-baseline.json` only after a net win is verified and document the reason in the PR -> Verify: baseline file reflects the final commit and strict lab scheduling fields.

## Done When
- [ ] At least one targeted hot path has a measured throughput improvement in strict lab mode.
- [ ] No profile exceeds the resource gates: `.text` +3%, `.data + .bss` +5%, no new heap use.
- [ ] OPC UA conformance tests, interop smoke, and all affected negative-path tests still pass.
- [ ] Every behavior-touching change cites the OPC UA section it preserves.
