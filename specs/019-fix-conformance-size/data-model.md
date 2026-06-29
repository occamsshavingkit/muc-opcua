# Data Model: OPC UA Conformance and Size Repairs

## StatusCode Constant Set

Represents the public mapping from `MU_STATUS_*` names to standard 32-bit OPC UA StatusCode values.

**Fields**

- `macro_name`: Public C macro name.
- `standard_name`: OPC UA StatusCode name.
- `wire_value`: Canonical 32-bit numeric value from `StatusCode.csv`.
- `reference`: OPC-10000-4 §7.38.2 plus CSV row source.

**Validation Rules**

- Every value changed by this feature has a unit test using a numeric literal from the official table.
- Duplicate local macro definitions for the same status are removed or consolidated.
- Diagnostic status-name mappings remain aligned with corrected values.

## Aggregate Filter Contract

Represents the scoped monitored-item aggregate filter accepted by this library.

**Fields**

- `encoding_node_id`: `AggregateFilter_Encoding_DefaultBinary`, namespace 0 numeric `730`.
- `aggregate_function_node_id`: Supported namespace 0 numeric values: Average `2342`, Minimum `2346`, Maximum `2347`.
- `processing_interval_ms`: Positive finite interval derived from the encoded Duration.
- `configuration_scope`: AggregateConfiguration fields decoded for wire correctness, with unsupported semantics either ignored only when safe or rejected with a cited StatusCode.

**Validation Rules**

- Non-standard or stale aggregate function NodeIds are rejected.
- Unsupported aggregate functions return `Bad_MonitoredItemFilterUnsupported`.
- Invalid processing intervals return the documented monitored-item filter failure result.

## Endpoint Filter Contract

Represents the subset of `GetEndpoints` request filtering implemented by this feature.

**Fields**

- `endpoint_url`: Request field parsed for forward compatibility; not used for locale/profile filtering decisions in this feature unless implemented safely.
- `locale_ids`: Request field parsed or skipped according to binary encoding rules.
- `profile_uris`: Requested transport profile URI list from OPC-10000-4 §5.5.4.2.
- `endpoint_transport_profile_uri`: Transport profile URI advertised by each endpoint.

**Validation Rules**

- Empty `profile_uris` means all local endpoints may be returned.
- Non-empty `profile_uris` returns only endpoints whose transport profile URI equals at least one requested URI.
- Response service result remains Good when no endpoints match.

## Size Measurement Baseline

Represents profile measurements used to prevent hidden resource regressions.

**Fields**

- `profile`: nano, micro, embedded, or full.
- `flash_text_bytes`: ARM archive text bytes.
- `static_ram_bytes`: Measured host proxy for major structs when available.
- `stack_budget_status`: Pass/fail and largest emitted frame details.
- `delta`: Difference from audit baseline.

**Validation Rules**

- Measurements must be recorded after implementation.
- Any flash delta greater than 1 percent is documented.
- Static RAM must not increase in embedded defaults.
