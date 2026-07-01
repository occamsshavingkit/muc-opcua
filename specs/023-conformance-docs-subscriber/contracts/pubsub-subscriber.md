# Contract: Scoped UADP Subscriber Decode

## Public Behavior

The subscriber decode entry point accepts a UDP payload containing one scoped UADP NetworkMessage and caller-provided output slots. On success, it fills a decoded message structure with:

- UInt32 PublisherId.
- UInt16 DataSetWriterId.
- Decoded scalar Variant fields.
- Number of decoded fields.

The decoder does not allocate memory and does not own the input payload. Fixed-width scalar Variant values are copied into caller-provided slots. For String, ByteString, QualifiedName, and LocalizedText Variants, the input buffer must outlive decoded field values because payload bytes may be borrowed from that buffer.

## Accepted Wire Shape

- UADP NetworkMessage with version 1 and DataSetMessage payload.
- UInt32 PublisherId.
- PayloadHeader present with exactly one DataSetWriterId.
- One DataSetMessage size entry.
- Data Key Frame DataSetMessage.
- Variant field encoding for scalar fields.

## Rejected Wire Shape

| Condition | StatusCode |
|---|---|
| Null input pointer, null decoded-message pointer, or null field output pointer with nonzero field capacity | `Bad_InvalidArgument` |
| Truncated header, payload header, DataSetMessage size, or Variant field | `Bad_DecodingError` |
| Declared DataSetMessage size exceeds remaining payload | `Bad_DecodingError` |
| More decoded fields than caller capacity, including zero capacity for a payload that contains fields, or more than `MU_PUBSUB_MAX_FIELDS` | `Bad_TooManyOperations` |
| Unsupported PublisherId type | `Bad_NotSupported` |
| Security header, ExtendedFlags2 message types other than DataSetMessage payload, chunk NetworkMessage, discovery message, multiple DataSetMessages, delta/event/action message, RawData-only metadata-dependent payload | `Bad_NotSupported` |

## OPC UA References

- OPC-10000-14 §5.4.2.1 and §5.4.2.2 for Subscriber/message reception.
- OPC-10000-14 §5.4.6.2.2 and §7.3.2.1 for UADP over UDP.
- OPC-10000-14 §7.2.4.1, §7.2.4.2, §7.2.4.4.2 for UADP NetworkMessage mapping and layout.
- OPC-10000-14 §7.2.4.5.2, §7.2.4.5.3, §7.2.4.5.4, and §7.2.4.5.5 for payload header, payload sizes, DataSetMessage header, and Data Key Frame structure.
- OPC-10000-6 §5.2.2.16 for Variant Binary DataEncoding.
- OPC-10000-4 §7.38.2 for StatusCode names and values.
