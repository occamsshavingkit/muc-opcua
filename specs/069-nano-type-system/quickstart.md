# Quickstart: Nano Core Type System & Base Info CUs

## Prerequisites

- Build environment: `cmake`, C11 compiler
- OPC UA reference tools available via `opc-ua-reference` MCP for NodeId lookups

## Build and test validation

### 1. Build nano profile (default)

```bash
cmake -S . -B build/nano -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_PROFILE=nano
cmake --build build/nano -j$(nproc)
ctest --test-dir build/nano --output-on-failure
```

### 2. Verify new nodes appear in browse output

```bash
# Build and run the server, then browse the address space
cmake -S . -B build/nano -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_PROFILE=nano \
  -DMUC_OPCUA_BUILD_EXAMPLES=ON
cmake --build build/nano -j$(nproc)
```

### 3. Optional CUs (off by default)

Enable specific optional CUs via Kconfig:

```bash
cmake -S . -B build/nano-full -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_PROFILE=nano \
  -DMUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE=ON \
  -DMUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES=ON
cmake --build build/nano-full -j$(nproc)
```

### 4. CI gate matrix

All profiles must pass:
- `profile-tests (nano)` in CI
- `freestanding-core` in CI
- `pico-cross-compile` in CI
- `static-analysis` (cppcheck)
- `coverage` (no regression)

### 5. Size verification

```bash
bash scripts/measure_size.sh nano
# Expected: nano elf_text <= 30 KB (was ~27 KB, +3 KB for all new nodes max)
```

## Per-CU validation strategy

| CU category | Test approach |
|-------------|---------------|
| ReferenceType nodes | Browse References folder; verify HasSubtype hierarchy |
| DataType nodes | Browse DataTypes folder; verify subtype of Structure |
| VariableType nodes | Browse VariableTypes folder; verify subtype of BaseDataVariableType |
| ObjectType nodes | Browse ObjectTypes folder |
| Server properties | Read the Property from the Server object |
| Folder nodes | Browse children of the new folder object |
| AccessLevelEx flags | Read AccessLevelEx from a Variable; verify bitmask |
| Documentation CUs | Review docs/build-and-gating.md tables |
