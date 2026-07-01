# Contract: Stale-Old-Name Regression Guard

Implements FR-008 / SC-006. Mirrors the existing stale-claim pattern in
`tests/unit/test_conformance_docs.c` and `tests/unit/test_traceability_docs.c`
(both already walk documentation files at test time and fail the suite on a
forbidden substring), extended to a repo-wide sweep.

## Inputs

- The set of tracked files in the repository, excluding:
  - build output directories (`build*/`), .NET build intermediates/output
    (`bin/`, `obj/`), and VCS metadata (`.git/`);
  - `specs/001-023/**` and the per-feature `docs/traceability/NNN-*.md` files —
    these are **frozen historical record** (research.md Decision 4) and are
    expected to retain the old name forever, by design, the same way a project
    rename doesn't rewrite already-merged commits or PR descriptions. This is a
    permanent, two-part directory/glob exclusion, not a growing allow-list.
  - two self-referential test files — this guard's own source (its
    `forbidden_literals[]` array and failure message must contain the old
    name to search for it) and `test_traceability_docs.c` (which quotes
    `include/micro_opcua/...` paths verbatim to match the frozen
    `docs/traceability/023-*.md` content byte-for-byte). Distinct from the
    single-entry migration-note allow-list below — this is a structural
    necessity of self-referential tests, not a growing exception budget.
- The forbidden-literal set from research.md Decision 1: `micro-opcua`,
  `micro_opcua`, `MICRO_OPCUA`, `MicroOpcUa`, `Micro-OPCUA`, and (per the
  Decision 1 Addendum found during T030) `Micro OPC UA` (case-sensitive, exact
  substring match — not a word-boundary regex, consistent with how the rename
  itself was applied).

## Behavior

- Scans every input file for any forbidden literal as an exact substring.
- On any match: reports the file path, line number, and matched literal, and fails
  (non-zero exit for a script; a failing `TEST_ASSERT_*` for a Unity test).
- On zero matches across the whole tree: passes.

## Allow-list resolution (open item carried from data-model.md)

The migration note describing this rename (FR-007) will, almost by necessity,
need to say something like "renamed from micro-opcua" to be legible as a migration
note at all. Task-level implementation MUST resolve this one of two ways, decided
during `/speckit-tasks`/`/speckit-implement`, not left ambiguous in the shipped
guard:

1. **Split-string technique**: the migration note spells the old name in a way
   that is human-legible but not a literal substring match (e.g. joining pieces,
   or using a code span with a zero-width separator) — the guard then truly has a
   zero-entry allow-list, which is simplest to reason about and matches Decision 6's
   "no allow-list" default.
2. **Path-based exclusion**: the guard explicitly excludes exactly one file (the
   migration note) by path. Simpler to write, but is a standing exception that must
   be justified and kept minimal — exactly one entry, never a directory-wide
   exclusion, and never extended to cover future stale references.

Either is acceptable; the guard's own source/config MUST make the chosen mechanism
and its scope obvious to a future reader (a bare hardcoded exclusion list buried in
the test with no comment is not acceptable).

## Verification

- Before this feature's rename is complete: the guard, run against the
  pre-rename tree, fails loudly (proves it actually detects the old name).
- After the rename: the guard passes against the full tree.
- A follow-up PR that reintroduces `micro_opcua` anywhere (e.g. a careless
  copy-paste of an old code sample) MUST fail CI via this guard.
