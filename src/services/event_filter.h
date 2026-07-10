/* src/services/event_filter.h
 *
 * EventFilter WhereClause (ContentFilter) model + evaluator.
 * OPC-10000-4 §7.7 (ContentFilter) and §7.4 (EventFilter).
 *
 * The wire ContentFilter is decoded into the compact, self-contained
 * mu_where_clause_t below and stored owned by the MonitoredItem so it can
 * outlive the request PDU (LiteralOperand string/bytestring payloads are
 * copied into an inline blob). This shares the FilterOperator model with the
 * Query service (query.h) but keeps a compact per-item storage form rather
 * than the Query service's transient, PDU-pointing mu_content_filter_t.
 */
#ifndef MU_SERVICES_EVENT_FILTER_H
#define MU_SERVICES_EVENT_FILTER_H

#include "muc_opcua/config.h"

#ifdef MUC_OPCUA_EVENTS

#include "muc_opcua/capacities.h"
#include "muc_opcua/types.h"
#include <stdbool.h>

/* BaseEventType field indices (OPC-10000-5 §6.4.2): the resolution target of a
   SimpleAttributeOperand BrowsePath. This server's event model emits
   EventId/EventType/Time/Message/Severity; the other fields resolve to a Null
   value (a comparison against Null is false; IsNull is true) per §7.7.3. */
typedef enum {
    MU_EVENT_FIELD_EVENTID = 0,
    MU_EVENT_FIELD_EVENTTYPE = 1,
    MU_EVENT_FIELD_SOURCENODE = 2,
    MU_EVENT_FIELD_SOURCENAME = 3,
    MU_EVENT_FIELD_TIME = 4,
    MU_EVENT_FIELD_RECEIVETIME = 5,
    MU_EVENT_FIELD_LOCALTIME = 6,
    MU_EVENT_FIELD_MESSAGE = 7,
    MU_EVENT_FIELD_SEVERITY = 8,
    MU_EVENT_FIELD_NONE = 0xFFu
} mu_event_field_t;

/* Resolve a SimpleAttributeOperand terminal BrowseName to a field index.
   Shared by the select-clause and where-clause decoders (filter_reader.c). */
mu_event_field_t mu_event_field_from_name(const mu_string_t *name);

#if MUC_OPCUA_EVENT_FILTER_WHERE

/* SimpleAttributeOperand_Encoding_DefaultBinary (OPC UA NodeIds.csv; the
   DataType/Xml/Binary triple is contiguous, matching ElementOperand 592/594
   and LiteralOperand 595/597 already used by the Query decoder). */
#define MU_ID_SIMPLEATTRIBUTEOPERAND_ENCODING_DEFAULTBINARY 603u
#define MU_ID_ELEMENTOPERAND_ENCODING_DEFAULTBINARY 594u
#define MU_ID_LITERALOPERAND_ENCODING_DEFAULTBINARY 597u

typedef enum {
    MU_WHERE_OPERAND_ELEMENT = 0u,   /* reference to another ContentFilterElement */
    MU_WHERE_OPERAND_LITERAL = 1u,   /* constant value */
    MU_WHERE_OPERAND_ATTRIBUTE = 2u, /* SimpleAttributeOperand → event field */
    MU_WHERE_OPERAND_UNSUPPORTED = 3u
} mu_where_operand_kind_t;

/* A single FilterOperand in compact owned form. Strings/bytestrings and
   string NodeId identifiers live in the owning clause's blob (blob_off/len). */
typedef struct {
    opcua_byte_t kind;       /* mu_where_operand_kind_t */
    opcua_byte_t field;      /* ATTRIBUTE: mu_event_field_t */
    opcua_byte_t lit_type;   /* LITERAL: mu_builtin_type_t */
    opcua_uint16_t blob_off; /* LITERAL string/bytestring/NodeId-string payload */
    opcua_uint16_t blob_len;
    opcua_uint32_t element_index; /* ELEMENT: index into elements[] */
    opcua_uint16_t nodeid_ns;     /* LITERAL NodeId namespace */
    union {
        opcua_int64_t i;               /* integer / boolean / datetime literal */
        opcua_double_t d;              /* float / double literal */
        opcua_uint32_t nodeid_numeric; /* numeric NodeId literal */
    } num;
} mu_where_operand_t;

typedef struct {
    opcua_byte_t op;           /* mu_filter_operator_t (query.h) */
    opcua_byte_t operand_base; /* index into operands[] */
    opcua_byte_t operand_count;
} mu_where_element_t;

/* An owned ContentFilter. elements[0] is the root (OPC-10000-4 §7.7.1); the
   whole clause is empty (matches all) when element_count == 0. */
typedef struct {
    mu_where_element_t elements[MU_INTERN_MAX_WHERE_ELEMENTS];
    mu_where_operand_t operands[MU_INTERN_MAX_WHERE_OPERANDS];
    opcua_byte_t blob[MU_INTERN_WHERE_BLOB_BYTES];
    opcua_byte_t element_count;
    opcua_byte_t operand_count;
    opcua_uint16_t blob_len;
} mu_where_clause_t;

/* Evaluate the where-clause against event fields. Empty clause → true.
   Returns whether the event passes the filter. OPC-10000-4 §7.7.1. */
bool mu_where_clause_eval(const mu_where_clause_t *filter, const mu_event_fields_t *fields);

#endif /* MUC_OPCUA_EVENT_FILTER_WHERE */

#endif /* MUC_OPCUA_EVENTS */

#endif /* MU_SERVICES_EVENT_FILTER_H */
