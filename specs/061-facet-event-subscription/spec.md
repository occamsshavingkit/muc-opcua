# Spec 061: Standard Event Subscription Server Facet (B2)

Project-B facet B2 (`docs/superpowers/specs/2026-07-09-...-roadmap.md`). Feature flag
**`MUC_OPCUA_EVENT_FILTER_WHERE`** (exists; default ON standard/full; requires
`MUC_OPCUA_EVENTS` → `MUC_OPCUA_SUBSCRIPTIONS`). Scope stance (per B1 precedent, locked
with user): **implement the facet completely** — a real ContentFilter WhereClause
evaluator over an operand tree, not the current stub.

Grounded against OPC-10000-4 §7.4 (EventFilter) and §7.7 (ContentFilter) — the
FilterOperator numeric table below is verbatim from §7.7.3.

## Motivating gap (from a full read of the current implementation)

**The WhereClause is decoded and then thrown away, so the entire where-clause path is
dead code.** Specifically:

- `read_event_filter_body` (`filter_reader.c:352-380`) reads the ContentFilter's
  element/operator/operand *structure* for validation but stores **nothing** — every
  operand is `skip_extension_object_body`'d. `mu_monitored_item_create_body_t` has no
  where-clause fields at all; `subscription_helpers.c` copies only the select clauses.
  So `item->where_clause_count` is never set > 0.
- The evaluator (`event_filter.c`) is therefore never exercised against a real filter.
  And it couldn't evaluate one anyway: it consumes three pre-flattened parallel arrays
  (`operators[]`, `field_indices[]`, `int64 values[]`) — **there is no operand-type
  model** (no Literal typing, no ElementOperand links, no SimpleAttributeOperand). It
  reads **only the integer Severity field** (`read_field_int` returns 0 for every other
  index). AND/OR/NOT walk the flat array rather than an operand tree; ISNULL/BETWEEN/
  INLIST/CAST are unimplemented; LIKE is a `return true` stub.
- **Latent encoding bug:** filtering is applied at *encode* time (`notification.c:239`)
  but the EventFieldList array length prefix (`notification.c:223`) is computed
  *before* filtering — if a where-clause ever dropped an event, the length prefix would
  over-count and corrupt the response. Masked only because filtering never triggers.
- Two divergent select-clause resolvers: the tested 9-field
  `mu_event_filter_resolve_select_clause` (`event_filter.c`) is **unused in prod**; the
  live path uses a separate 5-field `resolve_event_field_type_by_name`
  (`filter_reader.c`). Capacities are magic-number `8` (no `MU_*_MAX`), and select
  clauses are accepted up to 100 but silently truncated past 8.
- Only 5 of the 9 BaseEventType fields are even carried through the event queue
  (`mu_event_notification_t`: event_type/event_id/time/message/severity) — SourceNode/
  SourceName/ReceiveTime/LocalTime are dropped, so a filter on them can't work.

## Grounded FilterOperator table (OPC-10000-4 §7.7.3)

| Operator | Code | Operands | Semantics |
|---|---:|---|---|
| Equals | 0 | 2 | operand[0] == operand[1] (with implicit type conversion) |
| IsNull | 1 | 1 | operand[0] is a null value |
| GreaterThan | 2 | 2 | operand[0] > operand[1] |
| LessThan | 3 | 2 | operand[0] < operand[1] |
| GreaterThanOrEqual | 4 | 2 | operand[0] >= operand[1] |
| LessThanOrEqual | 5 | 2 | operand[0] <= operand[1] |
| Like | 6 | 2 | operand[0] matches wildcard pattern operand[1] |
| Not | 7 | 1 | NOT operand[0] |
| Between | 8 | 3 | operand[1] <= operand[0] <= operand[2] |
| InList | 9 | 2..n | operand[0] equals any of operand[1..] |
| And | 10 | 2 | operand[0] AND operand[1] |
| Or | 11 | 2 | operand[0] OR operand[1] |
| Cast | 12 | 2 | convert operand[0] to type operand[1] |
| InView | 13 | 1 | node is in View operand[0] |
| OfType | 14 | 1 | node is operand[0]'s type or subtype |
| RelatedTo | 15 | 6 | node relates to operand[1] via operand[2] |
| BitwiseAnd | 16 | 2 | integer operand[0] & operand[1] |
| BitwiseOr | 17 | 2 | integer operand[0] \| operand[1] |

## Grounded operand model (OPC-10000-4 §7.4.4)

A ContentFilterElement's FilterOperands are ExtensionObjects, one of:
- **LiteralOperand** { value: Variant } — a constant.
- **ElementOperand** { index: UInt32 } — reference to another element (nests And/Or/Not).
- **SimpleAttributeOperand** { typeDefinitionId: NodeId, browsePath: QualifiedName[],
  attributeId: UInt32 (13=Value), indexRange: String } — addresses an **event field**
  by browse path (the only operand that reads event data; the decoder already parses
  this for select clauses, so the layout is proven in-tree).
- (AttributeOperand — general node attribute; out of scope, events use SimpleAttribute.)

## Requirements

