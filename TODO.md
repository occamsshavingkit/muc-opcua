# TODO — muc-opcua

**Updated**: 2026-07-04
**Source**: `docs/review/five-lens-audit-2026-07-04.md`, interop findings, code review

## Remaining Backlog

### Deferred audit findings

| File | Finding | Notes |
|------|---------|-------|
| `src/services/secure_channel.c` | T17 | Channel ID entropy. Fix 3 integration tests first (they assume channel_id=1). |
| `src/services/browse.c` | T22 | TypeDefinition cache. Requires adding field to `mu_node_t` struct. |

### Tech debt

- `src/core/service_dispatch.c` (being split in spec 032) — extract per-service dispatch into modules
- `src/services/subscription.c` (split completed in spec 032) — verify module boundaries

### Interop Test Hardening (HIGH)

**Problem discovered 2026-07-04**: WriteResponse was missing the mandatory `diagnosticInfos[]` field (OPC-10000-4 §5.11.4.2, OPC-10000-6 §5.2.5). Neither interop smoke tests nor unit tests caught this — open62541 client was the detector.

**Root causes identified**:
1. `tests/interop/interop_smoke.py` has **zero write tests** — no Write request is ever sent to the server
2. `tests/unit/test_write_decoder.c` validates `results[]` encoding but **never reads diagnosticInfos** — incomplete wire-level test
3. No test verifies a full WriteResponse binary round-trip against a known-good fixture

**Required**:
- [ ] Audit all interop tests: verify each service (Read, Write, Browse, Subscribe, Publish, Call, CreateSession, ActivateSession) has at least one wire-level round-trip test against a real client
- [ ] Audit all `*_encode` unit tests: verify each reads every mandatory field in the encoded response, including null/empty arrays
- [ ] Add write interop test: issue Write via asyncua/opcua-asyncio, verify server responds with well-formed WriteResponse
- [ ] Generate binary fixture for WriteResponse and add round-trip encode/decode test
- [ ] No silent failures: every test that reads a binary response must verify `reader->position == expected_length` at the end to catch trailing/missing fields
