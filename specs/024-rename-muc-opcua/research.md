# Phase 0 Research: Rename micro-opcua to muc-opcua

## Decision 1 — Exact literal-string substitution set (resolves the "Micro profile" collision risk)

**Decision**: The rename is implemented as five exact, case-sensitive literal-string
substitutions, applied uniformly across the whole tracked tree (no directory is
skipped from the *mechanical* substitution — see Decision 4 for how this coexists
with "don't rewrite historical narrative"):

| Old literal | New literal | Where it appears |
|---|---|---|
| `MICRO_OPCUA` | `MUC_OPCUA` | Preprocessor macros, CMake options/cache vars, header guards (`MICRO_OPCUA_ENCODING_H` -> `MUC_OPCUA_ENCODING_H`) |
| `micro_opcua` | `muc_opcua` | Directory `include/micro_opcua/`, `#include "micro_opcua/...".h`, CMake target/library name (`add_library(micro_opcua ...)`), `project(micro_opcua ...)`, archive filename `libmicro_opcua.a` |
| `micro-opcua` | `muc-opcua` | GitHub URL, README/doc prose, kebab-case mentions |
| `MicroOpcUa` | `MucOpcUa` | CMake include-module filenames (`cmake/MicroOpcUaOptions.cmake` etc.), the .NET interop harness's C# namespace (`MicroOpcUa.Interop`) |
| `Micro-OPCUA` | `Muc-OPCUA` | One Markdown title (`ROADMAP.md` first line: `# Micro-OPCUA Roadmap`) |
| `Micro OPC UA` | `muc-opcua` | Human-readable project display name with a literal space between "OPC" and "UA" (console banners, `application_name` config strings, doc prose) — see Addendum below |

**Rationale**: A grep across the full tree (`grep -rEIo '[Mm][Ii][Cc][Rr][Oo][-_ ]?[Oo][Pp][Cc][Uu][Aa]'`,
excluding `build*/`) found exactly these five case/separator variants and no others —
1441 `MICRO_OPCUA`, 955 `micro_opcua`, 284 `micro-opcua`, 44 `MicroOpcUa`, 1
`Micro-OPCUA`. Every one of these compound strings always has `opcua`/`OpcUa`/`OPCUA`
directly attached to `micro`/`Micro`/`MICRO`, so a literal-string substitution (not a
word-boundary regex on bare "micro") cannot touch the OPC Foundation's own **Micro**
server-profile vocabulary, which never appears in one of these five compound forms.

**Addendum (found during T030 local CI replication)**: the original grep's `[Oo][Pp][Cc][Uu][Aa]`
segment requires "OPCUA" contiguous, so it has a blind spot for the project referring
to itself as "Micro OPC UA" — two words, with a space between "OPC" and "UA" — in
`README.md`, `docs/api-reference.md`, `docs/getting-started.md`,
`examples/minimal_server/{main.c,README.md}`, `platform/pico/pico_minimal_server.c`,
`src/encoding/binary_nodeid.c`, and `cmake/MucOpcUaOptions.cmake`'s `message(STATUS ...)`.
This is unambiguously the project's own display name (console banners, an
`application_name` config string, a code comment) rather than the OPC Foundation's
profile vocabulary — no instance reads "Micro OPC UA Embedded Device..." or similar
profile phrasing — so it was renamed to `muc-opcua` for consistency with every other
prose mention. `tests/unit/test_no_stale_project_name.c`'s `forbidden_literals[]` was
extended with this sixth pattern to guard against regression. The one frozen
occurrence (`specs/017-historical-access/research.md`) is already out of scope via the
existing `specs/` directory exclusion.

**Alternatives considered**:
- *Regex `\bmicro\b` word-boundary rename, then manually fix profile-name
  collisions* — rejected: this is strictly riskier (would touch "Micro Embedded
  Device 2017 Server Profile", `MicroEmbeddedDevice2017` profile URIs, `make micro` /
  `MICRO_OPCUA_PROFILE=micro` / `$(BUILD)/micro` build-profile plumbing, and any future
  prose mentioning the Micro profile) for no benefit over the exact-literal approach.
- *Case-insensitive blanket `micro` -> `muc` substring rename* — rejected outright:
  would corrupt `MICRO_OPCUA_PROFILE=micro` (a legitimate OPC UA profile-tier value,
  confirmed still meaning "OPC UA Micro profile" after this rename — see Decision 2),
  `make micro` / `$(BUILD)/micro` in `Makefile`, and prose like "Micro Embedded Device
  2017 Server Profile" throughout `docs/conformance/*` and `ROADMAP.md`.

