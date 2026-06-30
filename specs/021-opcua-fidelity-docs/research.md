# Research: OPC UA Fidelity Docs and Next Work

## Decision: Close FreeRTOS + lwIP Docs in Getting Started First

**Rationale**: GitHub issue #222 asks for a practical integration pattern and the user explicitly requested getting-started documentation. `docs/integration-guide.md` already explains the full adapter contract; adding a concise getting-started section gives users the minimum working shape without duplicating the full porting guide.

**Alternatives considered**: A new `docs/platforms/freertos-lwip.md` page was deferred because the current request names getting-started docs and there is no in-tree FreeRTOS example to anchor a standalone page yet.

## Decision: Keep FreeRTOS/lwIP Outside the Core Claim

**Rationale**: The library's no-heap claim applies to the core protocol path and caller-owned buffers. FreeRTOS and lwIP normally maintain their own heap/task/socket resources, so documentation must account for those separately rather than implying zero-heap firmware.

**Alternatives considered**: Claiming end-to-end zero heap for firmware was rejected because the platform stack may allocate internally.

## Decision: Fix Citation Drift Before Protocol Expansion

**Rationale**: `docs/conformance/services.md` feeds future implementation and review work. Method Call belongs to OPC-10000-4 §5.12.2, while NodeManagement belongs to OPC-10000-4 §5.8. Correcting these before task execution prevents coders from re-referencing stale rows.

**Alternatives considered**: Deferring citation cleanup until after behavior changes was rejected because the constitution requires section-level traceability for tasks and reviews.

## Decision: Keep Aggregate Work Scoped

**Rationale**: Current aggregate support targets Average, Minimum, and Maximum. The project must continue rejecting unsupported AggregateFunction NodeIds with `Bad_MonitoredItemFilterUnsupported` rather than implying full OPC-10000-13 coverage.

**Alternatives considered**: Expanding to full aggregate semantics was rejected because it would add scope, tests, memory review, and conformance surface beyond the current profile-targeting work.

## Decision: Treat Subscription Negative Paths as Test-First Protocol Work

**Rationale**: MonitoredItem and Subscription services manage bounded storage and wire-visible StatusCodes. Invalid IDs, capacity exhaustion, invalid acknowledgements, malformed filters, and unsupported services need RED tests before behavior changes.

**Alternatives considered**: Documentation-only negative-path claims were rejected for behavior changes because they do not prove parser/state-machine correctness.

## Decision: Use the async-opcua Perf Pass as Comparative Hotspot Evidence

**Rationale**: The localhost playbook artifacts under `/home/quackdcs/scratch/opcua-localhost-bench/perf-20260630-b92d983f-4e74d40-async-*` show that the captured async-opcua read path spends visible time in `Chunker::decode` (12.35%), `ReadRequest::decode` (9.66%), `SendBuffer::write` (11.46%), `Chunker::encode_into` (8.99%), and `join_all` (6.46%). The write path similarly highlights `Chunker::decode` (10.87%), `WriteRequest::decode` (8.91%), `SendBuffer::write` (8.13%), `dispatch_write_audit` (8.00%), and `join_all` (5.07%). `trace_locks` is negligible in this captured report, so it should not drive this feature's optimization priority unless new local evidence changes that.

**Application to micro-opcua**: Treat the Rust server hotspots as triage input, not direct proof about this C library. Resource-risk implementation tasks should preserve bounded OPC-10000-6 binary/message-chunk handling, avoid new heap-backed per-request fanout, and measure any aggregate/subscription changes with the local size/performance commands before completion.

**Alternatives considered**: Optimizing lock tracing first was rejected because the current captured evidence no longer shows it as material. Importing async-opcua implementation techniques was rejected because this project is a freestanding C library with caller-owned storage and a different concurrency model.

## Decision: Profile Claims Stay Profile-Targeting

**Rationale**: OPC-10000-7 §4.2 and §4.3 govern conformance-unit/profile claims. There is no external CTT evidence in this feature, so profile-compliant and CTT-verified claims remain withheld.

**Alternatives considered**: Promoting current implementation evidence to profile-compliant status was rejected because project docs and tests explicitly require external CTT evidence.
