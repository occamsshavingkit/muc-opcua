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
| `include/micro_opcua/services/node_management.h` | NodeManagement | Part 4 | 5.7.2, 5.7.3, 5.7.4, 5.7.5 | NodeManagement services interface |
| `src/services/node_management.c` | NodeManagement | Part 4 | 5.7.2, 5.7.3, 5.7.4, 5.7.5 | NodeManagement services implementation |
| `src/services/query.h` | Query | Part 4 | 5.8 | Query services interface |
| `src/services/query.c` | Query | Part 4 | 5.8 | Query services implementation |
| `src/encoding/binary_query.c` | Encoding | Part 6 | 5.2 | Query service payload binary encoding |
| `tests/unit/test_query_encoding.c` | Tests | Part 6 | 5.2 | Query binary encode/decode tests |
| `tests/unit/test_query_service.c` | Tests | Part 4 | 5.8 | Query services tests |
| `tests/unit/test_node_management.c` | Tests | Part 4 | 5.7 | NodeManagement positive tests |
| `tests/unit/test_node_management_errors.c` | Tests | Part 4 | 5.7 | NodeManagement error tests |
| `tests/unit/test_address_space_dynamic.c` | Tests | Part 4 / 5 | 5.7 / 6.3 | Dynamic Address Space tests |
| `src/address_space/base_nodes.h` | Base Information | Part 5 | 6.3, 7 | Standard Server node set interface + resolver |
| `src/address_space/base_nodes.c` | Base Information | Part 5 | 6.3, 7 | Default Server/ServerStatus/ServerCapabilities nodes |
| `src/security/security_policy.h` | SecurityPolicy | Part 7 | 6.x | Policy id + Basic256Sha256 parameter table |
| `src/security/security_policy.c` | SecurityPolicy | Part 7 | 6.x | SecurityPolicyUri parsing |
| `src/security/certificate.h` | Certificates | Part 6 / 4 | 6.7.4 / 7.2 | Thumbprint + peer cert validation interface |
| `src/security/certificate.c` | Certificates | Part 6 / 4 | 6.7.4 / 7.2 | SHA-1 thumbprint, RSA key-size validation |
| `src/security/asym_chunk.h` | Asymmetric Chunk | Part 6 | 6.7.2 | OPN chunk sign/encrypt interface |
| `src/security/asym_chunk.c` | Asymmetric Chunk | Part 6 | 6.7.2 | OPN OAEP encrypt + RSA-SHA256 sign + padding |
| `tests/unit/test_asym_chunk.c` | Tests | Part 6 | 6.7.2 | OPN wrap/unwrap round-trip + tamper tests |
| `src/security/sym_chunk.h` | Symmetric Chunk | Part 6 | 6.7.2, 6.7.5 | MSG chunk sign/encrypt + key-derivation interface |
| `src/security/sym_chunk.c` | Symmetric Chunk | Part 6 | 6.7.2, 6.7.5 | MSG AES-256-CBC + HMAC-SHA256 (Sign / SignAndEncrypt) |
| `tests/unit/test_sym_chunk.c` | Tests | Part 6 | 6.7.2, 6.7.5 | MSG wrap/unwrap round-trip + tamper tests |
| `tests/integration/test_server_handshake_secure.c` | Tests | Part 6 | 6.7.2-6.7.5 | End-to-end Basic256Sha256 SignAndEncrypt handshake |
| `tests/integration/test_write_service.c` | Tests | Part 4 | 5.10.4 | End-to-end Write service tests |
| `tests/unit/test_security_policy.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `tests/unit/test_certificate.c` | Tests | Part 6 / 4 | 6.7.4 / 7.2 | Thumbprint + cert validation tests |
| `src/security/key_derivation.h` | Key Derivation | Part 6 | 6.7.5 | P-SHA256 PRF interface for channel keys |
| `src/security/key_derivation.c` | Key Derivation | Part 6 | 6.7.5 | P-SHA256 (RFC 5246) key material expansion |
| `src/platform/host_crypto_adapter.h` | Crypto Adapter | Part 7 | 6.x | Host crypto backend interface (Basic256Sha256) |
| `src/platform/host_crypto_adapter.c` | Crypto Adapter | Part 7 | 6.x | OpenSSL primitives for SecurityPolicy Basic256Sha256 |
| `tests/unit/test_crypto.c` | Tests | Part 6 / 7 | 6.7.5 / 6.x | Known-answer crypto + P-SHA256 tests |
| `tests/unit/test_status.c` | Tests | Part 4 / 6 | 7.38.2 / 7.1.5 | Test Status helpers |
| `tests/unit/test_server_config.c` | Tests | Part 6 | 7.1.2.3, 7.1.2.4 | Test config validation |
| `tests/unit/test_platform_adapter_contract.c` | Tests | Part 6 | 7.2 | Test adapters structure |
| `tests/unit/test_write_decoder.c` | Tests | Part 6 | 5.10.4 | Test Write service request decoder and response encoder |
| `tests/unit/test_connection_multiplex.c` | Tests | Part 4 | 5.13 | Test concurrent connection multiplexing limits |
| `fuzz_binary_reader.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `fuzz_binary_types.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `fuzz_message_chunk.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_string.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_secure_channel.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_service_state_errors.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_array_errors.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_address_space_callbacks.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_string_errors.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_primitives.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_extension_object.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_service_dispatch.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_nodeid.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_address_space_string_limits.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_browse_limits.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_discovery_endpoint.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_browse_service.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_message_chunk.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_variant_datavalue.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_session.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_discovery_services.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_address_space_values.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_extension_object_errors.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_security_identity_errors.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_tcp_connection.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_nodeid_errors.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_no_heap_lifecycle.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_read_service.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_message_chunk_errors.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_server_address_space_config.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_traceability_docs.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_public_headers.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_address_space_validation.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_minimal_server_flow.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_discovery_endpoint_no_session.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_single_client_limit.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `fake_platform.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `fake_platform.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `unity_config.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `mu_arduino_adapter.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `mu_arduino_adapter.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `value_source.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `address_space.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `node_id.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `host_tcp_adapter.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `host_tcp_adapter.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `binary_variant.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `binary_writer.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `binary_le.h` | OPC-10000-6 | 5.2.1 | OPC UA Binary little-endian pack/unpack helpers | shared by binary_reader.c / binary_writer.c |
| `binary_nodeid.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `src/encoding/binary_variant.c` | OPC UA Part 6 | 5.3.13 | Variant encoding | Variant support |
| `src/encoding/binary_datavalue.c` | OPC UA Part 6 | 5.3.14 | DataValue encoding | DataValue support |
| `src/encoding/uadp_encoder.c` | OPC UA Part 14 | 6.x | PubSub UADP encoding | PubSub UADP |
| `src/core/pubsub.c` | OPC UA Part 14 | 6.x | PubSub engine | PubSub runtime |
| `src/platform/host_udp_adapter.c` | OPC UA Part 14 | 6.x | UDP transport | PubSub network |
| `include/micro_opcua/pubsub.h` | OPC UA Part 14 | 6.x | PubSub headers | PubSub network |
| `binary_extension_object.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `binary_datavalue.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `binary_reader.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `binary_string.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `browse.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `session.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `read.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `discovery.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `secure_channel.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `browse.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `services/alarms_conditions.h` | Alarms and Conditions | Part 9 | 5.11 | Advanced alarms and conditions |
| `services/history.h` | Historical Access | Part 4 / 11 | 5.10.3, 5.10.4 | Historical Access structures and types |
| `session.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `read.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `write.h` | Attribute Write | Part 4 | 5.10.4 | Write service interface |
| `write.c` | Attribute Write | Part 4 | 5.10.4 | Write service implementation |

### Implementation (`src/`)

| File | Part | Sections | Notes |
|------|------|----------|-------|
| `core/server.c` | 4 | 5.4, 5.5 | Service dispatch |
| `services/alarms_conditions.c` | 9 | 5.11 | Alarms and Conditions execution |
| `services/history.c` | 4, 11 | 5.10.3, 5.10.4 | HistoryRead, HistoryUpdate parsing and dispatch |
| `services/read.c` | 4 | 5.10.2 | Read service implementation |
| `secure_channel.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `discovery.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `service_dispatch.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `uasc.c` | UASC MessageChunk framing | Part 6 | 6.7.2, 6.7.3, 6.7.7 | Symmetric response chunk framing |
| `uasc.h` | UASC MessageChunk framing | Part 6 | 6.7.2, 6.7.3, 6.7.7 | Symmetric response chunk framing |
| `test_uasc_framing.c` | Tests | Part 6 | 6.7.2, 6.7.3, 6.7.7 | Test symmetric response framing |
| `service_header.c` | RequestHeader / ResponseHeader | Part 4 | 7.32, 7.33 | Common request/response header codec |
| `service_header.h` | RequestHeader / ResponseHeader | Part 4 | 7.32, 7.33 | Common request/response header types |
| `test_service_header.c` | Tests | Part 4 | 7.32, 7.33 | Test request/response header codec |
| `sequence.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `service_message.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `service_message.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `message_chunk.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `sequence.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `tcp_connection.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `server_internal.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `tcp_connection.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `message_chunk.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `service_dispatch.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `opcua_ids.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `opcua_types.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `micro_opcua.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `address_space.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `opcua_ids.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `encoding.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `subscription.c` | Subscription + MonitoredItem service sets (Micro) | Part 4 | 5.13, 5.14 | No-heap data-change subscription engine |
| `subscription.h` | Subscription + MonitoredItem service sets (Micro) | Part 4 | 5.13, 5.14, 7.17, 7.21 | Subscription engine data model + contract |
| `mbedtls_crypto_adapter.c` | Crypto Adapter | Part 7 | 6.x | Mbed TLS primitives for SecurityPolicy Basic256Sha256 |
| `wolfssl_crypto_adapter.c` | Crypto Adapter | Part 7 | 6.x | wolfSSL primitives for SecurityPolicy Basic256Sha256 |
| `test_mbedtls_adapter.c` | Tests | Part 7 | 6.x | Test Mbed TLS crypto adapter |
| `test_wolfssl_adapter.c` | Tests | Part 7 | 6.x | Test wolfSSL crypto adapter |
| `src/security/trustlist.c` | TrustList | Part 4 | 5.6.2 | Application Authentication TrustList |
| `include/micro_opcua/security.h` | TrustList | Part 4 | 5.6.2 | Application Authentication TrustList Header |
| `tests/unit/test_aggregate.c` | Tests | Part 4 / 13 | 7.22.4 / 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, 5.4.3.11 | Unit tests for scoped MonitoredItem AggregateFilter support |
| `specs/018-aggregate-subscriptions/spec.md` | Specification | Part 4 / 13 | 7.22.4 / 4.2.2.4, 4.2.2.9, 4.2.2.10 | Specification for Aggregate Subscriptions |
| `include/micro_opcua/services/alarms_conditions.h` | Alarms and Conditions | Part 9 | 5.5, 5.7, 5.8 | Alarms & Conditions types and API |
| `src/services/alarms_conditions.c` | Alarms and Conditions | Part 9 | 5.5, 5.7, 5.8 | Alarms & Conditions method processing |
| `tests/unit/test_alarms_conditions.c` | Tests | Part 9 | 5.5, 5.7, 5.8 | Test Alarms & Conditions functionality |
