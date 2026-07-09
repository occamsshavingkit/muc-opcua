# Spec 058: Capacity & OperationLimits Documentation (documentation CU for limits)

**Status**: Implemented | **Branch**: `058-documentation-facet`
**Roadmap**: Project A / A3 | **Implements**: spec 051 §3 — **reframed**, see below
**OPC UA**: OPC-10000-7 v1.05.02 revision history (Mantis 7003/4413) — mandatory
"documentation" CUs for limits; §1 Scope (lab/inspection testing category)

## Correction to spec 051 §3

Spec 051 §3 called this a "**Documentation Server Facet**". Verified against the
`opc-ua-reference`: **no such facet exists** — `search_cu("Documentation")` in
OPC-10000-7 returns nothing. What the v1.05.02 change log actually adds is
"mandatory **documentation CUs for limits**" (Mantis 7003 / ~4413): a server must
**document its operation limits and capacities** in product documentation. These
are attached to existing facets (e.g. the now-mandatory *Base Info Server
Capabilities 2* in Core 2022), not a standalone facet, and are verified by **lab
inspection** rather than CTT protocol traffic (OPC-10000-7 §1 recognizes the lab
testing category; per-CU test methods live in Compliance Part 8/9).

The deliverable is unchanged: a documentation artifact stating each profile's
limits/capacities. Only the naming is corrected (no "facet").

## Requirements

- **FR-001**: Create `docs/conformance/documentation.md` — the product-documentation
  artifact for the limits/capacity documentation CU. It lists, per profile
  (nano/micro/embedded/standard/full), the compiled OperationLimits and capacities.
- **FR-002**: Values are the **actually compiled** ones (from `capacities.h` /
  `config.h`), captured by a build probe — not restated by hand — so the doc can't
  drift from the binary. Storage bytes reference the README/size-ledger tables
  (single source) rather than duplicating them.
- **FR-003**: Note that OperationLimits values equal what the server enforces
  (spec 057) and are also exposed at runtime under `Server.ServerCapabilities`.
- **FR-004**: Bump the spec-version reference in the profile conformance docs to
  OPC-10000-7 v1.05.02 where they still say v1.04/2017 (spec 051 §4).

## Out of Scope

- CTT verification; the per-CU lab test procedure (Part 8/9).
- Documenting limits for services a profile does not compile.
