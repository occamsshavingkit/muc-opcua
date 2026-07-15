<!-- markdownlint-disable MD013 -->

# Interface Contracts

## Manifest Contract

Each in-scope CU claim must satisfy this contract before it is marked implemented:

- `id` remains one of the ten roadmap item 006 CU ids.
- `opc_display_name` remains aligned with OPC Foundation naming.
- `kconfig_symbol` is a dedicated `MUC_OPCUA_CU_*` symbol.
- `implementation_state` is not `implemented` unless `backing_tests` is non-empty.
- `profile_defaults.nano` is changed only when a separate approved spec changes nano profile membership.
- `backing_tests` lists tests that run in the relevant host/profile path.

## Service Behaviour Contract

Each service behaviour proof must specify:

- Request family: Discovery, View, Write, Session, or Diagnostics.
- Request case type: positive, negative, boundary, permission, validation, or state conflict.
- Expected service result StatusCode.
- Expected operation-level StatusCode when the service returns per-operation results.
- Expected response shape for encoded fixtures.
- Normative OPC-10000 section citation.

## Dedicated Behaviour Gate Contract

Every implemented claim must satisfy all of these conditions:

- Its dedicated `MUC_OPCUA_CU_*` symbol reaches the CMake/source or dispatch gate for the claimed behavior.
- Disabling that symbol in `FIX-005` makes only the corresponding behavior unavailable with an explicit OPC UA StatusCode.
- Other enabled CUs sharing a source module or aggregate compatibility alias remain available.
- Disabled or rejected state-changing operations preserve address-space values, Session identity/nonce state, and diagnostics surfaces except for explicitly specified rejection counters.
- Aggregate aliases may compile shared infrastructure but cannot independently establish or retain a CU claim.

## Generated Artifact Contract

After manifest changes, generation must update all affected Kconfig, defconfig,
claim-map, conformance documentation, and build documentation artifacts. The
manifest validator must pass with no generated drift left unstaged.
