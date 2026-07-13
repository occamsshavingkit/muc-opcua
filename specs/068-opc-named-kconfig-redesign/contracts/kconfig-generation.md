# Contracts: OPC-Named Kconfig Redesign

## Kconfig Generation Contract

### Output file: `/Kconfig`

**Profile choice section**:
```kconfig
mainmenu "muc-opcua OPC UA Server Configuration"

choice
    prompt "OPC UA Server Profile"
    default MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2017
    help
      Select an OPC UA Server Profile.  Choosing a named profile sets all
      Facets and Conformance Units to the OPC-defined defaults for that
      profile.  Changing any Facet or CU from the profile defaults
      switches to the Custom profile.

config MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2017
    bool "Nano Embedded Device 2017 UA Server Profile"
    help
      OPC UA Server Profile URI:
      http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2017

config MUC_OPCUA_PROFILE_MICRO_EMBEDDED_DEVICE_2017
    bool "Micro Embedded Device 2017 UA Server Profile"
    ...

config MUC_OPCUA_PROFILE_EMBEDDED_2017_UA_SERVER
    bool "Embedded 2017 UA Server Profile"
    ...

config MUC_OPCUA_PROFILE_STANDARD_2017_UA_SERVER
    bool "Standard 2017 UA Server Profile"
    ...

config MUC_OPCUA_PROFILE_FULL
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
    default y if MUC_OPCUA_PROFILE_MICRO_EMBEDDED_DEVICE_2017 || ...
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
MUC_OPCUA_PROFILE_STANDARD_2017_UA_SERVER=y
```

Each defconfig selects exactly one profile symbol.

### Output file: `muc_opcua_config.cmake` (via `scripts/kconfig/gen_config.py`)

```cmake
set(MUC_OPCUA_PROFILE_STANDARD_2017_UA_SERVER y)
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
set(MUC_OPCUA_KCONFIG_FEATURES
    MUC_OPCUA_FACET_CORE_2017_SERVER
    MUC_OPCUA_CU_ATTRIBUTE_READ
    MUC_OPCUA_CU_VIEW_BASIC
    MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF
    ...
    MUC_OPCUA_MARKER_STANDARD_PROFILE
)
```

The old `SERVICE_READ`, `SECURITY`, `SUBSCRIPTIONS`, etc. symbols are removed from this list. The new OPC-name-derived symbols replace them.

### `src/CMakeLists.txt`

C source files use `#ifdef MUC_OPCUA_<NEW_SYM>` guards. All `#ifdef` references to old symbols must be updated to their new OPC-name-derived equivalents.

Mapping (old → new) for symbols that currently have build-time feature gates in C code:
- The mapping is computed by the generator from the manifest and emitted as part of the generation step.
