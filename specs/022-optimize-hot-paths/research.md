# Research: Optimize Hot Paths

## Decision: Treat benchmark evidence as a release gate

**Decision**: Every optimization must begin with the controlled baseline and end
with a controlled comparison that records host fingerprint, isolated CPU 11,
`sudo -n` realtime scheduling at priority 80, workload rows, normalized
throughput, and ROM/RAM measurements.

**Rationale**: The baseline is already calibrated for repeatability and records 42
rows across `nano`, `micro`, `embedded`, and `full`. Without the same scheduling and
host controls, speed deltas can reflect host noise rather than code changes.

**Alternatives considered**: Ad hoc local timing was rejected because it cannot
distinguish code regressions from scheduler noise. External localhost-only
benchmarks remain useful for exploration but are not sufficient as the acceptance
gate for embedded resource work.

## Decision: Preserve existing OPC UA scope and conformance claims

**Decision**: Optimization work must not add services, transports, encodings,
security policies, or conformance claims. Profile terminology remains governed by
OPC-10000-7 section 4.2.

**Rationale**: The feature is performance/resource work, not protocol expansion.
The constitution requires conformance honesty and exact scope claims.

**Alternatives considered**: Combining optimization with broader service support
was rejected because it would mix performance evidence with new behavior and make
regressions harder to attribute.

## Decision: Use exact OPC UA sections as behavior anchors

**Decision**: Behavior-sensitive optimizations are grounded in:

- OPC-10000-6 section 5.2.1 for Binary DataEncoding sequencing.
- OPC-10000-6 section 5.2.5 for array length/null/empty handling.
- OPC-10000-6 sections 6.7.2, 6.7.2.2, and 6.7.2.4 for MessageChunk, Message
  Header, and Sequence Header behavior.
- OPC-10000-6 sections 6.7.4, 7.1.2.2, and 7.2 for SecureChannel/OPC UA TCP
  framing boundaries.
- OPC-10000-4 sections 5.11.2 and 5.11.4 for Read and Write services.
- OPC-10000-4 sections 5.11.2.4, 5.11.4.4, and 7.38.2 for operation-level
  StatusCode behavior.
- OPC-10000-4 section 6.5.8 for Write/audit semantics.

**Rationale**: The likely hot paths are wire-visible or status-visible. Optimizing
them safely requires preserving the normative contract, not merely passing a
throughput run.

**Alternatives considered**: Citing only broad service-set sections was rejected
because tasks would lack the section-level anchors needed during implementation.

## Decision: Reject lookup-table optimizations unless they prove net size benefit

**Decision**: No new static tables are planned. A task may propose a compact lookup
table only if the before/after evidence shows net ROM/RAM and throughput benefit.

**Rationale**: Tables can make hot paths faster but are high risk for the smallest
profiles. The constitution prioritizes size discipline and application headroom.

**Alternatives considered**: Precomputed tables for every message/header/status
case were rejected as too likely to increase ROM for limited benefit.

## Decision: Keep public interfaces stable

**Decision**: The feature must not expand the public API. Benchmark/report
contracts are documentation contracts for review evidence, not new runtime APIs.

**Rationale**: Optimization should be behavior-preserving and easy for downstream
users to adopt.

**Alternatives considered**: Adding user-facing tuning knobs was rejected for this
feature because it would broaden scope and require separate compatibility planning.

## Decision: Use test-first checks for behavior-sensitive changes

**Decision**: Any optimization touching parsing, encoding, service dispatch,
StatusCodes, or framing must first add or identify a failing/protective test for
the behavior being preserved.

**Rationale**: The constitution requires tests before wire-visible or state-machine
changes. Existing interop and conformance tests can miss low-level negative paths.

**Alternatives considered**: Relying only on the benchmark matrix was rejected
because benchmarks are not exhaustive correctness checks.
