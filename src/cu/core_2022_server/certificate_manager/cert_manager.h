/* Internal Certificate Manager Pull Model module header (spec 097, CU 1631).
 * OPC-10000-12 §7.6-7.9.
 *
 * The Pull Model Method handlers (StartSigningRequest, FinishRequest,
 * GetRejectedList, StartNewKeyPairRequest) are file-local (static) in
 * cert_manager.c and wired into the Method Server Facet through the single
 * public registration entry point declared below. The public adapter struct,
 * NodeId constants, and the register function live in the public header. */
#ifndef MUC_OPCUA_INTERNAL_CERT_MANAGER_H
#define MUC_OPCUA_INTERNAL_CERT_MANAGER_H

#include "muc_opcua/config.h"
#include "muc_opcua/services/certificate_manager.h"

#ifdef MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL

struct mu_server;

/* Register all four Pull Model certificate management Method callbacks on the
 * server. Called automatically from mu_server_init() when the CU is enabled.
 * Consumes 4 of the MU_MAX_REGISTERED_METHODS slots. Declared here for
 * internal callers; the canonical declaration is in the public header. */
opcua_statuscode_t mu_certificate_manager_register(struct mu_server *server);

#endif /* MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL */

#endif /* MUC_OPCUA_INTERNAL_CERT_MANAGER_H */
