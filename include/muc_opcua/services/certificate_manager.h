/* include/muc_opcua/services/certificate_manager.h
 *
 * Certificate Manager Pull Model (spec 097, D-GDS, CU 1631).
 * Full Certificate Pull Management per OPC-10000-12 §7.6-7.9.
 * Type-system InstanceDeclarations for CertificateDirectoryType,
 * CertificateGroupType, CertificateType + subtypes, plus Method
 * handlers for the Pull workflow (StartSigningRequest, FinishRequest,
 * GetRejectedList, StartNewKeyPairRequest).
 * OPC-10000-12 §7.6-7.9, OPC-10000-7 v1.05.02.
 *
 * Requires MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT (type nodes).
 * Method dispatch rides the existing Method Server Facet (spec 062).
 */
#ifndef MUC_OPCUA_SERVICES_CERTIFICATE_MANAGER_H
#define MUC_OPCUA_SERVICES_CERTIFICATE_MANAGER_H

#include "muc_opcua/config.h"
#include "muc_opcua/status.h"
#include "muc_opcua/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ObjectType and Instance NodeIds in namespace 0 (OPC-10000-12 §7.8-7.9). */
#define MU_ID_CERTIFICATE_DIRECTORY_TYPE 15594u
#define MU_ID_CERTIFICATE_GROUPS_DIR 15624u
#define MU_ID_DEFAULT_APPLICATION_GROUP 15625u
#define MU_ID_DEFAULT_HTTPS_GROUP 15626u
#define MU_ID_DEFAULT_USER_TOKEN_GROUP 15627u
#define MU_ID_APPLICATION_CERTIFICATE_TYPE 12557u
#define MU_ID_HTTPS_CERTIFICATE_TYPE 12558u
#define MU_ID_USER_CERTIFICATE_TYPE 15017u
#define MU_ID_RSA_SHA256_CERTIFICATE_TYPE 12559u
#define MU_ID_RSA_MIN_CERTIFICATE_TYPE 15421u

/* Method NodeIds (OPC-10000-12 §7.9). */
#define MU_ID_CM_START_SIGNING_REQUEST 12482u
#define MU_ID_CM_START_NEW_KEY_PAIR_REQUEST 12483u
#define MU_ID_CM_FINISH_REQUEST 12484u
#define MU_ID_CM_GET_REJECTED_LIST 12747u

#ifdef MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL

struct mu_server;

/* Integrator-provided certificate management adapter. The library does not
 * implement signing, key generation, or certificate storage -- the adapter
 * is the bridge to the application's certificate lifecycle logic.
 *
 * A NULL adapter is valid: type-system nodes appear in the address space
 * but Method calls return Bad_NotSupported. */
typedef struct {
    /* Submit a CSR and receive a polling requestId.
       Returns MU_STATUS_GOOD with *out_request_id populated. */
    opcua_statuscode_t (*start_signing_request)(void *ctx, const mu_bytestring_t *csr, mu_nodeid_t group_id,
                                                uint32_t *out_request_id);

    /* Retrieve the completed certificate, private key, and issuer chain
       for a previously submitted requestId.
       Returns MU_STATUS_GOOD with buffers filled, or MU_STATUS_BAD_NOTFOUND. */
    opcua_statuscode_t (*finish_request)(void *ctx, uint32_t request_id, mu_bytestring_t *out_cert,
                                         mu_bytestring_t *out_key, mu_bytestring_t *out_issuers);

    /* Query rejected certificate requests.
       Returns MU_STATUS_GOOD; out_buffer written with encoded list. */
    opcua_statuscode_t (*get_rejected_list)(void *ctx, uint8_t *out_buffer, size_t *out_len);

    /* Request the CertificateManager to generate a new asymmetric key pair.
       Returns MU_STATUS_GOOD with *out_request_id populated. */
    opcua_statuscode_t (*start_new_key_pair)(void *ctx, const mu_bytestring_t *key_spec, mu_nodeid_t group_id,
                                             uint32_t *out_request_id);

    void *context;
} mu_certificate_manager_adapter_t;

/* Register the Pull Model Method callbacks on the server. Called
 * automatically by mu_server_init() when MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL
 * is enabled. */
opcua_statuscode_t mu_certificate_manager_register(struct mu_server *server);

#endif /* MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL */

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_SERVICES_CERTIFICATE_MANAGER_H */
