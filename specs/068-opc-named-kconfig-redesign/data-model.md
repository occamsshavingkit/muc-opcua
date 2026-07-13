# Data Model: OPC-Named Kconfig Redesign

## Manifest Schema Extensions

### Top-level additions

```yaml
facet_containment:           # NEW: Maps facet item IDs to lists of CU item IDs
  <facet_item_id>:
    - <cu_item_id>
    - <cu_item_id>

items:
  - id: "<item_id>"          # Existing, unchanged
    kind: "facet|conformance_unit|optimization"  # Existing
    opc_display_name: "<OPC UA standard name>"   # NEW: For Kconfig prompts
    kconfig_symbol: null     # CHANGED: Optional now. Generator computes if null.
    profile_defaults: {...}  # Existing, unchanged
    implementation_state: "claimed|implemented|unimplemented|deferred"  # Existing
    depends_on: [...]        # Existing
    backing_tests: [...]     # Existing
    opc_reference:           # Existing
      spec: "<OPC-10000-XX>"
      section: "<section>"
      facet: "<facet name>"
      cu_name: "<CU name>"
```

### Item entity

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | string | yes | Internal stable identifier (e.g., `service_read`) |
| `kind` | enum | yes | `facet`, `conformance_unit`, `optimization` |
| `opc_display_name` | string | yes | OPC UA standard name for Kconfig display |
| `kconfig_symbol` | string\|null | no | Computed by generator if null; explicit if override needed |
| `profile_defaults` | object | yes | Per-profile boolean defaults |
| `implementation_state` | enum | yes | One of `claimed`, `implemented`, `unimplemented`, `deferred` |
| `depends_on` | string[] | no | Kconfig symbol dependencies |
| `depends_on_op` | enum | no | `and` (default) or `or` |
| `backing_tests` | string[] | no | Test names for claim verification |
| `opc_reference` | object | no | OPC spec grounding |
| `default_unconditional` | boolean | no | Always default y if true |

### Facet containment entity

| Field | Type | Description |
|-------|------|-------------|
| `facet_containment` | object | Maps facet item `id` → list of CU item `id`s |

Each facet must appear as an item with `kind: "facet"`. Each CU must appear as an item with `kind: "conformance_unit"`. Containment references use item `id` values, not `kconfig_symbol`s.

### Profile entity (unchanged)

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | string | yes | Kconfig choice value (e.g., `PROFILE_STANDARD`) |
| `display` | string | yes | Legacy display string (kept for defconfig compatibility) |
| `opc_profile_uri` | string\|null | no | OPC UA Profile URI |
| `selectable` | boolean | yes | Whether shown in Kconfig choice |

### Capacity entity (unchanged)

No changes to capacity model. Capacities remain in separate menu with existing override chain.

### Advertised profile markers entity

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Marker symbol (e.g., `MARKER_STANDARD_PROFILE`) |
| `default_if` | string[] | Profile choice values that set this marker to y |

## Symbol Naming Convention

### Computed symbol names

For items where `kconfig_symbol` is null, the generator computes:

```python
def compute_symbol(kind: str, opc_display_name: str) -> str:
    prefix = {
        "profile":      "MUC_OPCUA_PROFILE_",
        "facet":        "MUC_OPCUA_FACET_",
        "conformance_unit": "MUC_OPCUA_CU_",
        "optimization": "MUC_OPCUA_OPT_",
    }[kind]
    
    working = opc_display_name
    # Strip trailing kind word if redundant
    kind_suffixes = {"profile": "profile", "facet": "facet"}
    if kind in kind_suffixes:
        suffix = " " + kind_suffixes[kind]
        if working.lower().endswith(suffix):
            working = working[:-len(suffix)]
    
    body = working.upper()
    # Replace non-alphanumeric with underscore
    body = "".join(c if c.isalnum() else "_" for c in body)
    # Collapse multiple underscores
    import re
    body = re.sub(r"_+", "_", body)
    # Strip leading/trailing underscores
    body = body.strip("_")
    
    return prefix + body
```

### Validation rules

- No Facet symbol may end with `_FACET` after the `MUC_OPCUA_FACET_` prefix.
- No Profile symbol may end with `_PROFILE` after the `MUC_OPCUA_PROFILE_` prefix.
- All symbols must be valid Kconfig identifiers (`[A-Za-z0-9_]+`).
- No duplicate symbols across items or capacities.

## State Transitions

### Profile state machine

```
          ┌─────────────┐
          │   Custom    │◄────────┐
          └─────────────┘         │
                ▲                 │ (user changes any Facet/CU
                 │                 │  from profile default)
    ┌───────┐   │   ┌────────┐    │
    │Nano   │◄──┼──►│Standard│────┘
    └───────┘   │   └────────┘
    ┌───────┐   │   ┌────────┐
    │Micro  │◄──┼──►│ Full   │
    └───────┘   │   └────────┘
    ┌──────────┐│
    │Embedded  ││
    └──────────┘│
                │
     (user selects named profile)
```

- Selecting a named profile sets all Facet/CU defaults per manifest.
- Any Facet/CU change from profile defaults → effective profile becomes custom.
- Return all to profile defaults → fidelity check passes for that profile.
