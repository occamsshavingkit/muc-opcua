# Feature Specification: Split Oversized Source Files

**Feature ID**: 041-split-oversized-files
**Status**: Draft
**Created**: 2026-07-05
**Source**: File-size audit — 11 logic files exceed 500 lines

## Summary

Split 11 source files exceeding 500 lines into sub-directories containing
per-concern `.c` files. Each original file becomes a directory with 2-5
smaller files. Build system (`CMakeLists.txt`) updated to include the new
file locations. Zero behavioral changes — existing tests are the regression
gate. No header or API changes.

## Files to Split

| File | Lines | Directory | Sub-files |
|------|-------|-----------|-----------|
| `src/core/service_dispatch.c` | 2,400 | `src/core/service_dispatch/` | session_handler.c, attribute_handler.c, view_handler.c, subscription_handler.c, transfer_handler.c, common.c |
| `src/services/subscription_publish.c` | 1,542 | `src/services/subscription_publish/` | publish.c, notification.c, deadband.c, retransmit.c |
| `src/core/server.c` | 1,143 | `src/core/server/` | init.c, poll.c, chunk_io.c, dispatch.c |
| `src/core/dispatch_session.c` | 960 | `src/core/dispatch_session/` | create.c, activate.c, close.c, common.c |
| `src/services/node_management.c` | 753 | `src/services/node_management/` | add_nodes.c, add_references.c, delete.c, common.c |
| `src/platform/host_crypto_adapter.c` | 593 | `src/platform/host_crypto/` | cipher.c, asymmetric.c, hash.c, sign.c |
| `src/security/asym_chunk.c` | 550 | `src/security/asym_chunk/` | wrap.c, unwrap.c, common.c |
| `src/services/read.c` | 526 | `src/services/read/` | read_attribute.c, read_process.c, cache.c |
| `src/platform/wolfssl_crypto_adapter.c` | 526 | `src/platform/wolfssl_crypto/` | cipher.c, asymmetric.c, hash.c, sign.c |
| `src/core/dispatch_discovery.c` | 515 | `src/core/dispatch_discovery/` | get_endpoints.c, find_servers.c, register.c |
| `src/services/browse.c` | 500 | `src/services/browse/` | browse.c, browse_next.c, translate_paths.c |

## Motivation

Single-file modules over 500 lines make it hard to:
- Find the handler for a specific OPC UA service
- Track which tests cover which code paths
- Avoid merge conflicts when multiple developers work on different services

Splitting into directories with per-concern files provides natural boundaries.

## User Stories

### US1: Split Core Dispatch Files (Priority: P1)

**As a** developer debugging service dispatch, **I want**
`service_dispatch.c`, `dispatch_session.c`, `dispatch_discovery.c`, and
`server.c` split into per-handler files under named directories,
**so that** I can open the exact handler file for a given OPC UA service
rather than searching a 2,400-line file.

**Why this priority**: service_dispatch.c is 5× the 500-line budget and
contains every service handler. Breaking it up provides immediate ROI.

**Independent Test**: All existing tests pass, build succeeds on all profiles.

### US2: Split Service Implementation Files (Priority: P2)

**As a** developer extending subscription or read behavior, **I want**
`subscription_publish.c`, `read.c`, `browse.c`, and `node_management.c`
split into per-concern files,
**so that** I can modify publish logic without touching notification
encoding or deadband checking.

**Independent Test**: All existing tests pass.

### US3: Split Security and Platform Adapters (Priority: P3)

**As a** developer porting to a new crypto backend, **I want**
`asym_chunk.c`, `host_crypto_adapter.c`, and `wolfssl_crypto_adapter.c`
split into per-operation files,
**so that** I can compare implementations across backends or replace one
operation without touching unrelated code.

**Independent Test**: All existing tests pass.

---

### Edge Cases

- What if a shared `static` function is used across multiple sub-files?
  Promote to an internal header in the new directory (e.g., `common.h`).
- What if a sub-file ends up under 20 lines? That's fine — small files
  are better than monolithic ones. Minimum viable sub-file is one function.
- What about include paths? Sub-files include from the new directory
  relative to each other: `#include "common.h"` within the directory,
  `#include "../core.h"` for out-of-directory includes.
- What about the CMake glob? Replace `file(GLOB ...)` patterns to capture
  the new sub-directory structure. Verify no files are lost.

## Requirements

### Functional Requirements

- **FR-001 through FR-011**: Each of the 11 files MUST be split into a
  sub-directory with per-concern `.c` files. No sub-file exceeds 500 lines.
- **FR-012**: Shared functions within a split directory MUST be declared in
  an internal `common.h` header within that directory.
- **FR-013**: The original `.c` file MUST be deleted after successful split.
- **FR-014**: CMakeLists.txt MUST be updated to include files from the new
  sub-directories. `file(GLOB ...)` patterns MUST not miss any files.
- **FR-015**: All existing tests MUST pass unchanged on default, micro,
  and standard profiles.
- **FR-016**: No public API changes. Include paths in public headers are
  unchanged (internal include paths only).
- **FR-017**: clang-format MUST pass on all new files.

### OPC UA Normative Scope

- **OPC-001**: No normative change. File organization is an internal
  implementation detail with no wire-visible impact.
- **OPC-002**: Conformance status unchanged: profile-targeting.

### Scope Boundaries

- **In Scope**: 11 files → 11 directories with ~40 sub-files. CMake
  build system updates. Format enforcement.
- **Out of Scope**: Public API changes, behavioral changes, new tests,
  header reorganization, test file splitting.

### Key Entities

- **Directory**: A `src/<area>/<module>/` directory replacing the original
  `src/<area>/<module>.c` file.
- **Sub-file**: A `.c` file in the new directory containing one logical
  concern (≤ 500 lines).
- **Internal header**: A `common.h` or `<subsystem>.h` in the directory
  declaring shared `static` functions made non-static for cross-file use.

## Success Criteria

- **SC-001**: All 11 original `.c` files are deleted.
- **SC-002**: All 11 new directories exist with ≥ 2 sub-files each.
- **SC-003**: No sub-file exceeds 500 lines.
- **SC-004**: All existing tests pass on default, micro, and standard profiles.
- **SC-005**: CMake build succeeds without missing symbols.
- **SC-006**: clang-format passes on all new files.

## Assumptions

- Sub-files within a directory can `#include "common.h"` for shared types
  and function declarations previously in the monolithic file.
- Functions that were `static` in the monolithic file become non-static
  (but internal-linkage) when moved to sub-files — declared in `common.h`
  and not part of the public API.
- CMake uses `file(GLOB ...)` which auto-discovers new files; `GLOB_RECURSE`
  may be needed for nested directories.
- The split is mechanical: copy function bodies to new files, update
  includes, delete the original file. No logic changes.
