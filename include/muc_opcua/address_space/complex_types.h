/* include/muc_opcua/address_space/complex_types.h
 *
 * ComplexType Server Facet: custom Structure and Enumeration DataType
 * registration. OPC-10000-3 §5.6.4 (StructureDefinition), §5.6.5 (EnumDefinition).
 */
#ifndef MUC_OPCUA_ADDRESS_SPACE_COMPLEX_TYPES_H
#define MUC_OPCUA_ADDRESS_SPACE_COMPLEX_TYPES_H

#include "muc_opcua/address_space.h"
#include "muc_opcua/config.h"
#include "muc_opcua/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MUC_OPCUA_COMPLEX_TYPES

#define MU_STRUCTURE_TYPE_STRUCTURE 0
#define MU_STRUCTURE_TYPE_OPTIONAL 1

typedef struct {
    const char *name;
    mu_localized_text_t description;
    mu_nodeid_t data_type;
    opcua_int32_t value_rank;
    opcua_uint32_t *array_dimensions;
    opcua_uint32_t max_string_length;
    opcua_boolean_t is_optional;
} mu_structure_field_t;

typedef struct {
    mu_nodeid_t default_encoding_id;
    mu_nodeid_t base_data_type;
    opcua_uint32_t structure_type;
    const mu_structure_field_t *fields;
    opcua_uint16_t field_count;
} mu_structure_definition_t;

typedef struct {
    opcua_int64_t value;
    mu_localized_text_t display_name;
    mu_localized_text_t description;
    const char *name;
} mu_enum_field_t;

typedef struct {
    const mu_enum_field_t *fields;
    opcua_uint16_t field_count;
} mu_enum_definition_t;

struct mu_server;
opcua_statuscode_t mu_register_structure_type(struct mu_server *server, const mu_nodeid_t *type_id,
                                              const mu_structure_definition_t *def);
opcua_statuscode_t mu_register_enumeration_type(struct mu_server *server, const mu_nodeid_t *type_id,
                                                const mu_enum_definition_t *def);

#endif /* MUC_OPCUA_COMPLEX_TYPES */

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_ADDRESS_SPACE_COMPLEX_TYPES_H */
