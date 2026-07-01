---

description: "Task list for feature implementation"
---

# Tasks: Rename micro-opcua to muc-opcua

**Input**: Design documents from `/specs/024-rename-muc-opcua/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: This feature changes zero OPC UA protocol behavior (spec OPC-001
through OPC-005), so no new protocol/wire tests are required. The one new test
artifact is the stale-old-name regression guard (FR-008), which — per
Constitution Principle IV's test-first spirit — is written and confirmed
**failing** against the pre-rename tree (T002) before the rename proceeds, then
confirmed **passing** once the rename is complete (T031).

**Organization**: Tasks are grouped by user story per spec.md priorities (US1,
US2 = P1; US3 = P2). Purely mechanical, judgment-free substitutions (same
five-literal-string pattern from research.md Decision 1 applied identically
everywhere in a subtree) are grouped into one task per directory tree — splitting
those further would fragment a single indivisible operation with no independent
verification value. Prose-bearing documentation, where a human/LLM must actually
read surrounding text rather than blind-substitute, gets one task per file or
tightly-related file group instead, so each task has a real, reviewable unit of
judgment.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3)
- All five literal-string patterns (research.md Decision 1) apply in every task
  below unless the task says otherwise: `MICRO_OPCUA` -> `MUC_OPCUA`,
  `micro_opcua` -> `muc_opcua`, `micro-opcua` -> `muc-opcua`, `MicroOpcUa` ->
  `MucOpcUa`, `Micro-OPCUA` -> `Muc-OPCUA`.
- The bare word `micro` as an OPC UA profile-tier value (`MUC_OPCUA_PROFILE=micro`,
  `make micro`, "Micro [Embedded Device 2017 Server] Profile" prose) is explicitly
  OUT of scope for every task (research.md Decision 2) — no task should touch it.

## Path Conventions (post-rename)

- **Public API**: `include/muc_opcua/`
- **Core protocol**: `src/core/`, `src/encoding/`, `src/services/`
- **Address space**: `src/address_space/`
- **Security interfaces**: `src/security/`
- **Platform adapters**: `platform/pico/`, `platform/arduino/`
- **Examples**: `examples/minimal_server/`, `examples/pubsub_server/`
- **Tests**: `tests/unit/`, `tests/integration/`, `tests/fuzz/`, `tests/benchmark/`, `tests/support/`, `tests/interop/`
- **Documentation**: `docs/**` (except the per-feature `docs/traceability/NNN-*.md` files), `README.md`, `ROADMAP.md`, `CLAUDE.md`, `AGENTS.md`
- **Frozen historical record — not edited at all, same as past git commits**: `specs/001-023/**`, and the per-feature `docs/traceability/NNN-*.md` files

---

## Phase 1: Setup

**Purpose**: Establish the pre-rename baseline and the regression guard that
proves the rename is complete, before any renaming happens.

- [X] T001 [P] Run `scripts/measure_size.sh all` and record its output (append a
  dated "pre-024-rename baseline" note, not a numeric change, to
  `docs/size/feature-size-ledger.md`, phrased using the new `muc_opcua`/
  `MUC_OPCUA_*` names throughout so it introduces no old-name text of its own)
  as the reference point T012/T033 diff against. This is the only task that adds
  new content to this file; T018 (US2) only fixes old-name references in the
  file's *pre-existing* prose and never touches this new note
- [X] T002 Implement the stale-old-name regression guard per
  `contracts/regression-guard-contract.md` (new `tests/unit/test_no_stale_project_name.c`
  following the existing `tests/unit/test_conformance_docs.c` pattern, or a new
  `tests/tools/check_no_stale_name.sh` if scanning non-C files like
  `.github/workflows/ci.yml` proves awkward as a C unit test), scanning the
  tracked tree **excluding** `build*/`, `.git/`, `specs/001-023/**`, and the
  per-feature `docs/traceability/NNN-*.md` files (the latter two are frozen
  history — see spec.md FR-006/FR-008 and research.md Decision 4 — and are
  expected to retain the old name forever, the same way old commits aren't
  rewritten) for the five literal patterns; run it now against the current
  pre-rename tree and confirm it **fails loudly** on the still-unrenamed living
  files, recording the failure output as proof the guard actually detects what
  it's meant to detect (and, incidentally, that its exclusion list is already
  correctly scoped, not accidentally hiding everything)

**Checkpoint**: Baseline captured; regression guard exists and is confirmed to
catch the old name.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Move the public header tree and CMake module files, and repoint the
top-level build config at them. Every other task depends on these paths existing.

**CRITICAL**: No user story task can begin until this phase is complete. The
build is expected to be **transiently broken** after this phase (src/, tests/,
platform/, examples/ still `#include "micro_opcua/...".h`) — that's expected for
a rename and is resolved by Phase 3 (US1), not a defect in this phase.

- [X] T003 `git mv include/micro_opcua/ include/muc_opcua/` (including
  `git mv include/muc_opcua/micro_opcua.h include/muc_opcua/muc_opcua.h` for the
  umbrella header), then fix every `MICRO_OPCUA_*_H` header guard and any
  internal `micro_opcua`/`MICRO_OPCUA` prose/path reference within all 14 files
  now under `include/muc_opcua/**` (including `include/muc_opcua/services/`)
- [X] T004 [P] `git mv cmake/MicroOpcUaOptions.cmake cmake/MucOpcUaOptions.cmake`,
  `git mv cmake/MicroOpcUaCodegen.cmake cmake/MucOpcUaCodegen.cmake`,
  `git mv cmake/MicroOpcUaStaticAnalysis.cmake cmake/MucOpcUaStaticAnalysis.cmake`,
  `git mv cmake/MicroOpcUaWarnings.cmake cmake/MucOpcUaWarnings.cmake`, updating
  every `MICRO_OPCUA_*` reference inside each renamed file
- [X] T005 Update `CMakeLists.txt`: `project(micro_opcua ...)` ->
  `project(muc_opcua ...)`, every `MICRO_OPCUA_*` option/cache-variable name ->
  `MUC_OPCUA_*` (leaving `MUC_OPCUA_PROFILE`'s accepted values `nano`/`micro`/
  `embedded`/`full`/`custom` untouched per research.md Decision 2), and the four
  `include(cmake/MicroOpcUa*.cmake)` call sites -> the T004 filenames (depends on
  T003, T004)
- [X] T006 [P] Update `src/CMakeLists.txt`: `add_library(micro_opcua STATIC ...)`
  -> `add_library(muc_opcua STATIC ...)` and
  `target_include_directories(micro_opcua PUBLIC ...)` -> `target_include_directories(muc_opcua PUBLIC ...)`
  (depends on T003)

**Checkpoint**: `include/muc_opcua/` and the CMake project/module identity exist
and are internally consistent; downstream `#include`/macro references in `src/`,
`tests/`, `platform/`, `examples/` still point at the old path (expected, fixed
next).

---

## Phase 3: User Story 1 - Integrator builds the renamed library from a fresh clone (Priority: P1) [MVP]

**Goal**: A fresh clone configures and builds cleanly using only `MUC_OPCUA_*`
CMake options and `muc_opcua/` include paths, with `ctest` green.

**Independent Test**: Clone fresh, run the `quickstart.md` "Build from a fresh
clone" commands, confirm success and zero `micro_opcua/` includes remain
(SC-001, SC-004).

### Implementation for User Story 1

- [ ] T007 [P] [US1] Update every `#include "micro_opcua/...".h` and every
  `MICRO_OPCUA_*` macro reference across all of `src/**` (`core/`, `encoding/`,
  `services/`, `address_space/`, `security/`, `platform/`, `generated/`) to
  `muc_opcua/`/`MUC_OPCUA_*` (depends on T003)
- [ ] T008 [P] [US1] Update every `#include` and `MICRO_OPCUA_*` reference across
  `tests/unit/`, `tests/integration/`, `tests/fuzz/`, `tests/benchmark/`, and
  `tests/support/` (excludes `tests/interop/`, covered under US3 T028) (depends on
  T003)
- [ ] T009 [P] [US1] Update every `#include` and `MICRO_OPCUA_*` reference across
  `platform/pico/` and `platform/arduino/` (depends on T003)
- [ ] T010 [P] [US1] Update every `#include` and `MICRO_OPCUA_*` reference across
  `examples/minimal_server/` and `examples/pubsub_server/` (depends on T003)
- [ ] T011 [US1] Configure and build the host profile fresh
  (`cmake -S . -B build/host -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_BUILD_EXAMPLES=ON -DMUC_OPCUA_PLATFORM=host`,
  `cmake --build build/host`, `ctest --test-dir build/host`) using only
  `MUC_OPCUA_*` options; fix any residual old-name reference the T007-T010
  mechanical passes missed until the build and full test suite are green
  (depends on T005, T006, T007, T008, T009, T010)
- [ ] T012 [US1] Re-run `scripts/measure_size.sh all` and diff its console
  output against the T001 baseline; confirm zero `.text`/`.data`/`.bss` delta
  for nano/micro/embedded/full-featured (SC-005). **Verification only — this
  task does not edit `docs/size/feature-size-ledger.md`**; recording the
  confirmed-clean result in that file is T018's job, not this one, so the two
  never race on the same file (depends on T011)

**Checkpoint**: User Story 1 is independently complete — a fresh clone builds
and tests green under the new names, with proven zero size impact.

---

## Phase 4: User Story 2 - Reader trusts current documentation and links (Priority: P1)

**Goal**: Every living document consistently says `muc-opcua`/`muc_opcua`/
`MUC_OPCUA`, with a migration note explaining the breaking rename.
`specs/001-023` and the per-feature `docs/traceability/NNN-*.md` files are
frozen history and are explicitly left alone — same as old commits.

**Independent Test**: Grep README, CLAUDE.md, AGENTS.md, ROADMAP.md, and
`docs/**` (excluding the per-feature `docs/traceability/NNN-*.md` files) for
the five literal patterns; confirm zero matches outside the migration note
(SC-002). Separately confirm `specs/001-023/**` and the per-feature
traceability docs are byte-for-byte unchanged (`git diff --stat` against the
pre-rename tree shows no touches to those paths).

### Implementation for User Story 2

- [ ] T013 [P] [US2] Update `README.md` (title, prose, status callouts, any
  command examples)
- [ ] T014 [P] [US2] Update `ROADMAP.md` (title `# Micro-OPCUA Roadmap` ->
  `# Muc-OPCUA Roadmap`, body references)
- [ ] T015 [US2] Update `.specify/memory/constitution.md`: title string
  (`# micro-opcua Constitution` -> `# muc-opcua Constitution`). Per research.md
  Decision 8, this is a **PATCH** version bump (1.0.0 -> 1.0.1, "Last Amended"
  updated to this feature's date) under the constitution's own Versioning
  policy (wording-only correction, no principle change) — not a full
  MINOR/MAJOR Amendment. Update the Sync Impact Report comment at the top of
  the file to record: the version change, that no principle changed, and a
  one-line pointer to this feature (024-rename-muc-opcua) as the rationale.
  The Governance section's ceremony is satisfied jointly by this task (version
  + rationale pointer), T016 (template review), and T024 (migration note) — no
  separate rationale document is needed (not marked `[P]`: sequence after T016
  so the Sync Impact Report can truthfully say the templates were already
  reviewed)
- [ ] T016 [P] [US2] Update `.specify/templates/plan-template.md` and
  `.specify/templates/tasks-template.md` path examples
  (`include/micro_opcua/` -> `include/muc_opcua/`) so future specs generated from
  these templates reference the current path
- [ ] T017 [P] [US2] Update `docs/conformance/status.md`, `services.md`,
  `profile-nano.md`, `profile-micro.md`, `profile-embedded.md`,
  `identity-policy.md`, `async-opcua-inventory.md` — verify every "Micro
  [Embedded Device 2017 Server] Profile" / "Micro profile" prose reference is
  left untouched (research.md Decision 2) while identifier/path/URL references
  are corrected
- [ ] T018 [P] [US2] Update `docs/size/feature-size-ledger.md`,
  `audit-hardening-baseline.md`, `host-minimal-server.md`,
  `pico-minimal-server.md` — prose/command/path references only; measured
  numeric values (bytes, percentages) are historical facts and stay unchanged
- [ ] T019 [P] [US2] Update `docs/traceability/sections-to-files.md`,
  `files-to-sections.md`, and `conformance-claims.md` — these are living
  index files describing *current* cross-references, in scope like the rest of
  `docs/`. **Do NOT touch** the numbered per-feature files
  (`004-optimization-fixes.md`, `005-embedded-profile-completion.md`,
  `007-optional-write-service.md`, `008-user-identity-auth.md`,
  `012-opcua-pubsub.md`, `013-advanced-security.md`,
  `019-fix-conformance-size.md`, `022-optimize-hot-paths.md`,
  `023-conformance-docs-subscriber.md`) — those are frozen history for
  already-shipped features, left completely alone (research.md Decision 4/5)
- [ ] T020 [P] [US2] Update `docs/validation/*.md` (audit-hardening.md,
  audit-hardening-closure.md, fuzz.md, host-tests.md, interop.md,
  pico-cross-compile.md, sanitizers.md, static-analysis.md),
  `docs/review/*.md`, and `docs/benchmarks/*` (speed-benchmark.md,
  speed-baseline.json)
- [ ] T021 [P] [US2] Update `docs/architecture.md`, `docs/api-reference.md`,
  `docs/getting-started.md`, `docs/integration-guide.md`
- [ ] T022 [P] [US2] Update root-level `optimize-hot-paths.md`
- [ ] T023 [US2] **Do not edit** `specs/001-minimal-embedded-server/**` through
  `specs/023-conformance-docs-subscriber/**`, or the per-feature
  `docs/traceability/NNN-*.md` files from T019 — per explicit user direction
  (research.md Decision 4), these are frozen historical record, left alone
  exactly like already-merged git commits, even where they literally quote an
  old macro name, include path, or command that would no longer work verbatim
  against the renamed project. This task is a **verification**, not a
  modification: confirm no other task in this feature touched these paths
  (`git diff --stat <pre-rename-ref>..HEAD -- specs/ docs/traceability/` should
  show no numbered `specs/NNN-*` or `docs/traceability/NNN-*.md` entries)
  (depends on T013-T022, since it verifies none of them strayed outside their
  documented scope)
- [ ] T024 [US2] Write the breaking-change migration note (FR-007): a new
  `CHANGELOG.md` entry (create the file if it doesn't exist) or
  `docs/migration-024-muc-opcua-rename.md`, stating exactly which identifiers
  changed (macro prefix, include path, CMake project/target/module names) and
  that there is no compatibility shim. **Must not alter, upgrade, or downgrade
  the project's profile-targeting conformance-status claim while describing
  this change (OPC-005)** — state plainly, in this note, that the rename
  changes no OPC UA wire behavior, StatusCode, service, or conformance claim,
  matching OPC-001 through OPC-005 in spec.md. Resolve and apply the
  regression-guard allow-list technique from
  `contracts/regression-guard-contract.md` (split-string spelling of the old
  name vs. a single named-file path exclusion in T002's guard) so this note
  doesn't trip the guard
- [ ] T025 [US2] Grep `docs/**` (excluding the per-feature
  `docs/traceability/NNN-*.md` files) and the root docs
  (README/ROADMAP/CLAUDE/AGENTS/optimize-hot-paths) for the five literal
  patterns; confirm zero remaining matches outside the T024 migration note
  (SC-002). Do **not** include `specs/001-023/**` or the per-feature
  traceability docs in this "must be zero" grep — they are expected to still
  contain the old name (T023) (depends on T013-T024)

**Checkpoint**: User Story 2 is independently complete — every living document
and historical literal reference is consistent, and the breaking change is
documented.

---

## Phase 5: User Story 3 - CI and GitHub-facing metadata reflect the new name (Priority: P2)

**Goal**: CI workflows and dev-environment metadata reference the renamed
options/repo and continue to pass; the .NET interop harness still builds under
its renamed namespace.

**Independent Test**: Run (or inspect) the CI workflow set and confirm every job
references `MUC_OPCUA_*` and passes; confirm the interop harness still builds
(SC-003).

### Implementation for User Story 3

- [ ] T026 [P] [US3] Update `.github/workflows/ci.yml`: every `-DMICRO_OPCUA_*`
  flag across every job (host-build, sanitizer-build, pico-cross-compile,
  static-analysis/fuzz) -> `-DMUC_OPCUA_*`
- [ ] T027 [P] [US3] Update `.devcontainer/devcontainer.json`: the `"name"`
  field and the baked `updateContentCommand`'s `-DMICRO_OPCUA_*` flags
- [ ] T028 [P] [US3] Update `tests/interop/dotnet/interop.csproj`
  (`<RootNamespace>MicroOpcUa.Interop</RootNamespace>`) and `Program.cs`
  (`namespace MicroOpcUa.Interop`, `ApplicationName`, `ApplicationUri`,
  certificate `SubjectName` string literals), plus any literal references in
  `tests/interop/run_interop.sh`, `run_interop_dotnet.sh`, and
  `interop_smoke.py`
- [ ] T029 [US3] Run `dotnet build` in `tests/interop/dotnet/` to confirm it
  compiles cleanly under the renamed `MucOpcUa.Interop` namespace (depends on
  T028)
- [ ] T030 [US3] Confirm the CI jobs pass locally with the renamed options per
  `quickstart.md`'s "Confirm CI still passes" commands (host build, sanitizer
  build, and Pico cross-compile at minimum) (depends on T007-T012, T026)

**Checkpoint**: User Story 3 is independently complete — CI and dev metadata are
consistent and green under the new name.

---

## Phase 6: Polish & Cross-Cutting Compliance

**Purpose**: Prove the rename is complete and self-guarding.

- [ ] T031 Re-run the T002 stale-old-name regression guard (over its defined
  scope — everything except `build*/`, `.git/`, `specs/001-023/**`, and the
  per-feature `docs/traceability/NNN-*.md` files); confirm it now **passes**
  (zero matches outside the resolved T024 allow-list) — SC-002, SC-006
  (depends on T011, T023, T025, T030)
- [ ] T032 Re-run the full host `ctest` suite; confirm green — SC-001, SC-003
  (depends on T011)
- [ ] T033 Add `docs/traceability/024-rename-muc-opcua.md` mapping this
  feature's FR-001 through FR-010 to the tasks/files that implement them,
  matching the existing per-feature traceability doc pattern, and recording
  T012's confirmed-zero-size-delta result (SC-005) as this feature's size
  evidence (this is where that result is recorded, not `docs/size/feature-size-ledger.md`
  — see T012). State plainly that this feature changes no OPC UA behavior,
  StatusCode, or conformance claim (OPC-001 through OPC-005), consistent with
  every other traceability doc's normative-scope framing (depends on
  T007-T030)
- [ ] T034 Walk through `quickstart.md` end to end on a genuinely fresh clone as
  a final sanity check; attach the output as evidence in the PR description
  (depends on T031, T032, T033)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies
- **Foundational (Phase 2)**: Depends on Setup completion (T002's guard should
  exist first so its "fails on old tree" proof is captured before anything
  changes); blocks all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational (T003-T006) completion
- **User Story 2 (Phase 4)**: Independent of User Story 1 — different files
  entirely (docs vs. code); can proceed in parallel with Phase 3 once
  Foundational is done
- **User Story 3 (Phase 5)**: T026-T028 are independent of Phases 3-4 (different
  files); T030 depends on Phase 3's build changes (T007-T012) being in place to
  actually verify CI passes
- **Polish (Phase 6)**: Depends on all three user stories being complete

### Parallel Opportunities

- T001 and T002 (Setup) can run in parallel
- T004 and T006 (Foundational) can run in parallel with each other, but T005
  needs T003+T004 done first
- T007, T008, T009, T010 (US1) can all run in parallel — four disjoint file
  trees
- T013, T014, T016, T017, T018, T019, T020, T021, T022 (US2) can all run in
  parallel — disjoint documentation files. T015 is sequenced after T016 (it
  needs the template review done first to truthfully record it in the
  constitution's Sync Impact Report — see Decision 8), so it is no longer
  marked `[P]`
- T026, T027, T028 (US3) can all run in parallel — disjoint files
- Once Phase 2 (Foundational) is done, **Phases 3, 4, and 5 can proceed
  concurrently** (disjoint file sets: code vs. docs vs. CI/interop), converging
  only at Phase 6

### Suggested Parallel Execution Example (post-Foundational)

```text
# Three independent workstreams, launched together:
Workstream A (US1, code):      T007, T008, T009, T010 -> T011 -> T012
Workstream B (US2, docs):       {T013,T014,T016,T017,T018,T019,T020,T021,T022 in parallel} -> T016 done triggers T015 -> T023 -> T024 -> T025
Workstream C (US3, CI/interop): T026, T027, T028 -> T029 ; T030 (needs Workstream A's T007-T012 done)
# Converge:
Phase 6: T031, T032 -> T033 -> T034
```

---

## Implementation Strategy

1. Complete Setup (capture baseline, stand up the regression guard, confirm it
   fails on purpose) and Foundational (header/CMake-module relocation) first —
   these are the only genuinely serial, blocking steps.
2. Deliver User Story 1 (MVP: the library builds and tests green under the new
   name) — this alone is a shippable, independently valuable increment even if
   docs/CI lag briefly behind.
3. Deliver User Story 2 (docs) and User Story 3 (CI/interop) in parallel with
   or immediately after US1 — they touch disjoint files and have no reason to
   serialize against each other.
4. Finish with Phase 6: the regression guard flipping from failing to passing
   is the single strongest signal that the rename is actually complete, not
   just "mostly done."

## Notes

- No task in this feature adds, removes, or alters OPC UA protocol behavior,
  StatusCodes, or conformance claims — every task here is a rename/documentation
  task, matching spec.md's OPC UA Normative Scope section (OPC-001 through
  OPC-005).
- `[P]` tasks = different files, no dependencies.
- `[Story]` label maps task to a user story for traceability.
- Commit after each task or logical group, consistent with this repo's existing
  one-branch-per-feature / squash-merge workflow.
- Avoid same-file conflicts: T007-T010 are disjoint by directory; T013-T022 are
  disjoint by file; do not let two parallel tasks touch the same file.
