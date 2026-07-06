# Data Model: Implement Placeholder Stub Tests

**Feature**: 043-implement-stub-tests | **Date**: 2026-07-06

No new data model. This feature is test-only — no production code, no new
types, no new state machines. All data entities used by the tests are defined
by the existing OPC UA implementation being tested.

## Entities Exercised by Tests

### aggregate_full tests
- `mu_aggregate_state_t` (from `src/services/subscription.h:104-118`)
- `mu_monitored_item_t` (from `src/services/subscription.h`)
- `mu_variant_t` / `mu_datavalue_t` (from `include/muc_opcua/types.h`)

### audit_events tests
- `mu_audit_event_t` (from `include/muc_opcua/services/audit.h:23-47`)
- `mu_server_t` / `mu_server_config_t` (from `include/muc_opcua/server.h`)

### complex_types tests
- `mu_structure_field_t` / `mu_structure_definition_t` (from
  `include/muc_opcua/address_space/complex_types.h`)
- `mu_enum_field_t` / `mu_enum_definition_t` (same header)

### integration tests
- `mu_server_t` / `mu_server_config_t` (server lifecycle)
- `mu_tcp_adapter_t` (mock transport)
- Protocol wire format: HEL/ACK, OPN, MSG, CLO chunks (OPC-10000-6 §7.1)
