<!-- markdownlint-disable MD013 -->

# Manifest generator drift — RESOLVED

`scripts/profile_manifest/generate.py` derives seven artifacts from
`profiles/opcua-profile-manifest.yaml`. They had drifted, so
`validate.py --check-generated` and `--check-profiles` failed and were kept out
of CI. **This is now repaired**: every committed artifact matches generator
output, both checks run in the `manifest-validation` CI job, and all five
profiles build and test identically to before the repair.

This document is kept as the record of what the drift was and how it was fixed,
since the same failure modes can recur.

## What the drift was

Generation last matched at `4122c923` (2026-07-18). Two commits broke it, and
neither regenerated the artifacts:

**Wave 1 — hand-edited Kconfig (2026-07-19 → 07-21).** Four feature toggles were
written directly into `Kconfig` with real `depends on`/`default` expressions and
no manifest entry at all: `MUC_OPCUA_CU_USER_TOKEN_JWT`,
`MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL`, `MUC_OPCUA_MDNS_DISCOVERY`,
`MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER`. Each gates real compiled code
(1–7 `#ifdef`s), so a plain regenerate would have silently dropped them.

**Wave 2 — `b4787bcf` (2026-07-21).** It flipped the nine *implemented* facets
to `implementation_state: documented` and regenerated nothing. `documented`
renders as a non-selectable `(DOCUMENTED)` comment (spec 073 / PR #314), and a
facet in neither `_SELECTABLE_STATES` nor `_UNSELECTABLE_STATES` is emitted as
*nothing at all* — so the facet menuconfigs and every CU inside them vanished,
collapsing generated output from ~1900 lines to 419. The 11 genuinely
unimplemented facets were simultaneously mislabeled `deferred` (selectable)
instead of `unimplemented` (comment).

## How it was fixed

- **Facet states corrected.** The 9 implemented facets → `implemented`
  (selectable, honest, no `backing_tests` requirement); the 11 unimplemented
  facets → `unimplemented` (rendered as `(NOT IMPLEMENTED)` comments, matching
  their real status).
- **Wave-1 symbols reconciled into the manifest** as `optimization` items (the
  same kind as `SECURE_CHANNEL_CRYPTO` — a security build-gate), reproducing each
  symbol's exact `depends_on` and full-profile default. `optimization` items use
  `kconfig_symbol` verbatim, so the emitted symbol matches the code's `#ifdef`.
- **Two `satisfied_by` anomalies fixed.** `opc_cu_3072`/`opc_cu_3073` were
  `claimed` while pointing `satisfied_by` at `service_read`/`service_register_nodes`,
  so both the canonical CU and the project CU emitted the same symbol — a
  duplicate config that only surfaced once the facets were selectable again.
  Set to `documented` (comment), matching every other reconciliation CU.
- **All seven artifacts regenerated and committed.**

## Build-neutrality

The repair changes menu structure and adds ~50 inert (menu-only or
`.disabled`-stub-only) CU toggles, but changes **no compiled code** in any
profile. Verified two ways: (1) for every profile, the resolved effective config
(`gen_config.py` → `-D` set) has no compiled-code symbol changing value versus
the pre-repair baseline; (2) all five profiles build and pass their full test
suite (nano 104, micro 124, embedded 125, standard 125, full 145).

## Guardrail

`validate.py --check-generated` and `--check-profiles` now run in CI. Any
manifest change must be followed by `scripts/profile_manifest/generate.py` or CI
fails. When adding a build-gate for an optional feature, add it to the manifest
(an `optimization` item, or a CU under a selectable facet) — never hand-edit
`Kconfig`.