## Decision 2 — The `micro`/`nano`/`embedded`/`full` profile-tier vocabulary is explicitly OUT of scope

**Decision**: `MICRO_OPCUA_PROFILE` (the CMake cache variable name) is renamed to
`MUC_OPCUA_PROFILE` per Decision 1, but its **values** — `nano`, `micro`, `embedded`,
`full`, `custom` — are NOT renamed. `make micro`, `$(BUILD)/micro`, and every
"Micro profile"/"Micro Embedded Device 2017 Server Profile" prose reference in
`docs/conformance/*`, `ROADMAP.md`, ADRs, etc. stay exactly as they are.

**Rationale**: Confirmed by inspecting `CMakeLists.txt` (`set(MICRO_OPCUA_PROFILE
"nano" CACHE STRING ...)`) and `Makefile` (`make micro:` target building
`-DMICRO_OPCUA_PROFILE=micro`) — none of these bare `micro` occurrences match any of
the five Decision-1 literal patterns, so they are untouched by construction, not by a
manual carve-out. This is exactly the collision the requester flagged as a reason for
the rename in the first place (avoid confusion between the *project* name and the OPC
UA *Micro* profile), and the mechanical approach naturally keeps them distinct going
forward: `MUC_OPCUA_PROFILE=micro` unambiguously reads as "the muc-opcua build,
targeting the OPC UA Micro profile."

## Decision 3 — `mu_`/`opcua_` C symbol prefixes stay unchanged

**Decision**: Confirmed as **no change**. `mu_server_init`, `mu_nodeid_t`,
`opcua_statuscode_t`, and every other `mu_`/`opcua_`-prefixed public/internal
identifier keep their current spelling.

**Rationale**: `mu_` already reads as the start of "muc" and never collided with
anything; per FR-010 in the spec this is a planning-time decision, and no technical
finding in the codebase (config macros, header guards, or naming collisions) argues
for changing it. Changing ~1000s of call sites across every `.c`/`.h` file for a
purely cosmetic gain is not justified against Size Discipline / Spec Fidelity
principles that ask for the smallest change that achieves the stated goal (a project
rename to fix external branding/naming confusion, not an API redesign). This keeps
the diff scoped to build-system identifiers, include paths, and prose — the things
that actually caused the confusion the requester described.

## Decision 4 — `specs/001-023` and their per-feature traceability docs are frozen history: left completely untouched

**Decision (final, per explicit user direction)**: `specs/001-*` through
`specs/023-*`, and the per-feature `docs/traceability/NNN-*.md` files that
document those same already-shipped features, are **not edited at all** by this
rename — not their narrative, and not their literal macro-name/include-path/
repository-URL references either. They are treated exactly like already-merged
git commits or PR descriptions: a project rename doesn't go back and rewrite
history, even the parts of history that happen to quote an old build flag.

**Rationale**: The user explicitly overrode an earlier, more aggressive
resolution of this exact question (see "History of this decision" below) with a
direct instruction: don't fix already-completed spec-kit workflow artifacts,
the same way a rename doesn't rename old commits. This is a clean, defensible
line — `specs/NNN` and their matching `docs/traceability/NNN-*.md` are both
literally the recorded output of a finished, merged spec-kit feature; nothing
about them is "current documentation a new reader consults to use the library
today" (that's README/docs/conformance/*.md/etc., which stay living). Leaving
them untouched also means the regression guard (Decision 6) has one clean,
narrow, permanent exclusion instead of a fragile per-string allow-list that has
to keep growing.

**History of this decision** (kept for traceability, since this flipped twice):
1. Original spec draft: apply the literal substitution inside `specs/001-023`
   too, reasoning that a reader might copy-paste a stale `-DMICRO_OPCUA_...`
   command from a historical quickstart and have it silently fail.
