# Research: Advanced Alarms & Conditions

## AcknowledgeableConditionType and AlarmConditionType
- **Decision**: Statically pre-allocate tracking structs inside `mu_server_t` for a configured maximum number of active conditions (`MU_MAX_CONDITIONS`, default 10).
- **Rationale**: Meets zero-heap constraints while supporting a realistic number of active alarms for embedded systems.
- **Alternatives considered**: Caller-managed linked lists (rejected due to complexity for users).

## DialogConditionType
- **Decision**: Track dialog state using the same condition tracking framework. Respond methods will trigger state changes via dedicated service handler callbacks.
- **Rationale**: Consolidates state tracking logic.
- **Alternatives considered**: Distinct tracking arrays (rejected as redundant).

## Method Call Routing
- **Decision**: Implement a centralized `mu_alarms_conditions_method_dispatch` function that intercepts Method Calls specifically targeting Acknowledge or Respond node IDs.
- **Rationale**: Decouples Alarms & Conditions from core generic Method call processing, allowing easy compilation toggling (`MICRO_OPCUA_SERVICE_ALARMS_CONDITIONS`).
- **Alternatives considered**: Expanding `method_call.c` (rejected as it clutters the core file with facet-specific logic).
