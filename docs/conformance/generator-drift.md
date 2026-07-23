<!-- markdownlint-disable MD013 -->

# Manifest generator drift

`scripts/profile_manifest/generate.py` derives seven artifacts from
`profiles/opcua-profile-manifest.yaml`. Several of them no longer match what
the generator produces, so `validate.py --check-generated` and
`--check-profiles` fail on `main`.

This is why the `manifest-validation` CI job runs only `--manifest-only`,
`--check-claims` and `--check-capacities` today. Enabling the other two is the
last step of the repair described below.

## Current state

| Artifact | Status |
| --- | --- |
| `docs/conformance/completion.md` | in sync |
| `include/muc_opcua/capacities.h` | in sync |
| `configs/{nano,micro,custom}.defconfig` | in sync |
| `Kconfig` | **drift — 419 generated vs 2063 committed** |
| `configs/{embedded,standard,full}.defconfig` | drift |
| `tests/conformance/claim_test_map.md` | drift |
| `docs/conformance/opc-profile-roadmap.md` | drift |
| `docs/build-and-gating.md` (generated section) | drift |

## How it happened

Generation last matched at `4122c923` (2026-07-18). The drift arrived in two
distinct waves, which is why this is not a single revert.

**Wave 1 — hand-edited Kconfig (2026-07-19 → 07-21).** Across roughly fifteen
commits the committed `Kconfig` grew 1938 → 2048 lines while generator output
stayed flat at ~1941. Content was added directly to `Kconfig` without a
corresponding manifest change, so the generator cannot reproduce it. Divergence
here is ~110 lines and accumulated gradually.

**Wave 2 — facets became unselectable (2026-07-21 → 07-22).** `b4787bcf` moved
the facet items off `deferred`/`in-progress`; they now sit at `documented` (9)
and `deferred` (11). `generate.py` defines
`_SELECTABLE_STATES = ("claimed", "implemented", "deferred")`, so
`_selectable_facet_toggles()` returns **0**, every facet toggle disappears, and
everything nested under those facet blocks stops being emitted. Generated output
collapsed from ~1941 lines to 419 (35 `config` entries vs 118 committed, 2
`comment` entries vs 116).

Wave 2 dominates the line count but wave 1 is the one that needs judgement.

## Repair, in order

Deliberately three tasks, not one. Task A is mechanical and unblocks measuring
task B; only task C should touch the committed artifacts.

**A. Restore facet selectability.** Decide whether facets belong in a selectable
`implementation_state`, or whether `_SELECTABLE_STATES` should include
`documented`. One decision, roughly a one-line change. Done when
`_selectable_facet_toggles()` returns 9 again and generated `Kconfig` is back to
~1941 lines. Note `b4787bcf` briefly introduced a `declared` state that is not
in `model._ALLOWED_IMPLEMENTATION_STATES` — confirm no entry still carries it.

**B. Reconcile the hand-edited Kconfig content.** With A done, the remaining
delta is wave 1. For each difference decide: express it in the manifest so the
generator emits it, or teach the generator to emit it. This is per-symbol review
work and splits cleanly across the ~15 commits that introduced it — it does not
need to land in one change.

**C. Regenerate and lock the gate.** Regenerate all seven artifacts, commit
them, then add `--check-generated` and `--check-profiles` to the
`manifest-validation` CI job so drift cannot silently return.

## Guardrail while the drift exists

Until C lands, `--check-generated` cannot verify a manifest change. Verify by
**delta** instead: run the generator over the pristine manifest and over the
edited one and compare the two outputs to each other. A reconciliation-only
change should leave `Kconfig`, `claim_test_map`, `capacities.h` and all six
defconfigs byte-identical.
