# Phase 1 Data Model — Optimization Audit Remediation

This feature is a remediation, not a new domain model. The "entities" below are the
new or changed *internal* data structures. All are static or caller-provided — no
heap (FR-019). Field names are indicative; final names are set during implementation.

---

## E1 — Address-space NodeId index (`mu_address_space_index_t`)

**Purpose**: Enable sub-linear NodeId resolution over a caller-provided const node
array (D3 / FR-009).

| Field | Type | Notes |
|---|---|---|
| `order` | `opcua_uint16_t[MU_MAX_ADDRESS_SPACE_NODES]` | node indices sorted by NodeId sort key |
| `count` | `size_t` | number of indexed nodes (= `node_count` if ≤ cap) |
| `indexed` | `bool` | false ⇒ fall back to linear scan (node_count > cap) |

- **Lifecycle**: built once at `mu_address_space_validate` / server init from the
  const `mu_address_space_t`; immutable thereafter.
- **Validation**: sort key derived from `(namespace_index, identifier_type,
  numeric value | deterministic string hash)`; a binary-search hit is confirmed with
  `mu_nodeid_equal` before returning (collision-safe).
- **Relationships**: references `mu_address_space_t.nodes` by index; never copies node
  data.
- **Storage/size**: `MU_MAX_ADDRESS_SPACE_NODES × 2 B` (e.g. 64 ⇒ 128 B), static.

---

## E2 — Per-channel cipher context (opaque) on `mu_secure_channel_t`

**Purpose**: Cache the AES-256 key schedule once per secure channel (D2 / FR-012).

| Field | Type | Notes |
|---|---|---|
| `cipher_ctx_client` | `opcua_byte_t[MU_CIPHER_CTX_SIZE]` | opaque, adapter-owned layout (inbound) |
| `cipher_ctx_server` | `opcua_byte_t[MU_CIPHER_CTX_SIZE]` | opaque (outbound) |
| `cipher_ctx_valid` | `bool` | true once `cipher_ctx_init` succeeded |

- **Guarded by** `#ifdef MICRO_OPCUA_SECURITY`.
- **Lifecycle**: initialized at OPN (where `client_keys`/`server_keys` are derived);
  zeroized + freed at channel close/reuse (overlaps E-zeroization, D6/FR-007).
- **Fallback**: if the adapter provides no cipher-context functions, these fields are
  unused and the codec calls the stateless `aes256_cbc_*` per message.
- **Constraint**: `MU_CIPHER_CTX_SIZE` ≥ the largest supported backend schedule, with
  a compile-time assert in the adapter binding.

---

## E3 — MonitoredItem resolved-node reference (on `mu_monitored_item_t`)

**Purpose**: Remove the per-tick address-space lookup from sampling (D-cache / FR-010).

| Field | Type | Notes |
|---|---|---|
| `resolved_node` | `const mu_node_t *` | cached at CreateMonitoredItems time |

- **Lifecycle**: set when the item is created (after the node is resolved once);
  read by the sampling timer; cleared on item delete. The address space is static for
  the server's lifetime, so the pointer stays valid.
- **Relationships**: points into `mu_address_space_t.nodes`.
- **Note**: existing shallow value-string caching (T21) is documented as a callback
  lifetime contract, not changed here unless promoted into scope.

---

## E4 — Server-owned secure scratch (on `struct mu_server`)

**Purpose**: Relocate large OPN-path scratch off the call-chain stack (D1 / FR-001).

| Field | Type | Notes |
|---|---|---|
| `secure_scratch` | `opcua_byte_t[MU_SECURE_SCRATCH_SIZE]` | shared response/OPN scratch, replaces stack arrays |

- **Guarded by** `#ifdef MICRO_OPCUA_SECURITY`.
- **Safety invariant**: one TCP connection, one `secure_channel`, one chunk processed
  per `mu_server_poll` ⇒ no concurrent use of the shared scratch (verified, D1).
- **Sizing**: sized to the worst-case secure response; asymmetric scratch right-sized
  to the real RSA block (256 B for 2048-bit), not 4096.

---

## E5 — Sticky codec status (on `mu_binary_reader_t` / `mu_binary_writer_t`)

**Purpose**: Collapse per-call error checks into one (D8 / T6, FR-015) and harden
bounds (FR-003).

| Field | Type | Notes |
|---|---|---|
| `status` | `opcua_statuscode_t` | sticky; set on first failure, primitives no-op afterward |

- **Behaviour**: `ensure_bytes`/`ensure_space` still run every call (bounds unchanged);
  the bound test uses overflow-safe `count > length - position`. Handlers check
  `reader/writer->status` once at the end.
- **Invariant**: byte-for-byte output identical to the pre-change codec for all valid
  inputs (SC-003).

---

## Configuration knobs (new, in `config.h`)

| Knob | Default (indicative) | Affects |
|---|---|---|
| `MU_MAX_ADDRESS_SPACE_NODES` | tuned to base+headroom (e.g. 64) | E1 index size / fallback threshold |
| `MU_CIPHER_CTX_SIZE` | backend max schedule | E2 cipher context |
| `MU_SECURE_SCRATCH_SIZE` | worst-case secure response | E4 scratch |
| `MICRO_OPCUA_STATUS_STRINGS` | OFF (embedded) | gates `mu_status_name` (D9) |
| `MICRO_OPCUA_LTO` (CMake) | OFF | opt-in LTO (D10) |

All are `-D`-overridable; defaults chosen so the existing profiles' behaviour and the
default example remain within the size budget.
