/* src/cu/core_2022_server/certificate_manager/cert_manager.h
 *
 * Registration declaration for internal mu_certificate_manager_register.
 */

#ifndef MUC_OPCUA_CERT_MANAGER_H
#define MUC_OPCUA_CERT_MANAGER_H

#include "muc_opcua/status.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mu_server;

opcua_statuscode_t mu_certificate_manager_register(struct mu_server *server);

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_CERT_MANAGER_H */
