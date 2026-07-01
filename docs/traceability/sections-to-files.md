# Traceability: OPC UA Sections to Files

This document maps explicit OPC UA normative sections to implementation and test files.

| OPC UA Part | Section | Description | Files |
|-------------|---------|-------------|-------|
| 3 | 5.2.1 | Base NodeClass | `include/micro_opcua/address_space.h`, `src/address_space/address_space.c` |
| 3 | 5.5.1 | Object NodeClass | `include/micro_opcua/address_space.h`, `src/address_space/address_space.c` |
| 3 | 5.6.2 | Variable NodeClass | `include/micro_opcua/address_space.h`, `src/address_space/value_source.c` |
| 3 | 5.9 | NodeClass Attributes | `include/micro_opcua/address_space.h`, `src/address_space/value_source.c` |
| 4 | 5.5.1 | Discovery Service Set | `src/services/discovery.c`, `src/core/server.c` |
| 4 | 5.5.2 | FindServers | `src/services/discovery.c` |
| 4 | 5.5.4 | GetEndpoints | `src/services/discovery.c`, `tests/unit/test_discovery_endpoint.c` |
| 4 | 5.5.4.2 | GetEndpoints parameters and profileUris filtering | `src/core/service_dispatch.c`, `src/services/discovery.c`, `tests/unit/test_discovery_endpoint.c` |
| 4 | 5.6.2 | OpenSecureChannel | `src/services/secure_channel.c` |
| 4 | 5.6.3 | CloseSecureChannel | `src/services/secure_channel.c` |
| 4 | 5.7.2 | CreateSession | `src/services/session.c` |
| 4 | 5.7.3 | ActivateSession | `src/services/session.c` |
| 4 | 5.7.4 | CloseSession | `src/services/session.c` |
| 4 | 5.9.2 | Browse | `src/services/browse.c` |
| 4 | 5.11.2 | Read | `src/services/read.c` |
| 4 | 5.13.2.4 | CreateMonitoredItems StatusCodes | `src/services/subscription.c`, `tests/unit/test_subscriptions_errors.c` |
| 4 | 5.13.3.4 | ModifyMonitoredItems StatusCodes | `src/services/subscription.c`, `tests/unit/test_subscriptions_errors.c` |
| 4 | 5.14 | Subscription Service Set | `src/services/subscription.c`, `tests/integration/test_subscriptions.c` |
| 4 | 7.29 | ReferenceDescription | `src/encoding/binary_extension_object.c` |
| 4 | 7.32 | RequestHeader | `src/core/service_message.c` |
| 4 | 7.33 | ResponseHeader | `src/core/service_message.c` |
| 4 | 7.38.2 | Common StatusCodes | `include/micro_opcua/status.h`, `src/core/status.c`, `src/encoding/uadp_encoder.c`, `docs/conformance/status.md`, `tests/unit/test_conformance_docs.c`, `tests/unit/test_traceability_docs.c`, `docs/traceability/023-conformance-docs-subscriber.md` |
| 4 | 7.22.1 | MonitoringFilter | `src/services/subscription.c`, `tests/unit/test_subscriptions_errors.c` |
| 4 | 7.22.4 | AggregateFilter | `src/core/service_dispatch.c`, `tests/unit/test_aggregate.c`, `specs/018-aggregate-subscriptions/spec.md` |
| 13 | 4.2.2.4 | Average AggregateFunction object | `src/services/subscription.c`, `tests/unit/test_aggregate.c` |
| 13 | 4.2.2.9 | Minimum AggregateFunction object | `src/services/subscription.c`, `tests/unit/test_aggregate.c` |
| 13 | 4.2.2.10 | Maximum AggregateFunction object | `src/services/subscription.c`, `tests/unit/test_aggregate.c` |
| 13 | 5.4.3.5 | Average aggregate | `src/services/subscription.c`, `tests/unit/test_aggregate.c` |
| 13 | 5.4.3.10 | Minimum aggregate | `src/services/subscription.c`, `tests/unit/test_aggregate.c` |
| 13 | 5.4.3.11 | Maximum aggregate | `src/services/subscription.c`, `tests/unit/test_aggregate.c` |
| 4 | 7.40.1 | UserIdentityToken | `src/encoding/binary_extension_object.c` |
| 4 | 7.40.3 | AnonymousIdentityToken | `src/encoding/binary_extension_object.c` |
| 4 | 7.41 | UserTokenPolicy | `src/services/discovery.c` |
| 6 | 5.2 | OPC UA Binary encoding | `src/encoding/binary_reader.c`, `src/encoding/binary_writer.c`, `tests/unit/test_binary_string_errors.c` |
| 6 | 5.2.1 | OPC UA Binary DataEncoding | `src/encoding/binary_reader.c`, `src/encoding/binary_writer.c` |
| 6 | 5.2.2.4 | String Encoding | `src/encoding/binary_string.c` |
| 6 | 5.2.2.9 | NodeId Encoding | `src/encoding/binary_nodeid.c` |
| 6 | 5.2.2.15 | ExtensionObject Encoding | `src/encoding/binary_extension_object.c` |
| 6 | 5.2.2.16 | Variant Encoding | `src/encoding/binary_variant.c`, `include/micro_opcua/pubsub.h`, `src/encoding/uadp_encoder.c`, `tests/unit/test_uadp_encoding.c`, `tests/unit/test_traceability_docs.c`, `docs/traceability/023-conformance-docs-subscriber.md` |
| 6 | 5.2.2.17 | DataValue Encoding | `src/encoding/binary_datavalue.c` |
| 6 | 5.2.5 | Array Encoding | `src/encoding/binary_reader.c`, `src/encoding/binary_writer.c` |
| 6 | 5.2.9 | Message Body Encoding | `src/core/service_message.c` |
| 6 | 6.7.1 | Secure Conversation | `src/core/message_chunk.c`, `src/services/secure_channel.c` |
| 6 | 6.7.2 | MessageChunk Structure | `src/core/message_chunk.c` |
| 6 | 6.7.3 | Chunk Type | `src/core/message_chunk.c` |
| 6 | 6.7.4 | SecureChannel Establishment | `src/services/secure_channel.c` |
| 6 | 6.7.7 | Sequence Numbers | `src/core/sequence.c` |
| 6 | 7.1.2.2 | Message Header | `src/core/message_chunk.c`, `src/core/server.c`, `tests/unit/test_message_chunk_errors.c` |
| 6 | 7.1.2.3 | Hello Message | `src/core/tcp_connection.c` |
| 6 | 7.1.2.4 | Acknowledge Message | `src/core/tcp_connection.c` |
| 6 | 7.1.5 | OPC UA TCP Errors | `src/core/tcp_connection.c` |
| 6 | 7.2 | OPC UA TCP | `src/platform/host_tcp_adapter.c` |
| 7 | 3.1.5 | Facets | |
| 7 | 4.2 | Conformance Units and Groups | `docs/conformance/profile-embedded.md`, `docs/conformance/services.md`, `tests/unit/test_conformance_docs.c` |
| 7 | 4.3 | Profiles | `docs/conformance/profile-embedded.md`, `docs/conformance/services.md`, `tests/unit/test_conformance_docs.c` |
| 7 | 4.4 | Profile Categories | |
| 7 | 4.6 | Profile Definitions | |
| 7 | 4.7 | Profile Versioning | |
| 7 | 4.8 | Applications | |
| 14 | 5.4.2.1 | PubSub Publisher component | `include/micro_opcua/pubsub.h`, `src/core/pubsub.c`, `docs/integration-guide.md` |
| 14 | 5.4.2.2 | PubSub Subscriber component | `include/micro_opcua/pubsub.h`, `src/encoding/uadp_encoder.c`, `tests/unit/test_uadp_encoding.c` |
| 14 | 5.4.6.2.2 | UADP WriterGroup message mapping | `src/core/pubsub.c`, `src/encoding/uadp_encoder.c`, `tests/unit/test_pubsub.c` |
| 14 | 7.2.4.4.2 | UADP NetworkMessage layout | `include/micro_opcua/pubsub.h`, `src/encoding/uadp_encoder.c`, `tests/unit/test_uadp_encoding.c`, `tests/unit/test_traceability_docs.c`, `docs/traceability/023-conformance-docs-subscriber.md` |
| 14 | 7.2.4.5.2 | UADP PayloadHeader | `include/micro_opcua/pubsub.h`, `src/encoding/uadp_encoder.c`, `tests/unit/test_uadp_encoding.c`, `tests/unit/test_traceability_docs.c`, `docs/traceability/023-conformance-docs-subscriber.md` |
| 14 | 7.2.4.5.3 | DataSet payload | `src/encoding/uadp_encoder.c`, `tests/unit/test_uadp_encoding.c` |
| 14 | 7.2.4.5.4 | DataSetMessage header | `src/encoding/uadp_encoder.c`, `tests/unit/test_uadp_encoding.c` |
| 14 | 7.2.4.5.5 | Data Key Frame DataSetMessage | `include/micro_opcua/pubsub.h`, `src/encoding/uadp_encoder.c`, `tests/unit/test_uadp_encoding.c`, `tests/unit/test_traceability_docs.c`, `docs/traceability/023-conformance-docs-subscriber.md` |
| 14 | 7.2.4.5.6 | Data Delta Frame DataSetMessage | `src/encoding/uadp_encoder.c`, `tests/unit/test_uadp_encoding.c` |
| 14 | 7.2.4.5.7 | Event DataSetMessage | `src/encoding/uadp_encoder.c`, `tests/unit/test_uadp_encoding.c` |
| 14 | 7.3.2.1 | UDP/UADP mapping | `include/micro_opcua/pubsub.h`, `src/core/pubsub.c`, `src/platform/host_udp_adapter.c`, `docs/integration-guide.md` |
