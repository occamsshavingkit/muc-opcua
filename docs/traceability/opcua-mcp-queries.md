# Traceability: OPC UA MCP Queries

This ledger records queries made to the OPC UA reference MCP for implementation decisions.

| Date | Query | Purpose | Resulting Decision/Code |
|------|-------|---------|-------------------------|
| 2026-06-30 | OPC-10000-14 `UADP NetworkMessage layout` | Ground scoped PubSub NetworkMessage header work | Use OPC-10000-14 section 7.2.4.4.2 for UADP flags and UInt32 PublisherId handling in `src/encoding/uadp_encoder.c`. |
| 2026-06-30 | OPC-10000-14 `PayloadHeader` | Ground payload count and DataSetWriterId work | Use OPC-10000-14 section 7.2.4.5.2 for the one-entry PayloadHeader accepted by the scoped decoder. |
| 2026-06-30 | OPC-10000-14 `DataSet payload / DataSetMessage / Data Key Frame` | Ground DataSetMessage size, header, body, and unsupported layouts | Use OPC-10000-14 sections 7.2.4.5.3, 7.2.4.5.4, 7.2.4.5.5, 7.2.4.5.6, and 7.2.4.5.7 for sized DataSet payload, Data Key Frame support, and explicit rejection of delta/event layouts. |
| 2026-06-30 | OPC-10000-14 `UDP UADP transport` | Ground transport-scope documentation | Use OPC-10000-14 section 7.3.2.1 to document UDP payload input/output without adding a receive loop. |
| 2026-06-30 | OPC-10000-6 `Variant Binary DataEncoding` | Ground PubSub field encoding | Use OPC-10000-6 section 5.2.2.16 for scalar Variant encode/decode in UADP fields. |
| 2026-06-30 | OPC-10000-7 `Conformance Units / Profiles` | Ground conformance-claim language | Use OPC-10000-7 sections 4.2 and 4.3 for profile-targeting/no-profile-compliance wording and stale-claim guards. |
