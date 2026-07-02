# Contract: Claim→test map enforcement

**Feature**: 028 · **FR**: FR-002, FR-005, FR-010 · replaces the substring-matching
weakness in `test_traceability_docs.c`.

## Artifact

A machine-readable table (checked manifest or ctest labels) with one row per
claimed conformance unit / behavior:

```
claim_id | opc_ua_section | profiles | backing_tests | kind(happy|negative|roundtrip)
```

## Checker semantics (a test in tests/conformance/, compiled per profile)

| Condition | Required result |
|-----------|-----------------|
| Row's `profiles` includes the current build profile AND ≥1 `backing_test` is registered and passes in this build | row OK |
| Row applies to this profile but has NO backing test that runs here | **FAIL**, printing the unmapped `claim_id` + profile |
| Row is covered only in a superset profile | does NOT satisfy the row for this profile (must run here) |
| A conformance doc claims a unit with no row in the map | **FAIL** (every claim must be mapped) |
| A StatusCode/type is listed as supported but has no forcing test row | **FAIL** (FR-005) — resolved by adding a test or removing the claim |

## Honesty rules (FR-010)

- Correcting a claim by removal MUST keep the anti-over-claim guards intact
  (no unqualified compliance or external-tool-verified wording without external
  evidence).
- Conformance status stays **profile-targeting**.

## Acceptance

- Removing a backing test (or its profile gate) for a mapped claim makes the
  claimed profile's build fail with a message naming the unit.
- Every row in `docs/conformance/*` claim tables has a map entry; every map entry
  resolves to a passing test in each listed profile.
