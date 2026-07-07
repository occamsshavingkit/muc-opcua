# Tasks: mDNS Discovery (F1)

**Feature**: 048-mdns-discovery
**Input**: Design artifacts from `specs/048-mdns-discovery/`
**Prerequisites**: spec.md, plan.md, research.md, data-model.md, contracts/api.md, quickstart.md

**Tests**: No unit tests — mDNS discovery is verified manually via `avahi-browse`/`dns-sd`.
Regression tests via existing CTest suite must remain green.

## Format: `[ID] [P?] [Story?] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to
- Include exact file paths in descriptions
- Include OPC UA references in protocol task descriptions

## Path Conventions

- **Public API**: `include/muc_opcua/`
- **Server core**: `src/core/server/`
- **Platform adapters**: `src/platform/`
- **Build system**: `src/CMakeLists.txt`

---

## Phase 1: Setup

- [x] T001 Verify baseline: `ctest --test-dir build --output-on-failure` — all existing tests pass before any changes

---

## Phase 2: Foundational (Adapter Interface)

**Goal**: Add `mu_mdns_adapter_t` definition and wire it into server config

**Independent Test**: File compiles when included — no behavioral test (no-op adapter)

- [x] T002 [US1] Add `mu_mdns_adapter_t` typedef to `include/muc_opcua/platform.h`: struct with `void *context`, `void (*publish)(void *, const char *, uint16_t, const char *, const char *)`, `void (*unpublish)(void *)` (FR-001, OPC-10000-12 §6.3)
- [x] T003 [US1] Add `mu_mdns_adapter_t *mdns_adapter` field to `mu_server_config_t` in `include/muc_opcua/server.h`, before the `#ifdef MUC_OPCUA_PUBSUB` block (FR-002, NULL = disabled)

---

## Phase 3: User Story 1 — Implement mDNS Discovery

**Goal**: No-op adapter, server wiring, host POSIX adapter

**Independent Test**: `ctest --test-dir build --output-on-failure` — zero regressions

### No-op Adapter

- [x] T004 [P] [US1] Create `src/platform/mdns_noop.c` — define `mu_mdns_adapter_t mu_mdns_adapter_noop = {NULL, NULL, NULL}` — static global with NULL callbacks (FR-005)

### Server Wiring

- [x] T005 [P] [US1] Wire `publish` call in `src/core/server/init.c`: after `server->is_running = true`, extract hostname and port from `config->endpoint_url`, then if `config->mdns_adapter` and `config->mdns_adapter->publish` are non-NULL, call `config->mdns_adapter->publish(context, hostname, port, config->application_uri, "/discovery")`. Publish failure must not abort init (FR-003, FR-007)
- [x] T006 [P] [US1] Wire `unpublish` call in `src/core/server/init.c` (`mu_server_close`): before TCP shutdown, if `config->mdns_adapter` and `config->mdns_adapter->unpublish` are non-NULL, call `config->mdns_adapter->unpublish(context)` (FR-004)

### Host POSIX Adapter

- [x] T007 [US1] Create `src/platform/posix_mdns.h` with `mu_host_mdns_adapter_init` declaration (FR-006)
- [x] T008 [US1] Create `src/platform/posix_mdns.c` — implement `mu_host_mdns_adapter_init` that creates a UDP socket, joins multicast group `224.0.0.251:5353`, and stores the socket in `adapter->context`. Implement `publish` callback that constructs and sends a single DNS-SD UDP packet containing PTR, SRV, TXT, and A records per RFC 6763 (FR-006). Implement `unpublish` callback that sends a goodbye packet (TTL=0) per RFC 6762 §10.1 (FR-007). Verify goodbye-packet wire format: TTL must be 0. Only compiled on POSIX platforms (Linux, macOS)
- [x] T009 [P] [US1] Add `src/platform/mdns_noop.c` and `src/platform/posix_mdns.c` to `src/CMakeLists.txt`: unconditionally compile `mdns_noop.c`; compile `posix_mdns.c` when host platform
- [x] T009a [P] [US1] Define `target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_MDNS_DISCOVERY=1)` in `src/CMakeLists.txt`, gated on `if(MUC_OPCUA_IS_HOST)`

---

## Phase 4: Polish & Validation

- [x] T010 Run `ctest --test-dir build --output-on-failure` — all tests pass, zero regressions (SC-004)
- [x] T011 Run `clang-format -i` on all modified files in `src/`, `include/`

---

## Task Summary

| Phase | Story | Count | Description |
|-------|-------|-------|-------------|
| Setup | — | 1 | Baseline verification |
| Foundational | US1 | 2 | Adapter interface + config field |
| US1 | P1 | 7 | No-op adapter, server wiring, host POSIX adapter, gate macro, CMake |
| Polish | — | 2 | ctest regression, clang-format |
| **Total** | | **12** | |

## Dependencies

- T001 (baseline) must pass before any other tasks
- T002 (adapter typedef) must complete before T004 (no-op), T005 (init wiring), T007/T008 (host adapter)
- T003 (config field) must complete before T005, T006
- T004, T005, T006, T007, T009, T009a can run in parallel after T002, T003
- T008 depends on T007 (header)
- T009a must complete before the build verification step in T010
- T010 depends on all prior tasks
- T011 can run at any time after first source file change

## Suggested Execution Order

```
T001 → T002,T003 (foundational)
  ├── T004 (no-op)
  ├── T005 (init wiring)
  ├── T006 (close wiring)
  ├── T007 → T008 (posix header + impl)
  ├── T009a (gate macro)
  └── T009 (CMake, after T009a)
T010,T011 (polish)
```
