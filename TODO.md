# TODO — muc-opcua

**Updated**: 2026-07-05 (spec 038 implementation)
**Source**: stale audit findings, interop gaps, hot-path audit, comp review

## Remaining Active Backlog

### Bugs (verified ACTIVE 2026-07-05)

| File | Finding |
|------|---------|
| `src/core/tcp_connection.c` | Hardcoded protocol version (`0`/`0u`) instead of `MU_OPC_UA_TCP_PROTOCOL_VERSION` |
| `src/core/service_dispatch.c` | ModifyMonitoredItems sampling interval=-1 resolves to 50ms instead of sub's publishing_interval_ms |
| `src/core/service_dispatch.c` | ModifyMonitoredItems timestampsToReturn decoded then `(void)`ed — not validated or applied |

### Hot-Path (from 2026-07-05 audit)

| ID | File | Severity | Issue |
|----|------|----------|-------|
| HP1 | `src/encoding/binary_variant.c` | HIGH | `calloc()` variant array leak — no `free()` in callers |
| HP2 | `src/services/read.c` | HIGH | Read cache disabled (always misses) |
| HP3 | `src/core/server.c`, `dispatch.c` | HIGH | Multiple redundant `get_tick_ms()` calls per poll |
| HP4 | `src/core/service_dispatch.c` | MEDIUM | Auth token extraction re-parses full NodeId |
| HP5 | `src/encoding/binary_reader.c` | MEDIUM | Per-primitive `ensure_bytes()` bounds checks |
| HP6 | `src/encoding/binary_writer.c` | MEDIUM | Per-primitive `ensure_space()` bounds checks |
| HP7 | `src/services/read.c` | MEDIUM | Full DataValue zero-init per result |
| HP8 | `src/core/server.c` | MEDIUM | `memmove()` of unconsumed recv buffer |
| HP9 | `src/core/server.c` | MEDIUM | Session timeout scans all slots |
| HP10 | `src/core/dispatch_attribute.c` | MEDIUM | Write type-check reads current value |
| HP11 | `src/address_space/address_space.c` | LOW | O(N^2) duplicate check at startup |
| HP12 | `server.c`, `message_chunk.c` | LOW | MessageSize parsed twice |
| HP13 | `src/core/server.c` | LOW | `listen()` inside `mu_server_init()` |

### Comprehensive Review (from 2026-07-05)

| ID | Severity | Issue |
|----|----------|-------|
| CQ6 | MEDIUM | `__builtin_ctz()` used 9x with no portability fallback |
| CQ7 | MEDIUM | Three duplicate LE uint32 write helpers |
| CQ8 | MEDIUM | Three copies of message-dispatch logic in server.c |
| AR3 | MEDIUM | `server_internal.h` unconditionally includes `subscription.h` |
| AR4 | MEDIUM | `handle_history_update` always returns GOOD |
| SE1 | MEDIUM | OPN nonce length mismatch (KDF_MAX_SEED=64 but nonces up to 128) |
| SE2 | MEDIUM | Password decrypt buffer undersized for 4096-bit RSA |
| SE3 | MEDIUM | Non-constant-time trust list `memcmp()` |
| SE4 | MEDIUM | `read_event_filter_body` no cap on select_count |
| UB3 | MEDIUM | Strict aliasing violation in binary_string.c |
| UB4 | MEDIUM | C11 requirement not documented |
| UB5 | MEDIUM | int32_t ← uint32_t cast impl-defined |
| UB6 | MEDIUM | `mu_binary_write_expanded_nodeid` drops NamespaceUri/ServerIndex flags |
| UB7 | MEDIUM | `calloc` pulls in heap allocator on embedded |
| TQ7 | MEDIUM | 6 placeholder-only tests provide false confidence |
| TQ8 | MEDIUM | Circular verification in response encoding tests |
| TQ9 | MEDIUM | No dispatch_subscription.c unit tests in isolation |
| D2 | MEDIUM | Zero spec grounding in binary_nodeid.c, binary_datavalue.c, uasc.c |
| D3 | MEDIUM | Zero spec grounding in public API headers (encoding.h, server.h) |
| D4 | MEDIUM | Sparse ADRs (only 2 for 37 specs) |

### Interop Test Hardening

- [ ] Audit all interop tests for per-service coverage
- [ ] Audit all `*_encode` unit tests for mandatory field verification
- [ ] Generate binary fixture for WriteResponse and add round-trip test
- [ ] Verify `reader->position == expected_length` in all response decode tests
- [ ] Add interop tests for Subscribe/Publish and Call service sets

### Documentation

- [ ] Add ADRs for: zero-heap design, adapter pattern, profile tier system, poll model
- [ ] Add spec grounding comments to `binary_nodeid.c`, `binary_datavalue.c`, `uasc.c`, `encoding.h`, `server.h`

## ✅ Completed in Spec 038

- CI/CD: removed `|| true` from 5 silent-failure steps, wired sanitizers, added coverage+fuzz jobs, added `standard` to matrix, set Debug build type
- Clang-tidy: added `--warnings-as-errors=*`
- Session timeout: gated on `MUC_OPCUA_SESSION_TIMEOUT` instead of `MUC_OPCUA_MULTI_CHUNK`
- `mu_session_create` deprecated in favor of `mu_session_create_with_identifiers`
- `mu_checked_memcpy_off` integer underflow guard (UB1)
- `mu_binary_read_expanded_nodeid` null-buffer guard (UB2)
- `handle_activate_session` decomposed into per-token helpers
- `mu_server_poll` common logic extracted from duplicated branches
- Version tracking: CHANGELOG.md + version.h.in
