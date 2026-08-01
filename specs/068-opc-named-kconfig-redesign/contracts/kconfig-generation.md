# Contracts: OPC-Named Kconfig Redesign

## Kconfig Generation Contract

### Output file: `/Kconfig`

**Profile choice section**:
```kconfig
mainmenu "muc-opcua OPC UA Server Configuration"

choice
    prompt "OPC UA Server Profile"
    default MUC_OPCUA_PROFILE_CUSTOM
    help
      Select an OPC UA Server Profile.  Choosing a named profile sets all
      Facets and Conformance Units to the OPC-defined defaults for that
      profile.  Facet and CU overrides are resolved against the profile
      seed; the selected profile remains stable.  Derived advertisement
      markers (e.g., MUC_OPCUA_MARKER_STANDARD_PROFILE) report whether
      the resolved mandatory CU closure satisfies the profile.
      Select "Custom" to start from core services and hand-pick Facets
      and CUs.

config MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER
    bool "Nano Embedded Device 2025 Server Profile"
    help
      OPC UA Server Profile URI:
      http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2025

config MUC_OPCUA_PROFILE_MICRO_EMBEDDED_DEVICE_2025_SERVER
    bool "Micro Embedded Device 2025 Server Profile"
    ...

config MUC_OPCUA_PROFILE_EMBEDDED_2025_UA_SERVER
    bool "Embedded 2025 UA Server Profile"
    ...

config MUC_OPCUA_PROFILE_STANDARD_2025_UA_SERVER
    bool "Standard 2025 UA Server Profile"
    ...

config MUC_OPCUA_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES
    bool "Full (everything enabled, generous capacities)"
    ...

config MUC_OPCUA_PROFILE_CUSTOM
    bool "Custom (hand-select Facets and CUs)"
    
endchoice
```

**Facet section**:
```kconfig
menu "OPC UA Facets and Conformance Units"

menu "Facet: Core 2017 Server"
config MUC_OPCUA_FACET_CORE_2017_SERVER
    bool "Enable Core 2017 Server Facet"
    default y
    help
      OPC source: OPC-10000-4 §5.

config MUC_OPCUA_CU_ATTRIBUTE_READ
    bool "CU: Attribute Read"
    depends on MUC_OPCUA_FACET_CORE_2017_SERVER
    default y
    help
      Implementation state: claimed.
      OPC source: OPC-10000-4 §5.10.2 -- CU 1673:Attribute Read.

config MUC_OPCUA_CU_ATTRIBUTE_WRITE
    bool "CU: Attribute Write"
    depends on MUC_OPCUA_FACET_CORE_2017_SERVER
    default n
    ...

endmenu

menu "Facet: Subscription Server"
config MUC_OPCUA_FACET_SUBSCRIPTION_SERVER
    bool "Enable Subscription Server Facet"
    default y if MUC_OPCUA_PROFILE_MICRO_EMBEDDED_DEVICE_2025_SERVER || ...
    ...

endmenu

endmenu
```

**Capacities section** (unchanged from current):
```kconfig
menu "Capacities"
# ... existing capacity int symbols
endmenu
```

**Project options section** (NEW):
```kconfig
menu "Project options"
config MUC_OPCUA_OPT_READ_CACHE
    bool "Read value cache optimization"
    default n
    ...
endmenu
```

**Unimplemented items** (unchanged approach):
```kconfig
comment "File Server Facet (NOT IMPLEMENTED) [OPC-10000-20]"
```

### Output file: `configs/<profile>.defconfig`

```text
MUC_OPCUA_PROFILE_STANDARD_2025_UA_SERVER=y
```

Each defconfig selects exactly one profile symbol.

### Output file: `muc_opcua_config.cmake` (via `scripts/kconfig/gen_config.py`)

```cmake
set(MUC_OPCUA_PROFILE_STANDARD_2025_UA_SERVER y)
set(MUC_OPCUA_FACET_CORE_2017_SERVER y)
set(MUC_OPCUA_CU_ATTRIBUTE_READ y)
set(MUC_OPCUA_OPT_READ_CACHE n)
set(MUC_OPCUA_MAX_SESSIONS 50)
```

The `CONFIG_=MUC_OPCUA_` prefix means Kconfig writes full names. `gen_config.py` reads `.config` and emits `set(...)` calls.

## Validation Contract

### `validate.py --all`

Must additionally check:
1. **Naming convention**: For each Facet symbol, `not symbol.endswith("_FACET")` (after prefix). For each Profile symbol, `not symbol.endswith("_PROFILE")`.
2. **Facet containment**: Each CU in `facet_containment[facet_id]` generates a `depends on MUC_OPCUA_FACET_<NAME>` line in Kconfig.
3. **Unimplemented visibility**: Items with `implementation_state` in `(unimplemented, deferred)` produce `comment`, not `config`.
4. **Menu labeling**: Facet menus use the `opc_display_name` for the menu label, not the internal id.
5. **Drift check**: All generated files match regeneration output byte-for-byte.

## CMake Contract

### `CMakeLists.txt`

```cmake
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/Kconfig" _kconfig_symbols
    REGEX "^(menu)?config[ \t]+MUC_OPCUA_(PROFILE|FACET|CU)_[A-Za-z0-9_]+$")

set(MUC_OPCUA_KCONFIG_FEATURES READ_CACHE SECURE_CHANNEL_CRYPTO)
foreach(_line IN LISTS _kconfig_symbols)
    string(REGEX REPLACE "^(menu)?config[ \t]+" "" _symbol "${_line}")
    list(APPEND MUC_OPCUA_KCONFIG_FEATURES "${_symbol}")
endforeach()
list(REMOVE_DUPLICATES MUC_OPCUA_KCONFIG_FEATURES)
```

The canonical override surface is discovered from generated Kconfig rather than duplicated in a partial hand-maintained list. Only selectable Profile, Facet, and CU symbols are discovered; hidden `MUC_OPCUA_MARKER_*` and `MUC_OPCUA_INTERN_*` symbols are derived state and MUST NOT be serialized from CMake cache overrides. The independent `READ_CACHE` and `SECURE_CHANNEL_CRYPTO` project controls remain explicit entries. Capacity symbols remain on their separate typed `MU_MAX_*` path.

### `src/CMakeLists.txt`

C source files use `#ifdef MUC_OPCUA_<NEW_SYM>` guards. All `#ifdef` references to old symbols must be updated to their new OPC-name-derived equivalents.

Mapping (old → new) for symbols that currently have build-time feature gates in C code:
- The mapping is computed by the generator from the manifest and emitted as part of the generation step.
