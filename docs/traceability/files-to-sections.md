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
| `fuzz_binary_reader.c` | TBD | TBD | TBD | TBD |
| `fuzz_binary_types.c` | TBD | TBD | TBD | TBD |
| `fuzz_message_chunk.c` | TBD | TBD | TBD | TBD |
| `test_binary_string.c` | TBD | TBD | TBD | TBD |
| `test_secure_channel.c` | TBD | TBD | TBD | TBD |
| `test_service_state_errors.c` | TBD | TBD | TBD | TBD |
| `test_binary_array_errors.c` | TBD | TBD | TBD | TBD |
| `test_address_space_callbacks.c` | TBD | TBD | TBD | TBD |
| `test_binary_string_errors.c` | TBD | TBD | TBD | TBD |
| `test_binary_primitives.c` | TBD | TBD | TBD | TBD |
| `test_binary_extension_object.c` | TBD | TBD | TBD | TBD |
| `test_service_dispatch.c` | TBD | TBD | TBD | TBD |
| `test_binary_nodeid.c` | TBD | TBD | TBD | TBD |
| `test_address_space_string_limits.c` | TBD | TBD | TBD | TBD |
| `test_browse_limits.c` | TBD | TBD | TBD | TBD |
| `test_discovery_endpoint.c` | TBD | TBD | TBD | TBD |
| `test_browse_service.c` | TBD | TBD | TBD | TBD |
| `test_message_chunk.c` | TBD | TBD | TBD | TBD |
| `test_binary_variant_datavalue.c` | TBD | TBD | TBD | TBD |
| `test_session.c` | TBD | TBD | TBD | TBD |
| `test_discovery_services.c` | TBD | TBD | TBD | TBD |
| `test_address_space_values.c` | TBD | TBD | TBD | TBD |
| `test_binary_extension_object_errors.c` | TBD | TBD | TBD | TBD |
| `test_security_identity_errors.c` | TBD | TBD | TBD | TBD |
| `test_tcp_connection.c` | TBD | TBD | TBD | TBD |
| `test_binary_nodeid_errors.c` | TBD | TBD | TBD | TBD |
| `test_no_heap_lifecycle.c` | TBD | TBD | TBD | TBD |
| `test_read_service.c` | TBD | TBD | TBD | TBD |
| `test_message_chunk_errors.c` | TBD | TBD | TBD | TBD |
| `test_server_address_space_config.c` | TBD | TBD | TBD | TBD |
| `test_traceability_docs.c` | TBD | TBD | TBD | TBD |
| `test_public_headers.c` | TBD | TBD | TBD | TBD |
| `test_address_space_validation.c` | TBD | TBD | TBD | TBD |
| `test_minimal_server_flow.c` | TBD | TBD | TBD | TBD |
| `test_discovery_endpoint_no_session.c` | TBD | TBD | TBD | TBD |
| `test_single_client_limit.c` | TBD | TBD | TBD | TBD |
| `fake_platform.h` | TBD | TBD | TBD | TBD |
| `fake_platform.c` | TBD | TBD | TBD | TBD |
| `unity_config.h` | TBD | TBD | TBD | TBD |
| `mu_arduino_adapter.c` | TBD | TBD | TBD | TBD |
| `mu_arduino_adapter.h` | TBD | TBD | TBD | TBD |
| `value_source.c` | TBD | TBD | TBD | TBD |
| `address_space.c` | TBD | TBD | TBD | TBD |
| `node_id.c` | TBD | TBD | TBD | TBD |
| `host_tcp_adapter.c` | TBD | TBD | TBD | TBD |
| `host_tcp_adapter.h` | TBD | TBD | TBD | TBD |
| `binary_variant.c` | TBD | TBD | TBD | TBD |
| `binary_writer.c` | TBD | TBD | TBD | TBD |
| `binary_nodeid.c` | TBD | TBD | TBD | TBD |
| `binary_extension_object.c` | TBD | TBD | TBD | TBD |
| `binary_datavalue.c` | TBD | TBD | TBD | TBD |
| `binary_reader.c` | TBD | TBD | TBD | TBD |
| `binary_string.c` | TBD | TBD | TBD | TBD |
| `browse.c` | TBD | TBD | TBD | TBD |
| `session.h` | TBD | TBD | TBD | TBD |
| `read.h` | TBD | TBD | TBD | TBD |
| `discovery.c` | TBD | TBD | TBD | TBD |
| `secure_channel.c` | TBD | TBD | TBD | TBD |
| `browse.h` | TBD | TBD | TBD | TBD |
| `session.c` | TBD | TBD | TBD | TBD |
| `read.c` | TBD | TBD | TBD | TBD |
| `secure_channel.h` | TBD | TBD | TBD | TBD |
| `discovery.h` | TBD | TBD | TBD | TBD |
| `service_dispatch.c` | TBD | TBD | TBD | TBD |
| `uasc.c` | UASC MessageChunk framing | Part 6 | 6.7.2, 6.7.3, 6.7.7 | Symmetric response chunk framing |
| `uasc.h` | UASC MessageChunk framing | Part 6 | 6.7.2, 6.7.3, 6.7.7 | Symmetric response chunk framing |
| `test_uasc_framing.c` | Tests | Part 6 | 6.7.2, 6.7.3, 6.7.7 | Test symmetric response framing |
| `service_header.c` | RequestHeader / ResponseHeader | Part 4 | 7.32, 7.33 | Common request/response header codec |
| `service_header.h` | RequestHeader / ResponseHeader | Part 4 | 7.32, 7.33 | Common request/response header types |
| `test_service_header.c` | Tests | Part 4 | 7.32, 7.33 | Test request/response header codec |
| `sequence.c` | TBD | TBD | TBD | TBD |
| `service_message.h` | TBD | TBD | TBD | TBD |
| `service_message.c` | TBD | TBD | TBD | TBD |
| `message_chunk.h` | TBD | TBD | TBD | TBD |
| `sequence.h` | TBD | TBD | TBD | TBD |
| `tcp_connection.h` | TBD | TBD | TBD | TBD |
| `server_internal.h` | TBD | TBD | TBD | TBD |
| `tcp_connection.c` | TBD | TBD | TBD | TBD |
| `message_chunk.c` | TBD | TBD | TBD | TBD |
| `service_dispatch.h` | TBD | TBD | TBD | TBD |
| `opcua_ids.c` | TBD | TBD | TBD | TBD |
| `opcua_types.h` | TBD | TBD | TBD | TBD |
| `micro_opcua.h` | TBD | TBD | TBD | TBD |
| `address_space.h` | TBD | TBD | TBD | TBD |
| `opcua_ids.h` | TBD | TBD | TBD | TBD |
| `encoding.h` | TBD | TBD | TBD | TBD |
