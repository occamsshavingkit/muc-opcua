# Data Model: Conformance Docs and PubSub Subscriber

## Documentation Evidence Row

Represents a user- or maintainer-facing support claim.

**Fields**

- `claim`: The documented service/profile/status/resource claim.
- `status`: `implemented subset`, `optional implemented subset`, `unsupported`, `profile-targeting`, `experimental`, or `CTT-verified`.
- `opc_reference`: Exact OPC UA part and section when the claim affects standard behavior.
- `evidence`: Test, fixture, traceability row, CI check, or validation artifact.
- `snapshot_date`: Required when the row includes measured sizes or benchmark numbers.
- `reproduction_command`: Required when the row includes measured sizes or benchmark numbers.

**Validation rules**

- Claims of profile compliance or CTT verification require explicit external evidence.
- Numeric claims must be either tested constants or labeled snapshots with reproduction commands.
- Service/support claims must not contradict `docs/conformance/services.md` or profile documents.

## Stale Numeric Guard

Represents a CI-enforced assertion against stale values.

**Fields**

- `subject`: StatusCode, NodeId, size number, benchmark number, or OPC UA section reference.
- `expected_value`: Canonical value or allowed pattern.
- `source`: Header, docs file, generated table, benchmark artifact, or size report.
- `failure_message`: Precise message for maintainers.

**Validation rules**

- StatusCode guards compare documented names/hex values to `include/micro_opcua/status.h`.
- Aggregate NodeId guards reject known stale values `11565`, `11569`, and `11570` as AggregateFunction identifiers and require Average `2342`, Minimum `2346`, Maximum `2347`.
- Section-reference guards reject vague placeholders such as `5.9.x` for Query rows.

## Scoped UADP Subscriber Message

Represents a decoded message from the supported UADP/UDP subset.

**Fields**

- `publisher_id`: UInt32 PublisherId from OPC-10000-14 §7.2.4.4.2.
- `data_set_writer_id`: UInt16 DataSetWriterId from OPC-10000-14 §7.2.4.5.2.
- `fields`: Caller-provided array of scalar `mu_variant_t` values decoded according to OPC-10000-6 §5.2.2.16.
- `field_count`: Number of decoded fields.

**Validation rules**

- The input must be one UADP NetworkMessage with DataSetMessage payload.
- The payload header must contain exactly one DataSetWriterId for this feature.
- The DataSetMessage must be a valid Data Key Frame with Variant field encoding.
- Field count must fit the caller-provided output capacity and `MU_PUBSUB_MAX_FIELDS`.
- Unsupported UADP layouts fail with `Bad_NotSupported`; malformed bytes fail with `Bad_DecodingError`.

## Unsupported PubSub Layout

Represents a valid or malformed PubSub payload outside the scoped decoder.

**Examples**

- UADP security header or encrypted/signed message.
- Chunk NetworkMessage.
- Discovery probe or announcement message.
- Multiple DataSetMessages in one payload.
- Delta frame, event message, action request, or action response.
- RawData-only payload requiring DataSetMetaData.
- JSON, MQTT/broker, Ethernet, or DTLS mapping.

**Validation rules**

- Unsupported layouts do not mutate trusted output.
- Unsupported layouts do not promote a broader PubSub conformance claim.
