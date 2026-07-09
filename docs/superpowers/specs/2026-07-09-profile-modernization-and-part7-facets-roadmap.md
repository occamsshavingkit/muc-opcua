# Roadmap: Profile Modernization + Part 7 Facet Rollout

**Date**: 2026-07-09
**Status**: Approved design — parent roadmap for two sequential project tracks
**Author**: brainstormed with occamsshavingkit
**Supersedes/Anchors**: TODO.md "Profile Gap Analysis (2026-07-09)", specs 050/051 drafts

## Purpose

Two related bodies of work, run **strictly in sequence** (Project A fully completes
before Project B starts):

- **Project A — Profile modernization.** Bring the Nano/Micro/Embedded profile
  targets up to date with OPC-10000-7 **v1.05.02** (current), from the v1.04
  (2017) baseline they target today.
- **Project B — Part 7 Server Facet rollout.** Implement additional Part 7 Server
  Facets, prioritized by real-world deployment frequency (size ignored for
  *ordering*), until the **full-profile binary reaches a 128 KiB `.text` stopper**.

## Decisions (locked)

1. **Sequential, A-before-B.** ECC-curve25519 is a hard v1.05.02 Nano mandatory;
   profiles are not "current" until it ships. Project B waits for Project A to
   complete (all four A-specs merged, including ECC).
2. **Facet priority = deployment-frequency ranking** (see Project B table).
3. **One spec per facet/feature** — each gets its own `NNN-*` spec → plan →
   implement → PR cycle, matching the existing 001–051 pattern. This lets the
   Project B rollout stop cleanly the moment the size budget is hit.
4. **No self-implemented cryptography.** All ECC (and any future crypto) is
   implemented by binding the `mu_crypto_adapter_t` vtable to the already-linked
   **mbedTLS / wolfSSL** primitives. Hand-rolled curve/field math is prohibited
   and is an explicit review gate on spec 059 (ECC, A4).

## Assumptions (baked in)

- The **128 KiB stopper** is measured against **full-profile archive `.text`**
  (the flash metric tracked in `docs/size/feature-size-ledger.md`), currently
  **67,337 B** → ~63 KB of headroom.
- New facets land **gated behind their existing `MUC_OPCUA_*` flags** and compile
  only into `standard`/`full`. nano/micro/embedded flash stays flat; only `full`
  (and `standard`) grow toward the stopper.

## Closing out the open speckit flows

| Item | Current state | Terminal action |
|------|---------------|-----------------|
| **PR #273** | Docs-only (TODO gap analysis + spec 050/051 drafts) on `054-profile-gap-analysis` | Merge first — written foundation for both tracks; CI-safe (no source touched). |
| **Spec 050** (Security Time Sync) | Draft, no implementation | Implemented as spec **055** (A1). |
| **Spec 051** (v1.05.02 migration) | Draft *migration plan* spanning 4 phases | Not a single feature — its phases become specs **057/058/059** (A2/A3/A4). Remains the parent plan of record. |

