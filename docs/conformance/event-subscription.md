# Conformance: Standard Event Subscription Server Facet (spec 061)

This server evaluates the OPC UA **EventFilter WhereClause** (a ContentFilter,
OPC-10000-4 §7.7) applied to event MonitoredItems — the Event-Subscription slice of the
Project-B Part-7 rollout (B2). Gated behind **`MUC_OPCUA_EVENT_FILTER_WHERE`** (default
**ON for standard/full**), which requires `MUC_OPCUA_EVENTS` and
`MUC_OPCUA_SUBSCRIPTIONS_STANDARD` (features.h guards). With the flag off, event
MonitoredItems still deliver their SelectClause fields unfiltered.

Everything below is grounded against OPC-10000-4: the FilterOperator table from §7.7.3,
the operand types from §7.4.4/§7.7.4, and the BaseEventType field BrowsePaths from
OPC-10000-5 §6.4.2. The SimpleAttributeOperand encoding NodeId (603) follows the
contiguous DataType/Xml/Binary triple confirmed in-tree by ElementOperand (594) and
LiteralOperand (597).

## Supported FilterOperators (OPC-10000-4 §7.7.3)

The WhereClause is decoded into an owned operand tree; `elements[0]` is the root and its
boolean result is the filter verdict. A submitted filter using any **unsupported**
operator fails the MonitoredItem at CreateMonitoredItems with
`Bad_FilterOperatorUnsupported` (not silently accepted).

| Operator | Code | Supported | Notes |
|---|---:|:---:|---|
| Equals | 0 | ✅ | type-aware (numeric / string / NodeId) |
| IsNull | 1 | ✅ | absent event fields (unset string / null NodeId) are Null |
| GreaterThan | 2 | ✅ | numeric + lexical string ordering |
| LessThan | 3 | ✅ | |
| GreaterThanOrEqual | 4 | ✅ | |
| LessThanOrEqual | 5 | ✅ | |
| Like | 6 | ✅ | `%` `_` `[set]` `[^set]` `a-b` ranges, `\` escape |
| Not | 7 | ✅ | operand is an ElementOperand |
| Between | 8 | ✅ | `op1 <= op0 <= op2` |
| InList | 9 | ✅ | `op0` equals any of `op1..` |
| And | 10 | ✅ | operands are ElementOperands (real tree) |
| Or | 11 | ✅ | |
| OfType | 14 | ⚠️ | **exact EventType match** — subtype-of resolution needs an event-type hierarchy this profile does not maintain |
| Cast | 12 | ❌ | `Bad_FilterOperatorUnsupported` |
| InView | 13 | ❌ | needs View membership |
| RelatedTo | 15 | ❌ | needs reference-graph traversal |
| BitwiseAnd/Or | 16/17 | ❌ | `Bad_FilterOperatorUnsupported` |

## Operand model (OPC-10000-4 §7.4.4)

FilterOperands are decoded ExtensionObjects:

| Operand | Encoding NodeId | Handling |
|---|---:|---|
| ElementOperand | 594 | index into another ContentFilterElement (nesting) |
| LiteralOperand | 597 | typed Variant constant; string/NodeId payloads copied into an item-owned blob |
| SimpleAttributeOperand | 603 | terminal BrowseName → BaseEventType field (below) |
| AttributeOperand | 600 | not supported (events use SimpleAttributeOperand) → rejected |

## Addressable event fields (OPC-10000-5 §6.4.2)

A SimpleAttributeOperand BrowsePath resolves to a BaseEventType field index. This
server's event model emits five fields; the rest resolve to a **Null** value (so a
comparison against them is false and `IsNull` is true):

| Field | Index | Type | Emitted |
|---|---:|---|:---:|
| EventId | 0 | ByteString | ✅ |
| EventType | 1 | NodeId | ✅ |
| SourceNode | 2 | NodeId | — (Null) |
| SourceName | 3 | String | — (Null) |
| Time | 4 | DateTime | ✅ |
| ReceiveTime | 5 | DateTime | — (Null) |
| LocalTime | 6 | TimeZoneDataType | — (Null) |
| Message | 7 | LocalizedText | ✅ |
| Severity | 8 | UInt16 | ✅ |

## Status codes

| Condition | Status |
|---|---|
| Unsupported FilterOperator | `Bad_FilterOperatorUnsupported` (0x80C20000) |
| Operand/element count over capacity (`MU_INTERN_MAX_WHERE_ELEMENTS`/`_OPERANDS`) | `Bad_TooManyOperations` (0x80100000) |
| Malformed / oversized WhereClause, unsupported operand | `Bad_EventFilterInvalid` (0x80470000) |
| Truncated ContentFilter bytes | `Bad_DecodingError` (0x80070000) |

Rejections are reported as the per-item `filterResult`/StatusCode at CreateMonitoredItems;
the whole request is not faulted.

## Filtering semantics

The WhereClause is applied per MonitoredItem at Publish encode time: an event is emitted
only if `mu_where_clause_eval` returns true, and the EventFieldList array length is
written **after** filtering (a backpatch), so a dropped event never over-counts the array
(which would desync the Publish stream). An empty WhereClause matches every event.

## Verification

- `tests/unit/test_event_filter_where.c` — operator KATs over hand-built operand trees
  (Equals, relational, IsNull, Between, And/Or/Not tree, Like, OfType); verdicts
  hand-computed (non-circular).
- `tests/unit/test_event_filter_select.c` — BrowseName → field-index resolver.
- `tests/integration/test_event_notifications.c` — CreateMonitoredItems with a live
  WhereClause (`Severity >= 500`): only the passing event is published and the
  EventFieldList count is exactly 1 (count regression); an unsupported operator fails the
  item with `Bad_FilterOperatorUnsupported`.
- `tests/fuzz/fuzz_event_filter_where.c` — arbitrary operand trees against the evaluator.
