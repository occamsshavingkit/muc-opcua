# Research: OPC UA Conformance and Size Repairs

## Decision: Use official CSV values for numeric identifier tests

**Decision**: Tests for StatusCode and NodeId constants will use expected values from the OPC Foundation UA NodeSet `UA-1.05.07-2026-04-15` `StatusCode.csv` and `NodeIds.csv`.

**Rationale**: OPC UA sections define the names and semantics, while the CSV tables provide canonical numeric values for wire compatibility. The audit found local constants that collided with unrelated standard values, so tests must compare against external expected numbers rather than local aliases.

**Alternatives considered**: Continue self-consistency tests against local macros. Rejected because those tests would preserve the current bug.

## Decision: Limit aggregate repair to scoped subscription AggregateFilter behavior

**Decision**: Correct the aggregate filter encoding NodeId to `730` and aggregate function NodeIds to Average `2342`, Minimum `2346`, and Maximum `2347`; keep support limited to the existing running-calculation subscription behavior and reject unsupported functions/configurations with monitored-item filter StatusCodes.

**Rationale**: OPC-10000-4 §7.22.4 defines `AggregateFilter`; OPC-10000-13 §5.4.3.5, §5.4.3.10, and §5.4.3.11 define Average, Minimum, and Maximum behavior. Full historical aggregate semantics are out of scope and would expand RAM/ROM beyond this repair feature.

**Alternatives considered**: Implement all aggregate configuration, bounding, and historical semantics. Rejected because this feature is a repair and size-control effort, not a full aggregate conformance expansion.

## Decision: Implement GetEndpoints transport-profile filtering only

**Decision**: Apply `profileUris` filtering from OPC-10000-4 §5.5.4.2 and leave endpoint URL and locale negotiation behavior documented as follow-up unless existing parsing already supports a low-risk fix.

**Rationale**: The audit identified `profileUris` as directly unimplemented and testable with existing endpoint data. Transport-profile filtering is the highest-value interoperability fix and can be implemented with bounded parsing.

**Alternatives considered**: Full GetEndpoints request handling for endpoint URL normalization and locale negotiation. Rejected for this feature because locale negotiation requires broader localized text policy review.

## Decision: Fix stack-budget gate semantics before stack refactors

**Decision**: Treat missing `.su` entries for static helpers as optimized-away/inlined unless the function is externally required, and continue enforcing thresholds on emitted frames and documented path estimates.

**Rationale**: The audit test failed because `handle_data_chunk_secure` did not appear as an emitted frame in the compiler output, not because a real frame exceeded the budget. The gate should fail for measurable excess stack, not for compiler symbol decisions.

**Alternatives considered**: Force noinline on the helper solely for measurement. Rejected because it would perturb code shape and potentially increase flash/stack.

## Decision: Use integer-only timeout conversion as the first ROM/speed repair

**Decision**: Replace the remaining session timeout `double` cast with the existing integer bit-to-milliseconds conversion pattern.

**Rationale**: `session.c` already clamps IEEE-754 bits by integer comparison, then reintroduces soft-float with a double cast. Removing that cast should reduce soft-float helper pull-in on Cortex-M0+ without changing wire behavior.

**Alternatives considered**: Leave session timeout untouched and focus only on large service-dispatch refactors. Rejected because a local fix is lower risk and directly measurable.

## Decision: Record large RAM refactors as configuration controls unless small safe reductions are available

**Decision**: This feature may add or document configuration controls for audited RAM hotspots, but will not split monitored-item storage or connection buffers unless the change is small and testable.

**Rationale**: The largest RAM opportunities are structural and can destabilize subscription behavior. The user asked to start implementation after planning, so this feature prioritizes correctness fixes and low-risk size/performance repairs.

**Alternatives considered**: Redesign subscription storage immediately. Rejected because it needs a separate feature with focused tests and migration notes.

## Audit Baselines

- ARM Cortex-M0+ archive text: nano 16,653 bytes; micro 23,962 bytes; embedded 47,126 bytes; full 47,400 bytes.
- Host `struct mu_server`: nano 944 bytes; micro 3,048 bytes; embedded 69,344 bytes; full 77,832 bytes.
- Host `mu_monitored_item_t`: micro 152 bytes; embedded/full 392 bytes.
- Host `mu_connection_t`: embedded/full 3,376 bytes; per-connection receive storage dominates multi-connection RAM. Later review confirmed OPC-10000-6 §7.1.2.3/§7.1.2.4 require an 8192-byte receive-buffer negotiation floor for the non-ECC transport profiles used here, so reducing this backing store below that floor is not a standards-compliant RAM reduction.
- Stack frames observed in test build: `handle_history_read` 10,256 bytes; `handle_browse` 5,104 bytes; `handle_read` 5,056 bytes; `handle_write` 4,464 bytes; `handle_create_session` 3,344 bytes.
