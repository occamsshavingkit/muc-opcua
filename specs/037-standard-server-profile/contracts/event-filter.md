# Contract: EventFilter Where-Clause Evaluation

**Feature**: 037-standard-server-profile | **OPC Reference**: OPC-10000-4 §7.22.3

## EventFilter Structure

```
EventFilter ::=
  SelectClauses[]  : SimpleAttributeOperand[]  -- fields to return
  WhereClause      : ContentFilter             -- filter expression
```

## ContentFilter Evaluation

```
ContentFilter ::=
  Elements[] : ContentFilterElement[]
    FilterOperator  : enum { Equals, IsNull, GreaterThan, LessThan,
                             GreaterThanOrEqual, LessThanOrEqual,
                             Like, Not, Between, InList, And, Or, Cast,
                             InView, OfType, RelatedTo, BitwiseAnd,
                             BitwiseOr, EqualsNull }
    FilterOperands[]: ExtensionObject[]  -- SimpleAttributeOperand or ElementOperand
```

## Evaluation Engine

```c
// Evaluate where-clause against an event instance.
// Returns true if the event matches, false otherwise.
bool mu_evaluate_event_filter_where(
    const mu_content_filter_element_t *elements,
    uint16_t element_count,
    const mu_event_fields_t *event_fields
);

// Supported operators (Phase 1):
//   Equals, GreaterThan, LessThan, GreaterThanOrEqual, LessThanOrEqual,
//   NotEqual, Like (basic wildcard *), And, Or, Not
//
// Supported operand types:
//   SimpleAttributeOperand with TypeDefinitionId pointing to BaseEventType
//   Operand index maps to select-clause index, resolved against event_fields

// Event field values for evaluation:
typedef struct {
    mu_byte_string_t  event_id;
    mu_node_id_t      event_type;
    mu_node_id_t      source_node;
    const char       *source_name;
    mu_date_time_t    time;
    mu_date_time_t    receive_time;
    mu_time_zone_t    local_time;
    mu_localized_text_t message;
    uint16_t          severity;
} mu_event_fields_t;
```

## Select Clause Extraction

For each SimpleAttributeOperand in SelectClauses, extract the requested BaseEventType
field from the event instance. Supported BrowsePaths include:
- `/EventId`, `/EventType`, `/SourceNode`, `/SourceName`
- `/Time`, `/ReceiveTime`, `/LocalTime`
- `/Message`, `/Severity`

Unrecognized BrowsePaths return a null Variant in the corresponding EventField.

## Limits

- Maximum ContentFilter element nesting depth: 4
- Maximum ContentFilter elements per filter: 16
- Maximum select clauses per filter: 32

## Test Contract

1. **Filter match**: Create event with Severity=800, set where-clause `Severity >= 500` → event delivered.
2. **Filter no-match**: Create event with Severity=300, same filter → event NOT delivered.
3. **AND filter**: `Severity >= 500 AND SourceName == "MyNode"` → only matching events delivered.
4. **OR filter**: `Severity >= 900 OR Severity <= 100` → matching events delivered.
5. **Multi-field select**: Select EventId, Time, Severity, SourceNode → all four fields populated.
6. **Unknown BrowsePath**: Select clause with bad path → null Variant in EventField.
7. **Empty filter**: No where-clause → all events delivered.

## Gating

`MUC_OPCUA_EVENT_FILTER_WHERE` CMake flag. When OFF, where-clause is ignored (all events delivered, current behavior).
