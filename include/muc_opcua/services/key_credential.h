/* include/muc_opcua/services/key_credential.h
 *
 * KeyCredential Service Server Facet (spec 094, PG18, CU 2113).
 * Credential management for OAuth2/OIDC broker authentication.
 * OPC-10000-12 §8.5-8.6, OPC-10000-7 v1.05.02.
 *
 * The facet exposes four Method nodes on the KeyCredentialConfigurationType
 * (17533) and KeyCredentialConfigurationFolderType (17496) ObjectType
 * InstanceDeclarations. Method dispatch rides the existing Method Server
 * Facet (spec 062): mu_server_init() calls mu_key_credential_register()
 * automatically when MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE is on, registering
 * the four method callbacks. The callbacks route to the integrator-provided
 * adapter; with no adapter each method returns Bad_NotSupported.
 *
 * The library never decrypts credential blobs -- it stores and retrieves
 * opaque ByteStrings. The GetEncryptingKey response is a DER-encoded public
 * key (typically the server's existing application-instance certificate key)
 * plus a keyId string.
 */
#ifndef MUC_OPCUA_SERVICES_KEY_CREDENTIAL_H
#define MUC_OPCUA_SERVICES_KEY_CREDENTIAL_H

#include "muc_opcua/config.h"
#include "muc_opcua/status.h"
#include "muc_opcua/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Method NodeIds in namespace 0 (OPC-10000-12 §8.5-8.6, NodeIds.csv). */
#define MU_ID_KEYCRED_GET_ENCRYPTING_KEY 17516u
#define MU_ID_KEYCRED_UPDATE_CREDENTIAL 17519u
#define MU_ID_KEYCRED_DELETE_CREDENTIAL 17521u
#define MU_ID_KEYCRED_CREATE_CREDENTIAL 17522u

/* ObjectType NodeIds (OPC-10000-12 §8.5-8.6). */
#define MU_ID_KEYCRED_CONFIG_FOLDER_TYPE 17496u
#define MU_ID_KEYCRED_CONFIG_TYPE 17533u

#ifdef MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE

struct mu_server;

/* Integrator-provided credential storage adapter. The library does not
 * implement persistent storage; the adapter is the bridge to whatever
 * backing store the application uses. All four operations are required --
 * a server configured with a NULL adapter returns Bad_NotSupported on every
 * KeyCredential method call.
 *
 * ResourceUri selects the per-service configuration (OAuth2 issuer, broker
 * endpoint, etc.). keyId is the credential identifier returned by
 * GetEncryptingKey and supplied on subsequent writes. The credential
 * ByteString is opaque (typically an encrypted PKCS#12 blob encrypted with
 * the public key returned by GetEncryptingKey). */
typedef struct {
    /* Return the DER-encoded public key (SubjectPublicKeyInfo) and its keyId
       for the given ResourceUri. MU_STATUS_BAD_NODATA when no key is
       configured for the resource. */
    opcua_statuscode_t (*get_encrypting_key)(void *ctx, const mu_string_t *resource_uri,
                                             mu_bytestring_t *out_public_key, mu_string_t *out_key_id);

    /* Store a new credential. MU_STATUS_BAD_ENTRYEXISTS or
       MU_STATUS_BAD_RESOURCEUNAVAILABLE when the store cannot accept it. */
    opcua_statuscode_t (*create_credential)(void *ctx, const mu_string_t *resource_uri,
                                            const mu_bytestring_t *credential, const mu_string_t *key_id);

    /* Replace an existing credential. MU_STATUS_BAD_NOENTRYEXISTS when the
       named credential is unknown. */
    opcua_statuscode_t (*update_credential)(void *ctx, const mu_string_t *resource_uri,
                                            const mu_bytestring_t *credential, const mu_string_t *key_id);

    /* Remove a credential. MU_STATUS_BAD_NOENTRYEXISTS when unknown. */
    opcua_statuscode_t (*delete_credential)(void *ctx, const mu_string_t *resource_uri, const mu_string_t *key_id);

    void *context; /* Integrator context pointer, delivered as `ctx`. */
} mu_key_credential_adapter_t;

/* Register the four KeyCredential method callbacks on the server. Called
 * automatically by mu_server_init() when MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE
 * is enabled. Exposed so tests and integrators can re-register or inspect.
 * Consumes 4 of the MU_MAX_REGISTERED_METHODS slots. */
opcua_statuscode_t mu_key_credential_register(struct mu_server *server);

#endif /* MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE */

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_SERVICES_KEY_CREDENTIAL_H */