2. `/speckit-analyze` (finding C1) caught that this original position was
   internally inconsistent between spec.md's SC-002 (which excluded
   specs/001-023 from the "zero matches" bar) and FR-006/this decision (which
   didn't) — and fixed it by making both sides consistently say "include."
3. The user then overrode both prior positions with the git-history analogy:
   exclude `specs/001-023` and the per-feature traceability docs entirely,
   full stop. This is the version that ships. The "stale command" risk from
   position 1 is accepted as a known, bounded, and reasonable cost — exactly
   the same cost every renamed project accepts when old historical docs/READMEs
   quote an old package name.

**Alternatives considered**: *Apply the substitution inside specs/001-023
because a reader might copy a stale command* (position 1 above) — superseded by
explicit user direction; the risk is real but small (these are dated,
clearly-historical documents, not the current getting-started path) and
accepting it is more consistent with how version control and changelogs
normally treat history than "silently editing old records" would be.

## Decision 5 — Full inventory of non-macro/non-include-path touch points

Confirmed by direct inspection, beyond the blanket literal substitution:

- **CMake module files** (repo-relative paths, not just content): `cmake/MicroOpcUaOptions.cmake`,
  `cmake/MicroOpcUaCodegen.cmake`, `cmake/MicroOpcUaStaticAnalysis.cmake`,
  `cmake/MicroOpcUaWarnings.cmake` — filenames renamed to `MucOpcUa*.cmake` (Decision 1,
  `MicroOpcUa` -> `MucOpcUa` pattern), plus their four `include(cmake/MicroOpcUa*.cmake)`
  call sites in the top-level `CMakeLists.txt`.
- **Public header tree**: `include/micro_opcua/` (14 headers incl. umbrella
  `micro_opcua.h` and the `services/` subdirectory) -> `include/muc_opcua/` with
  umbrella `muc_opcua.h`; every `#include "micro_opcua/...".h` across `src/` (all
  layers), `tests/` (unit/integration/fuzz/benchmark/support), `platform/`
  (pico, arduino), and `examples/` (minimal_server, pubsub_server) updated in the
  same pass.
- **CMake project identity**: `project(micro_opcua VERSION 0.1.0 LANGUAGES C)` in
  `CMakeLists.txt` and `add_library(micro_opcua STATIC ...)` +
  `target_include_directories(micro_opcua PUBLIC ...)` in `src/CMakeLists.txt`.
- **.NET interop harness** (`tests/interop/dotnet/`): `interop.csproj`
  `<RootNamespace>MicroOpcUa.Interop</RootNamespace>`, `Program.cs` namespace
  `MicroOpcUa.Interop` plus prose/URI strings (`ApplicationUri = "urn:micro-opcua:interop:client"`,
  `SubjectName = "CN=micro-opcua-interop-client"`) — all covered by Decision 1's
  patterns.
- **Dev/CI metadata**: `.github/workflows/ci.yml` (every `-DMICRO_OPCUA_*` CMake
  invocation across 6+ jobs), `.devcontainer/devcontainer.json` (`"name":
  "micro-opcua"` and the baked `updateContentCommand` using `-DMICRO_OPCUA_*`).
- **No badges found**: `README.md` was checked for `shields.io`/CI-badge markup
  referencing the old repo URL and none exists today, so there is no badge-URL
  fix-up needed beyond the prose/status-line text already covered by Decision 1.
- **Living process docs**: `.specify/memory/constitution.md` (title: "# micro-opcua
  Constitution"), `.specify/templates/plan-template.md`,
  `.specify/templates/tasks-template.md`, `AGENTS.md`, `CLAUDE.md`, `ROADMAP.md`,
  `README.md` — all in scope as living documentation per FR-004; the constitution
  amendment itself needs a PATCH-level version bump per its own Governance section
  (wording/non-semantic correction, not a principle change) since only its title
  string changes, not any principle.
- **Root-level loose file** `optimize-hot-paths.md` (referenced by spec
  022) also contains the old literal strings and is in scope as living
  documentation, not a historical `specs/` artifact.
- **`docs/traceability/` splits into living and historical halves**: the index
  files `sections-to-files.md`, `files-to-sections.md`, and
  `conformance-claims.md` describe *current* cross-references and are living
  documentation (in scope, updated). The per-feature files
  (`004-optimization-fixes.md`, `005-embedded-profile-completion.md`,
  `007-optional-write-service.md`, `008-user-identity-auth.md`,
  `012-opcua-pubsub.md`, `013-advanced-security.md`,
  `019-fix-conformance-size.md`, `022-optimize-hot-paths.md`,
  `023-conformance-docs-subscriber.md`) are each the traceability record of one
  already-shipped `specs/NNN` feature and are frozen history per Decision 4 —
  out of scope, untouched.

## Decision 6 — Regression guard implementation approach (FR-008)

**Decision**: Extend the existing stale-claim test pattern
(`tests/unit/test_conformance_docs.c` / `test_traceability_docs.c`, which already
walk documentation files at test time and fail on forbidden substrings) with a new
check — or a new small test file following the same pattern — that walks the
tracked file set **excluding** `build*/`, `.git/`, `specs/001-023/**`, and the
per-feature `docs/traceability/NNN-*.md` files (Decision 4 — those are frozen
history and are expected to retain the old name forever, by design), and fails
if it finds `micro-opcua`, `micro_opcua`, `MICRO_OPCUA`, or `MicroOpcUa` as a
literal substring outside of that exclusion plus one explicit allow-list entry
(the migration note itself).

**Revision history (post-analysis, then post-user-direction)**: This decision
flipped twice — see Decision 4's "History of this decision" for the full
sequence. The version shipped here (exclude `specs/001-023` and the per-feature
traceability docs) is the final one, per explicit user direction overriding both
the original draft and the `/speckit-analyze`-driven "include everything"
correction that preceded it. `contracts/regression-guard-contract.md` is updated
to match this final scope.

**Rationale**: This mirrors an established, already-reviewed pattern in this
codebase (Constitution Principle IV/VI: correctness/tooling gates enforced by CI) and
directly implements FR-008/SC-006 without inventing new project infrastructure (no
new CI tool, no new dependency — plain C string scanning like the existing docs
tests, or a `tests/tools/`-style Python script mirroring `check_build_matrix.sh`'s
existing shell-based repo-wide grep style if a build-time C test proves awkward for
scanning non-source files like `.github/workflows/ci.yml`). Exact mechanism (C unit
test vs. shell script invoked from CI) is a task-level implementation detail, not a
design decision that needs to block planning.

**Alternatives considered**: *No regression guard, rely on code review* — rejected
per FR-008, which the spec's Edge Cases section explicitly calls out as a
requirement, precisely because a 280-file mechanical rename is exactly the kind of
change a future contributor could partially and silently regress (e.g. copy-pasting
an old snippet from a search engine cache or an old fork).

## Decision 7 — No compatibility shim

**Decision**: No forwarding header (`include/micro_opcua/micro_opcua.h` that
`#include`s the new `muc_opcua.h`) and no `MICRO_OPCUA_*` -> `MUC_OPCUA_*` macro
aliasing is added.

**Rationale**: Per the spec's Assumptions, no published stable release exists, so
there are no known external integrators depending on the old names to protect; the
requester explicitly said no shim is required unless "essentially free," and even a
single forwarding header has a real cost here — it would need to keep working through
every future `MICRO_OPCUA_*` macro addition/removal, which reintroduces exactly the
kind of dual-naming confusion this rename exists to eliminate. The FR-007 migration
note communicates the breaking change instead.

## Decision 8 — The constitution title fix is a PATCH-level correction, not a full Amendment

**Decision**: Editing `.specify/memory/constitution.md`'s title (`# micro-opcua
Constitution` -> `# muc-opcua Constitution`) is a **PATCH** version bump
(1.0.0 -> 1.0.1) under the constitution's own Versioning policy ("PATCH:
clarifications, wording fixes, or non-semantic corrections"), not a full
principle-level Amendment. It nonetheless satisfies every element the
Governance section's amendment ceremony asks for, so there is no gap even
under the stricter reading:

- *Written rationale*: this rename's spec/plan/research constitute the
  rationale, and the Sync Impact Report comment at the top of the constitution
  file records a one-line pointer to it.
- *Review of affected Spec Kit templates and runtime guidance*: covered by the
  same-feature task that updates `.specify/templates/plan-template.md` and
  `tasks-template.md`.
- *Migration note for active specs or plans*: this feature's own FR-007
  migration note already documents the rename project-wide, including the
  constitution's title; no second, separate migration note is needed.
- *Updated semantic version and amendment date*: the PATCH bump itself, plus
  updating "Last Amended" to this feature's date.

**Rationale**: This finding came out of `/speckit-analyze` (A2): the
constitution's Governance section doesn't explicitly distinguish "this MUST
document is titled after the project, so a project rename touches it" from "a
principle changed." Its own Versioning policy already draws exactly that line
(PATCH vs. MINOR/MAJOR), so applying it here is the constitution being
consistent with itself, not this feature inventing an exception. Since the
constitution is treated as non-negotiable, this decision is recorded explicitly
rather than left for the task executing it to improvise.

**Alternatives considered**: *Treat it as a full amendment requiring a
standalone rationale document and a MINOR bump* — rejected: no principle,
scope constraint, or workflow gate changes; the Versioning policy's own PATCH
definition covers this exactly, and manufacturing a MINOR-level ceremony for a
title string would be inconsistent with the constitution's stated intent for
that version tier ("added principles, new mandatory quality gates...").

## Summary

No `NEEDS CLARIFICATION` markers remain from the spec. All Technical Context
unknowns are resolved by the decisions above, including two decisions that were
each revised once during review: Decision 4/6's final scope for
`specs/001-023`/`docs/traceability/NNN-*.md` (frozen history, excluded
entirely — the user's explicit final call, after an intermediate
`/speckit-analyze`-driven "include everything" correction that this supersedes),
and Decision 8 on the constitution's version-bump tier. Phase 1 proceeds
directly to design artifacts.
