# Traceability: Feature 024 Rename to muc-opcua

Feature 024 renames the project's build-system and documentation identity to
`muc-opcua` (see `docs/migration-024-muc-opcua-rename.md` for the prior name
and the exact old-identifier-to-new-identifier mapping). This is a pure
identifier/branding change: zero OPC UA protocol behavior, StatusCode,
service, encoding, or conformance claim is altered. Target status remains
**profile-targeting** (no profile-compliant/CTT-verified claim), unchanged
from before this feature.

## FR Evidence Skeleton

| Requirement | Source paths | Evidence paths | Completion marker |
|---|---|---|---|
| FR-001 (old macro prefix -> `MUC_OPCUA_*` options, see migration note) | `CMakeLists.txt`; `cmake/MucOpcUa{Options,Codegen,StaticAnalysis,Warnings}.cmake` | `tests/unit/test_no_stale_project_name.c`; local `cmake`+`ctest` replication | T004, T005 |
| FR-002 (include/muc_opcua/ header tree) | `include/muc_opcua/**` (14 files, `git mv`'d); every `#include` site in `src/`, `tests/`, `platform/`, `examples/` | `tests/unit/test_no_stale_project_name.c`; host build succeeds | T003, T007-T010 |
| FR-003 (CMake project/target/package identity) | `CMakeLists.txt` (`project(muc_opcua ...)`); `src/CMakeLists.txt` (`add_library(muc_opcua ...)`) | Host configure+build produces `libmuc_opcua.a` | T005, T006 |
| FR-004 (living documentation renamed) | `README.md`, `CLAUDE.md`, `AGENTS.md`, `ROADMAP.md`, `docs/**` except per-feature traceability docs | `tests/unit/test_no_stale_project_name.c`; T025 grep | T013-T022, T024-T025 |
| FR-005 (CI workflow + jobs still pass) | `.github/workflows/ci.yml` | Local replication: host build, sanitizer build, freestanding ARM compile all pass; `.NET` interop project builds | T026, T030 |
| FR-006 (specs/001-023 and per-feature traceability docs untouched) | `specs/001-*` through `specs/023-*`; `docs/traceability/NNN-*.md` (9 files) | `git diff --stat <pre-024-ref> -- specs/ docs/traceability/` shows only `specs/024-rename-muc-opcua/**` and the 3 living traceability index files changed | T023 |
| FR-007 (migration note, no shim) | `docs/migration-024-muc-opcua-rename.md` | Note states exactly what changed, what didn't, and the OPC-005 guardrail | T024 |
| FR-008 (regression guard) | `tests/unit/test_no_stale_project_name.c` | Confirmed failing against the pre-rename tree (T002), confirmed passing post-rename (T031) | T002, T031 |
| FR-009 (GitHub repo rename, already done) | N/A (out-of-band, before this spec) | `git remote -v` already points at `occamsshavingkit/muc-opcua` | Assumption, not a task |
| FR-010 (mu_/opcua_ C prefixes unchanged) | Every public/internal C symbol in `include/muc_opcua/**`, `src/**` | `docs/migration-024-muc-opcua-rename.md` "What did NOT change" section | research.md Decision 3 |

## OPC UA Behavior-Preservation Evidence (OPC-001 through OPC-005)

| Claim | Evidence |
|---|---|
| OPC-001: no wire/StatusCode/NodeId/service/profile-conformance change | Full host `ctest` suite: 98/98 pass, same pass count and same test names as pre-rename (only new addition is the regression guard itself, T002/T031) |
| OPC-002: no service/feature behavior change | `docs/conformance/*.md` conformance-unit rows unchanged in substance (only identifier/path text renamed, T013-T022) |
| OPC-003: unsupported-behavior StatusCode citations unchanged | No `src/services/*.c` or `src/core/service_dispatch.c` logic touched by this feature — only `src/address_space/base_nodes.c` and `examples/minimal_server/static_address_space.c` string *content* (see size evidence below), and `src/encoding/binary_nodeid.c` (a comment only) |
| OPC-004: wire encoding/transport unaffected | No chunk-framing, encoding, or transport-profile-URI code changed |
| OPC-005: conformance status unchanged | `docs/conformance/status.md` still states profile-targeting, non-CTT-verified status after the rename; the constitution PATCH bump (1.0.0 -> 1.0.1) is title-only, not a scope/principle change |

## Size Evidence (SC-005)

Per `scripts/measure_size.sh all` (T001 pre-rename baseline vs. T012
post-rename measurement, both recorded in
`docs/size/feature-size-ledger.md`):

- **nano, micro profiles**: byte-for-byte identical `.text`/`.data`/`.bss`.
- **embedded, full-featured profiles**: **-2 bytes** smaller `.data`/`.text`
  combined, not larger. Root cause: the `ServerArray`/`NamespaceArray` URN
  literal in `src/address_space/base_nodes.c` shortened by 2 characters (22
  bytes -> 20 bytes) as a direct, expected consequence of the shorter new
  project name, not a regression — see `docs/migration-024-muc-opcua-rename.md`
  for the exact before/after URN string. Confirmed the adjacent hardcoded
  `mu_string_t` length prefix was updated to match (`base_nodes.c` line 223:
  `{22, ...}` -> `{20, ...}`; `examples/minimal_server/static_address_space.c`
  lines 31/36: `{40, ...}` -> `{38, ...}` for the analogous minimal-server URN
  literal) — this class of bug (string content renamed by sed, paired integer
  length left stale) would otherwise silently corrupt wire encoding without
  failing any existing test, since no test currently asserts these exact byte
  lengths.

No size regression. SC-005 ("this rename does not increase flash/RAM
footprint") holds with margin (a small decrease, from shorter string
literals).

## Notes on Scope Boundaries Encountered During Implementation

- `mu_`/`opcua_` C identifiers: left unchanged per FR-010/research.md
  Decision 3 (default carried from planning, no reason found during
  implementation to revisit).
- `MUC_OPCUA_PROFILE`'s accepted values (`nano`/`micro`/`embedded`/`full`/
  `custom`), `make micro`, and OPC Foundation "Micro [Embedded Device 2017
  Server] Profile" prose throughout `docs/conformance/*.md`: confirmed
  untouched everywhere (research.md Decision 2).
- A sixth old-name spelling not covered by research.md Decision 1's original
  five literal patterns — used as the project's own display name in console
  banners, one `application_name` config string per example, and one code
  comment — was found during T030's local CI replication and fixed; see the
  Decision 1 Addendum in `research.md` (which records the exact spelling) and
  the regression guard's extended `forbidden_literals[]`.
