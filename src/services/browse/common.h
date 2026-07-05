/* src/services/browse/common.h */
#ifndef MUC_OPCUA_SERVICES_BROWSE_COMMON_H
#define MUC_OPCUA_SERVICES_BROWSE_COMMON_H

#include "../../address_space/base_nodes.h"
#include "../../core/server_internal.h"
#include "../browse.h"
#include <stddef.h>
#include <string.h>

/* OPC-10000-4 §5.9.2.2 Table 34: resultMask bits */
#define BROWSE_RESULTMASK_REFERENCE_TYPE 0x01U
#define BROWSE_RESULTMASK_IS_FORWARD 0x02U
#define BROWSE_RESULTMASK_NODE_CLASS 0x04U
#define BROWSE_RESULTMASK_BROWSE_NAME 0x08U
#define BROWSE_RESULTMASK_DISPLAY_NAME 0x10U
#define BROWSE_RESULTMASK_TYPE_DEFINITION 0x20U

#endif /* MUC_OPCUA_SERVICES_BROWSE_COMMON_H */
