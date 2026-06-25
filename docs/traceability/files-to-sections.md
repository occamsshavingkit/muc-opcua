# Traceability: Files to OPC UA Sections

This document maps implementation and test files back to OPC UA normative sections.

| File | Entity/Component | OPC UA Part | Section | Description |
|------|------------------|-------------|---------|-------------|
| `include/micro_opcua/config.h` | Limits | Part 6 | 7.1.2.3, 7.1.2.4 | Compile-time bounds for chunks/messages |
| `include/micro_opcua/types.h` | Built-in Types | Part 6 | 5.2.1, 5.2.2.* | Numeric, String, NodeId, Variant types |
| `include/micro_opcua/status.h` | StatusCodes | Part 4 / 6 | 7.38.2 / 7.1.5 | Public StatusCode constants |
| `include/micro_opcua/platform.h` | Platform Adapters | Part 4 / 6 | 5.6.2.2 / 7.2 | Adapter Interfaces |
| `include/micro_opcua/server.h` | Server API | Part 4 / 6 | 5.6.2.2 / 7.1.2.3 | Config & Lifecycle APIs |
| `src/core/status.c` | StatusCodes | Part 4 / 6 | 7.38.2 / 7.1.5 | Status Helper `mu_status_name` |
| `src/core/server.c` | Server Core | Part 4 / 6 | 5.6.2.2 / 7.1.2.3 | Init, validate, and poll implementations |
| `tests/unit/test_status.c` | Tests | Part 4 / 6 | 7.38.2 / 7.1.5 | Test Status helpers |
| `tests/unit/test_server_config.c` | Tests | Part 6 | 7.1.2.3, 7.1.2.4 | Test config validation |
| `tests/unit/test_platform_adapter_contract.c` | Tests | Part 6 | 7.2 | Test adapters structure |
