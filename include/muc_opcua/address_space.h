/* include/muc_opcua/address_space.h */
#ifndef MUC_OPCUA_ADDRESS_SPACE_H
#define MUC_OPCUA_ADDRESS_SPACE_H

#include "muc_opcua/config.h"
#include "muc_opcua/status.h"
#include "muc_opcua/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MU_NODECLASS_OBJECT = 1,
    MU_NODECLASS_VARIABLE = 2,
    MU_NODECLASS_METHOD = 4,
    MU_NODECLASS_OBJECTTYPE = 8,
    MU_NODECLASS_VARIABLETYPE = 16,
    MU_NODECLASS_REFERENCETYPE = 32,
    MU_NODECLASS_DATATYPE = 64,
    MU_NODECLASS_VIEW = 128
} mu_node_class_t;

typedef struct {
    mu_nodeid_t reference_type_id;
    mu_nodeid_t target_id;
    opcua_boolean_t is_forward;
} mu_reference_t;

typedef enum { MU_VALUESOURCE_STATIC = 0, MU_VALUESOURCE_CALLBACK = 1 } mu_value_source_type_t;

/* Read callback function signature */
typedef opcua_statuscode_t (*mu_read_callback_t)(void *context, const mu_nodeid_t *node_id, mu_variant_t *value);

typedef struct {
    mu_value_source_type_t type;
    union {
        mu_variant_t static_value;
        struct {
            mu_read_callback_t read;
            void *context;
        } callback;
    } data;
} mu_value_source_t;

struct mu_server;

/* Method callback function signature (OPC-10000-4 §5.12.2) */
typedef opcua_statuscode_t (*mu_method_callback_t)(struct mu_server *server, const mu_nodeid_t *object_id,
                                                   const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                                   size_t input_args_count, mu_variant_t *output_args,
                                                   size_t *output_args_count);

typedef struct {
    mu_nodeid_t node_id;
    mu_node_class_t node_class;
    mu_string_t browse_name;
    mu_string_t display_name;

    /* References array */
    const mu_reference_t *references;
    size_t reference_count;

    /* Optional value source for variables */
    const mu_value_source_t *value;

    /* Cached HasTypeDefinition target (OPC 10000-3). Zero-initialized when the
     * node has no TypeDefinition. Eliminates the O(R*T) scan in Browse. */
    mu_nodeid_t type_definition;

    /* EventNotifier attribute (OPC-10000-3 §5.4.6). Bits: 0=SubscribeToEvents, 1=HistoryRead.
     * Zero means the node does not support event subscriptions. */
    opcua_byte_t event_notifier;
} mu_node_t;

typedef struct mu_address_space mu_address_space_t;

/* Bounded NodeId index for sub-linear lookup. Entries are sorted by the
 * NodeId sort key (namespace, identifier_type, numeric value or string hash);
 * a binary-search hit is confirmed with mu_nodeid_equal. */
typedef struct {
    opcua_uint16_t order[MU_INTERN_MAX_ADDRESS_SPACE_NODES]; /* node indices sorted by NodeId sort key */
    size_t count;                                            /* number of indexed nodes */
    opcua_boolean_t indexed;                                 /* false => fall back to linear scan (node_count > cap) */
    const mu_address_space_t *built_for;                     /* address space currently represented by order[] */
    size_t built_count;                                      /* node count used when order[] was built */
} mu_address_space_index_t;

struct mu_address_space {
    const mu_node_t *nodes;
    size_t node_count;
};

/* Address space validation */
opcua_statuscode_t mu_address_space_validate(const mu_address_space_t *address_space);

/* NodeId helpers */
static inline opcua_boolean_t mu_nodeid_is_ns0_numeric(const mu_nodeid_t *nodeid) {
    return nodeid->identifier_type == MU_NODEID_NUMERIC && nodeid->namespace_index == 0u;
}
opcua_boolean_t mu_nodeid_equal(const mu_nodeid_t *n1, const mu_nodeid_t *n2);
opcua_boolean_t mu_nodeid_in_namespace(const mu_nodeid_t *node_id, opcua_uint16_t namespace_index);
const mu_node_t *mu_address_space_find_node(const mu_address_space_t *address_space, mu_address_space_index_t *index,
                                            const mu_nodeid_t *node_id);
/* Source compatibility: two-argument calls use the explicit NULL-index path. */
#define MU_ADDRESS_SPACE_FIND_NODE_2(address_space, node_id)                                                           \
    mu_address_space_find_node((address_space), ((mu_address_space_index_t *)0), (node_id))
#define MU_ADDRESS_SPACE_FIND_NODE_3(address_space, index, node_id)                                                    \
    mu_address_space_find_node((address_space), (index), (node_id))
#define MU_ADDRESS_SPACE_FIND_NODE_SELECT(_1, _2, _3, NAME, ...) NAME
#define mu_address_space_find_node(...)                                                                                \
    MU_ADDRESS_SPACE_FIND_NODE_SELECT(__VA_ARGS__, MU_ADDRESS_SPACE_FIND_NODE_3, MU_ADDRESS_SPACE_FIND_NODE_2)         \
    (__VA_ARGS__)

/* Value source operations */
opcua_statuscode_t mu_value_source_read(const mu_value_source_t *source, const mu_nodeid_t *node_id,
                                        mu_variant_t *value);

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_ADDRESS_SPACE_H */
