/* include/muc_opcua/services/method.h
 *
 * Method Server Facet: arbitrary method callback registration API.
 * OPC-10000-4 §5.12.2.2 (Call service).
 */
#ifndef MUC_OPCUA_SERVICES_METHOD_H
#define MUC_OPCUA_SERVICES_METHOD_H

#include "muc_opcua/address_space.h"
#include "muc_opcua/config.h"
#include "muc_opcua/status.h"
#include "muc_opcua/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MUC_OPCUA_METHOD_SERVER

struct mu_server;

/* Argument metadata (OPC-10000-5 §12.2.12.1, DataType i=296). An integrator
   declares a method's input/output signature with an array of these; the server
   uses it to validate incoming CallMethodRequest arguments and to serve the
   InputArguments/OutputArguments Property Value so clients can browse the
   signature. arrayDimensions is not modelled — value_rank (-1 scalar, >=1 array
   rank, per OPC-10000-3) is the validated dimension. */
typedef struct mu_argument {
    mu_string_t name;
    mu_nodeid_t data_type;
    opcua_int32_t value_rank;
    mu_localized_text_t description;
} mu_argument_t;

#endif /* MUC_OPCUA_METHOD_SERVER */

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_SERVICES_METHOD_H */