**FR-1 — A real operand-tree ContentFilter model + decode.** Replace the three flat
parallel arrays with a proper `mu_content_filter_t`: an array of
`mu_content_filter_element_t { operator; operands[] }` where each
`mu_filter_operand_t` is a tagged union of Literal (typed Variant), Element (index),
or SimpleAttribute (resolved event-field id). `read_event_filter_body`
(`filter_reader.c`) must **decode and store** the where-clause into this model (stop
discarding it), reject malformed/oversized with `Bad_EventFilterInvalid`, and store the
model in the create-body → item. Reuse the existing `mu_content_filter_element_t`/
`mu_filter_operand_t` from the **Query service** (`query.h`) if it fits — the roadmap
notes it as a consolidation target (it already decodes Element/Literal operands).

**FR-2 — Evaluate the standard operator set over the tree.** Implement, against the
resolved event-field values (not just Severity):
Equals, IsNull, GreaterThan, LessThan, GreaterThanOrEqual, LessThanOrEqual, Not,
Between, InList, And, Or, plus **OfType** (event-type filtering — the single most
common event where-clause, "give me events of type X or its subtypes", needs the type
hierarchy) and **Like** (real wildcard match, not a stub). Boolean combinators
(And/Or/Not) resolve their operands via **ElementOperand** links (a real tree), and the
top-level result of element[0] is the filter verdict (OPC-10000-4 §7.7.1). Bitwise
And/Or and Cast: implement if cheap; InView/RelatedTo are Out of scope (need View/
reference-graph traversal). Unsupported operator in a submitted filter →
`Bad_FilterOperatorUnsupported` at CreateMonitoredItems (fail at creation, not silently
pass).

**FR-3 — A typed event-field value model.** The evaluator must compare against the real
field value with correct types: Severity (UInt16), EventType (NodeId — for OfType),
SourceNode (NodeId), SourceName/Message (String/LocalizedText), Time/ReceiveTime
(DateTime). Extend the event carrier (`mu_event_notification_t` / `mu_event_fields_t`)
to carry the fields a filter can reference (currently only 5 of 9), and give the
comparison a small typed-Variant compare with the spec's implicit conversions for the
ordered operators (§7.7.3 "same conversion rules as GreaterThan").

**FR-4 — Fix the encode-time count bug + apply at the right point.** Either filter at
event-queue admission (preferred: the WhereClause decides whether the MonitoredItem
*reports* the event, §5.13.1) so the queue only holds passing events; or, if kept at
encode time, compute the EventFieldList length prefix **after** filtering. The
`notification.c:223` pre-filter count must not survive.

**FR-5 — Consolidate resolvers + capacity macros.** One select-clause/event-field
resolver (not the current unused-vs-used pair). Replace magic-number `8` with
`MU_INTERN_*` capacity macros (`MU_MAX_SELECT_CLAUSES`, `MU_MAX_WHERE_ELEMENTS`,
`MU_MAX_WHERE_OPERANDS`, `MU_MAX_WHERE_DEPTH`) in the profile cascade
(`capacities.h`); reject filters exceeding them with `Bad_EventFilterInvalid` /
`Bad_TooManyOperations` rather than silently truncating.

**FR-6 — Conformance doc + size.** `docs/conformance/event-subscription.md` (the
supported operator set, the SimpleAttributeOperand event-field addressing, the status
codes); update `status.md`/`profile-embedded.md`. `scripts/measure_size.sh` delta in
the ledger; confirm full stays under the 128 KiB stopper.

## Verification (test-first)

- **Operator KATs** — evaluate a decoded ContentFilter tree for each supported operator
  against synthetic events: Severity `>= 500` passes/fails; `OfType` an event-type
  subtree; `And`/`Or`/`Not` nesting via ElementOperand; `Between`, `InList`, `IsNull`,
  `Like`. Non-circular: expected verdicts are hand-computed, not read back.
- **Wire e2e** — CreateMonitoredItems with a non-empty EventFilter WhereClause
  (SimpleAttributeOperand on Severity + a Literal, GreaterThanOrEqual); trigger events
  above/below the threshold; verify only passing events appear in the Publish
  EventNotificationList, and the **EventFieldList count is correct** (FR-4 regression).
  Unsupported operator → `Bad_*` at creation.
- **Malformed** — truncated/oversized where-clause → `Bad_EventFilterInvalid`; operand
  count over capacity → the grounded rejection.
- Full matrix green; non-flag build unchanged (events still delivered unfiltered).

## Out of scope

- **InView / RelatedTo** operators (View membership / reference-graph traversal).
- **AttributeOperand** (general node attribute) — events use SimpleAttributeOperand.
- Full implicit type-conversion matrix (OPC-10000-4 Table B.x) — support the practical
  numeric/string/nodeid/datetime comparisons events actually use; document the subset.
- Alarm/Condition-specific event fields beyond BaseEventType (that's OPC-10000-9,
  a later concern).

## On approval

speckit plan/tasks, execute test-first, PR against `main` with a merge commit. B2 of
Project B; B3+ follow while the size budget holds.
