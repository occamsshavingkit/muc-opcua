# Tasks: TypeDefinition Cache

**Input**: Design documents from `specs/034-typedef-cache/`
**Tests**: No new tests — existing 108 serve as regression gates.

---

## Phase 1: Implementation

- [x] T001 Add `type_definition` field of type `mu_nodeid_t` to `mu_node_t` struct in `include/muc_opcua/address_space.h`.

- [x] T002 Add `.type_definition = {0}` initializers to every static node definition in `src/address_space/base_nodes.c`. For nodes that have a HasTypeDefinition reference, populate the initializer with the correct NodeId from the existing references array.

- [x] T003 Add `.type_definition = {0}` initializers to every static node definition in `examples/minimal_server/static_address_space.c`.

- [x] T004 Replace the HasTypeDefinition scan loop in `src/services/browse.c:360-368` with a direct read from `target->type_definition`. Remove the scan loop entirely.

- [x] T005 Build and run all 108 tests: `cmake --build build -j$(nproc) && ctest`.

---

## Phase 2: Validation

- [x] T006 Verify ARM sizes unchanged: `BUILD_ROOT=build/size-034 scripts/measure_size.sh all`.

- [x] T007 Verify ASAN/UBSan clean: `cmake -B build-san -DMUC_OPCUA_SANITIZE=ON -DMUC_OPCUA_BUILD_TESTS=ON && cmake --build build-san -j$(nproc) && ctest --test-dir build-san`.