> **Numbering note:** branches `052`-`054` are taken (`052-fix-profile-gating`,
> `053-fix-client-clang-tidy`, `054-profile-gap-analysis`), and **`056` landed
> out-of-band** as `056-fix-capacity-propagation` (the capacity-resolution cascade,
> PR #275 — a prerequisite cleanup surfaced while scoping this roadmap). So this
> roadmap's specs are **055** (time sync) then **057** onward; `056` is spoken for.

## Project A — Profile modernization (sequential; blocks Project B)

Each row = its own spec → plan → implement → PR. Order = cheap-and-mandatory
first, big refactor last so it never stalls the smaller conformance wins.

| # | Spec | Covers (051 §ref) | Effort | Size |
|---|------|-------------------|--------|------|
| A1 | `055-security-time-sync` | Spec 050 in full: read+validate OPN `clientTimestamp` vs server time within `MU_TIME_SYNC_MAX_CLOCK_SKEW_MS` (default 300000); `Bad_SecurityChecksFailed` on drift | ~1d | +~200 B flash, 0 RAM |
| A2 | `057-base-info-capabilities` | 051 §2/§7: verify/complete the 6 OperationLimits (`MaxNodePerRead`, `MaxNodesPerWrite`, `MaxNodesPerSubscription`, `MaxNodesPerBrowse`, `MaxArrayLength`, `MaxStringLength`) advertised in `Server.ServerCapabilities` via `base_nodes.c`; now Mandatory | ~0.5d | ~0 |
| A3 | `058-documentation-facet` | 051 §3: new `docs/conformance/documentation.md` capacity tables per profile; bump profile-doc + traceability spec-version references to v1.05.02 | ~0.5d | 0 (docs only) |
| A4 | `059-ecc-security-policies` | 051 §4 (PG13/PG14): `SecurityPolicy - ECC-curve25519` (Nano-mandatory) + `ECC-nist256`. **Extend `mu_crypto_adapter_t` with ECDH-derive / ECC-sign / ECC-verify slots; bind to mbedTLS (`mbedtls_ecdh_*`, `mbedtls_ecdsa_*`) and wolfSSL (`wc_ecc_*`, `wc_curve25519_*`). NO hand-rolled crypto.** Wire ECC key derivation into `security_policy.c` / `key_derivation.c`. | **~5d** | measured; gated, standard/full only |

**Definition of done for Project A:** A1–A4 PRs merged; per-profile ctest matrix
green; ECC round-trip tests pass against a known-answer vector; profile
conformance docs reference v1.05.02.

## Project B — Part 7 Server Facet rollout (starts only after A4 merges)

Budget-gated. Implement in this order; **remeasure `full` archive `.text` after
each spec**; **stop when it reaches ~128 KiB**. Remaining facets stay
documented-but-deferred in TODO.md.

| # | Spec | Facet | Flag | Effort |
|---|------|-------|------|--------|
| B1 | `060-facet-data-access` | Data Access (AnalogItem/discrete, EURange, percent deadband) | `MUC_OPCUA_DATA_ACCESS` | ~3d |
| B2 | `061-facet-event-subscription` | Standard Event Subscription (EventFilter where-clause eval) | `MUC_OPCUA_EVENT_FILTER_WHERE` | ~3d |
| B3 | `062-facet-method-server` | Method Server (arbitrary user-registered methods) | `MUC_OPCUA_METHOD_SERVER` | ~3d |
| B4 | `063-facet-enhanced-datachange` | Enhanced DataChange Subscription 2017 (higher capacity, Monitor Items 500) | capacity | ~2d |
| B5 | `064-facet-base-server-behaviour` | Base Server Behaviour (diagnostics objects, config mgmt) | `MUC_OPCUA_SERVER_DIAGNOSTICS` | ~2d |
| B6 | `065-facet-reverse-connect` | Reverse Connect (server-initiated TCP; stub test exists) | `MUC_OPCUA_REVERSE_CONNECT` | ~3d |
| B7 | `066-facet-client-redundancy` | Client Redundancy (TransferSubscriptions profile wiring) | `MUC_OPCUA_REDUNDANCY` | ~2d |

**Ranking rationale (deployment frequency):** Data Access appears in nearly every
industrial server (analog/discrete process values); server-side event filtering
and user Methods are the next most common; enhanced subscriptions, diagnostics,
reverse-connect, and redundancy tail off from there.

**Size gate (every B-spec):** each spec's final task runs
`bash scripts/measure_size.sh full` and records `.text`. If ≥131,072 B, the
rollout halts; the current spec ships and later B-rows are marked deferred.

## Testing strategy (both tracks)

- Test-first per feature (project convention): tests land red, feature turns
  them green, per-profile ctest matrix stays green (nano/micro/embedded/
  standard/full).
- Internal tests remain the conformance evidence (no external CTT).
- ECC (A4) additionally requires known-answer vectors for X25519 / P-256 to
  prove the library binding is correct.

## Out of scope

- PubSub profile updates (separate track).
- Client profile updates (spec 049 line).
- v1.05.02 additions beyond Nano/Micro/Embedded mandates: Authorization Service,
  KeyCredential, User Role Management, GDS facets (PG17–PG20) — deferred.
- External CTT verification.
