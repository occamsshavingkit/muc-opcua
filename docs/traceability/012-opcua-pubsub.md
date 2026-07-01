# Traceability: OPC UA PubSub (012-opcua-pubsub)

## Target Profile
Scoped PubSub UADP/UDP Publisher and matching decoder (OPC-10000-14).
Per OPC-10000-7 §4.2/§4.3, this is scoped interoperability evidence only:
profile-targeting, no profile-compliance claim, no CTT-verified claim, and no
full PubSub Subscriber profile claim.

## Implemented Scope

- UInt32 `PublisherId`.
- UADP PayloadHeader with exactly one `DataSetWriterId`.
- One sized Data Key Frame `DataSetMessage`.
- Scalar fields encoded and decoded as OPC UA Binary Variants.
- UDP unicast, multicast, or broadcast destination selected by
  `mu_pubsub_connection_t.address`; `NULL` keeps the host broadcast default.
- Caller-storage UADP NetworkMessage decoder for the publisher-compatible
  layout, using caller-provided `mu_variant_t` output slots.

## Explicitly Out of Scope

- PubSub security headers and secured PubSub messages.
- Non-UInt32 `PublisherId` layouts, ExtendedFlags2, and non-DataSetMessage
  payload types.
- Multiple PayloadHeader entries, multiple `DataSetMessages`, and multiple
  DataSetWriters per WriterGroup.
- Metadata NetworkMessages, discovery NetworkMessages, chunked NetworkMessages,
  delta frames, event messages, action messages, and RawData-only
  metadata-dependent payloads.
- Variant arrays and unsupported complex Variant field encodings.
- Dynamic PublishedDataSet, DataSetReader, or PubSub configuration management.
- MQTT, AMQP/broker, JSON, and Ethernet mappings/transports.

## Mappings

| Implementation File | OPC-10000 Section | Description |
|---------------------|-------------------|-------------|
| `include/micro_opcua/pubsub.h` | OPC-10000-14 §7.2.4.4.2, §7.2.4.5.2, §7.2.4.5.5, §7.3.2.1 | Public scoped publisher configuration API and caller-owned decoder output contract |
| `src/encoding/uadp_encoder.c` | OPC-10000-14 §7.2.4.4.2, §7.2.4.5.2, §7.2.4.5.3, §7.2.4.5.4, §7.2.4.5.5; OPC-10000-6 §5.2.2.16 | UADP NetworkMessage, PayloadHeader, Data Key Frame, and Variant field encode/decode |
| `src/core/pubsub.c` | OPC-10000-14 §5.4.6.2.2, §7.3.2.1 | Cooperative Publisher timing and UDP send dispatch |
| `src/core/server.c` | OPC-10000-14 §5.4.6.2.2, §7.3.2.1 | `mu_server_poll()` drives PubSub without requiring an active TCP Session |
| `src/platform/host_udp_adapter.c` | OPC-10000-14 §7.3.2.1 | Host UDP send adapter for UADP datagrams |
| `examples/pubsub_server/main.c` | OPC-10000-14 §7.3.2.1 | Host example for configured scalar UADP/UDP publishing |

| Test File | OPC-10000 Section | Description |
|-----------|-------------------|-------------|
| `tests/unit/test_uadp_encoding.c` | OPC-10000-14 §7.2.4.4.2, §7.2.4.5.2, §7.2.4.5.3, §7.2.4.5.4, §7.2.4.5.5; OPC-10000-6 §5.2.2.16 | Byte-level UADP encoder and decoder tests for flags, PublisherId, DataSetWriterId, DataSetMessage size, Variant fields, fixed field capacity, malformed input, and unsupported layouts |
| `tests/unit/test_pubsub.c` | OPC-10000-14 §5.4.6.2.2, §7.3.2.1 | Publisher timing including 32-bit tick wrap, configured destination address, and connectionless `mu_server_poll()` behavior |
