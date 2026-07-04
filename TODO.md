# TODO — muc-opcua

**Updated**: 2026-07-04
**Source**: `docs/review/five-lens-audit-2026-07-04.md`, Codacy triage, deferred findings

## Deferred: Five-Lens Audit (15 findings, 3 hotspots)

### `src/services/subscription.c` — T4, T5/T24, T6, T27, T28, T31

- [ ] T4: Single-pass scan for monitored item collection
- [ ] T5/T24: Remove `fabs` usage (float in embedded)
- [ ] T6: 64-bit division in publish timer — use shift or platform-safe alternative
- [ ] T27: Deadband filter — review for correctness
- [ ] T28: Publish timer accuracy

**Lesson**: Split into atomic tasks: collect indices, change write loop, change cleanup loop. Verify each independently.

### `src/core/service_dispatch.c` — T8, T12, T13, T23, T25

- [ ] T8: Session ordering in dispatch
- [ ] T12: Certificate token `#ifdef` guard
- [ ] T13: Nonce stack zeroize
- [ ] T23: Dispatch scan optimization
- [ ] T25: Profile URI parse validation

**Lesson**: Any new `mu_secure_*` calls in `USER_AUTH` blocks need `#ifdef MUC_OPCUA_SECURITY` guard.

### `src/services/secure_channel.c` — T17

- [ ] T17: Channel ID entropy — update integration tests first (3 files assume `channel_id=1`), then apply counter

## Codacy

- [ ] Complete custom coding standard configuration in Codacy UI (finalize pattern exclusions)
- [ ] Tune Lizard complexity thresholds (currently: CCN>8, NLOC>50, params>5, file>500 — protocol code is naturally higher)

## Tech Debt

- [ ] `src/core/service_dispatch.c:284` — HistoryUpdate dispatch entry has wrong response_id: `MU_ID_HISTORYUPDATEREQUEST` (700) is used for both request and response; the response_id should be `MU_ID_HISTORYUPDATERESPONSE` (703). Pre-existing bug surfaced while reordering the table for binary search (T013). Out of scope for T013; fixing changes the wire-visible response NodeId and needs its own test.
- [ ] `subscription.c` is 1,692 lines — consider splitting into separate concerns (publish, monitor, filter)
- [ ] `service_dispatch.c` is large — service-specific dispatch could be extracted
- [ ] MISRA 15.6: 10 single-statement bodies remain in platform crypto adapters (wolfssl, host_crypto, mbedtls)
- [ ] MISRA 10.4: 2 `size_t` vs `0u` type inconsistencies in `subscription.c:1576,1610`
