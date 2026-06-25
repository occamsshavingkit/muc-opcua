/* include/micro_opcua/address_space.h */
#ifndef MICRO_OPCUA_ADDRESS_SPACE_H
#define MICRO_OPCUA_ADDRESS_SPACE_H

#include "micro_opcua/types.h"
#include "micro_opcua/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MU_NODECLASS_OBJECT = 1,
    MU_NODECLASS_VARIABLE = 2
} mu_node_class_t;

typedef struct {
    mu_nodeid_t reference_type_id;
    mu_nodeid_t target_id;
    opcua_boolean_t is_forward;
} mu_reference_t;

typedef enum {
    MU_VALUESOURCE_STATIC = 0,
    MU_VALUESOURCE_CALLBACK = 1
} mu_value_source_type_t;

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
} mu_node_t;

typedef struct {
    const mu_node_t *nodes;
    size_t node_count;
} mu_address_space_t;

/* Address space validation */
opcua_statuscode_t mu_address_space_validate(const mu_address_space_t *address_space);

#ifdef __cplusplus
}
#endif

#endif /* MICRO_OPCUA_ADDRESS_SPACE_H */
