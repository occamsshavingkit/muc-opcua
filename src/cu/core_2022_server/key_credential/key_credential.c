/* src/cu/core_2022_server/key_credential/key_credential.c
 *
 * KeyCredential Service Server Facet (spec 094, OPC-10000-7 CU 2113,
 * OPC-10000-12 §8.5-8.6).
 *
 * Implements the four KeyCredential method handlers and registers them on
 * the server via the Method Server Facet (spec 062). Each handler extracts
 * the Call input arguments, dispatches to the integrator-provided
 * mu_key_credential_adapter_t, and encodes the Call response per
 * OPC-10000-4 §5.11. With no adapter configured every method returns
 * Bad_NotSupported (FR-004).
 *
 * The library treats credentials as opaque ByteStrings -- it never decrypts
 * (FR-008). GetEncryptingKey returns a DER-encoded public key plus keyId
 * (FR-007); the default backend simply forwards to the adapter.
 */
#include "muc_opcua/services/key_credential.h"
#include "core/server_internal.h"
#include "muc_opcua/address_space.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/services/method.h"
#include <stddef.h>
#include <string.h>

#if MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE

/* Pull the typedefs we need from server_internal.h without depending on the
 * whole internal surface in the test. The handler signature matches
 * mu_method_callback_t (address_space.h). */
typedef struct mu_server mu_server_t;

/* Resolve the adapter from the server's config. Returns NULL when no
 * adapter was supplied, in which case every method returns Bad_NotSupported. */
static const mu_key_credential_adapter_t *adapter_of(const mu_server_t *server) {
    if (server == NULL) {
        return NULL;
    }
    return server->config.key_credential_adapter;
}

/* Build a mu_variant_t scalar String from a mu_string_t. */
static mu_variant_t string_variant(mu_string_t s) {
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_STRING;
    v.value.str = s;
    return v;
}

/* Build a mu_variant_t scalar ByteString from a mu_bytestring_t. */
static mu_variant_t bytestring_variant(mu_bytestring_t b) {
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_BYTESTRING;
    v.value.bytestr = b;
    return v;
}

/* Extract a String input argument by index. Returns Bad_InvalidArgument
 * when the index is out of range or the argument is not a scalar String. */
static opcua_statuscode_t read_string_arg(const mu_variant_t *args, size_t count, size_t index, mu_string_t *out) {
    if (index >= count) {
        return MU_STATUS_BAD_ARGUMENTSMISSING;
    }
    if (args[index].is_array || args[index].type != MU_TYPE_STRING) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    *out = args[index].value.str;
    return MU_STATUS_GOOD;
}

/* Extract a ByteString input argument by index. Returns Bad_InvalidArgument
 * when the index is out of range or the argument is not a scalar ByteString. */
static opcua_statuscode_t read_bytestring_arg(const mu_variant_t *args, size_t count, size_t index,
                                              mu_bytestring_t *out) {
    if (index >= count) {
        return MU_STATUS_BAD_ARGUMENTSMISSING;
    }
    if (args[index].is_array || args[index].type != MU_TYPE_BYTESTRING) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    *out = args[index].value.bytestr;
    return MU_STATUS_GOOD;
}

/* GetEncryptingKey (OPC-10000-12 §8.5.4 Table 142).
 * Input:  ResourceUri:String
 * Output: EncryptingKey:ByteString, KeyId:String
 * StatusCode mirrors the adapter (Bad_NoData when no key configured). */
static opcua_statuscode_t handle_get_encrypting_key(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                                    const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                                    size_t input_args_count, mu_variant_t *output_args,
                                                    size_t *output_args_count) {
    (void)context;
    (void)object_id;
    (void)method_id;

    if (output_args_count == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    /* Reserve both output slots even on the failure path so the dispatch
     * layer never reads an uninitialised output_args_count. */
    *output_args_count = 0u;

    const mu_key_credential_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL || adapter->get_encrypting_key == NULL) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    mu_string_t resource_uri;
    opcua_statuscode_t s = read_string_arg(input_args, input_args_count, 0u, &resource_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_bytestring_t public_key = {0, NULL};
    mu_string_t key_id = {0, NULL};
    s = adapter->get_encrypting_key(adapter->context, &resource_uri, &public_key, &key_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (*output_args_count == 0u && output_args != NULL) {
        /* Two outputs: EncryptingKey (ByteString), KeyId (String). */
        output_args[0] = bytestring_variant(public_key);
        output_args[1] = string_variant(key_id);
        *output_args_count = 2u;
    }
    return MU_STATUS_GOOD;
}

/* CreateCredential (OPC-10000-12 §8.6).
 * Input:  ResourceUri:String, Credential:ByteString, KeyId:String
 * Output: none */
static opcua_statuscode_t handle_create_credential(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                                   const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                                   size_t input_args_count, mu_variant_t *output_args,
                                                   size_t *output_args_count) {
    (void)context;
    (void)object_id;
    (void)method_id;
    (void)output_args;

    if (output_args_count != NULL) {
        *output_args_count = 0u;
    }

    const mu_key_credential_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL || adapter->create_credential == NULL) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    mu_string_t resource_uri;
    mu_bytestring_t credential;
    mu_string_t key_id;
    opcua_statuscode_t s = read_string_arg(input_args, input_args_count, 0u, &resource_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = read_bytestring_arg(input_args, input_args_count, 1u, &credential);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = read_string_arg(input_args, input_args_count, 2u, &key_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    return adapter->create_credential(adapter->context, &resource_uri, &credential, &key_id);
}

/* UpdateCredential (OPC-10000-12 §8.6).
 * Input:  ResourceUri:String, Credential:ByteString, KeyId:String
 * Output: none */
static opcua_statuscode_t handle_update_credential(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                                   const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                                   size_t input_args_count, mu_variant_t *output_args,
                                                   size_t *output_args_count) {
    (void)context;
    (void)object_id;
    (void)method_id;
    (void)output_args;

    if (output_args_count != NULL) {
        *output_args_count = 0u;
    }

    const mu_key_credential_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL || adapter->update_credential == NULL) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    mu_string_t resource_uri;
    mu_bytestring_t credential;
    mu_string_t key_id;
    opcua_statuscode_t s = read_string_arg(input_args, input_args_count, 0u, &resource_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = read_bytestring_arg(input_args, input_args_count, 1u, &credential);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = read_string_arg(input_args, input_args_count, 2u, &key_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    return adapter->update_credential(adapter->context, &resource_uri, &credential, &key_id);
}

/* DeleteCredential (OPC-10000-12 §8.6).
 * Input:  ResourceUri:String, KeyId:String
 * Output: none */
static opcua_statuscode_t handle_delete_credential(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                                   const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                                   size_t input_args_count, mu_variant_t *output_args,
                                                   size_t *output_args_count) {
    (void)context;
    (void)object_id;
    (void)method_id;
    (void)output_args;

    if (output_args_count != NULL) {
        *output_args_count = 0u;
    }

    const mu_key_credential_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL || adapter->delete_credential == NULL) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    mu_string_t resource_uri;
    mu_string_t key_id;
    opcua_statuscode_t s = read_string_arg(input_args, input_args_count, 0u, &resource_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = read_string_arg(input_args, input_args_count, 1u, &key_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    return adapter->delete_credential(adapter->context, &resource_uri, &key_id);
}

/* Register all four KeyCredential method callbacks on the server. Called
 * automatically from mu_server_init() when the CU is enabled. Consumes 4 of
 * the MU_MAX_REGISTERED_METHODS slots. Idempotent: re-registering an
 * existing method_id overwrites in place (the underlying API behaviour). */
opcua_statuscode_t mu_key_credential_register(mu_server_t *server) {
    if (server == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    static const mu_nodeid_t get_encrypting_key_id = {0u, MU_NODEID_NUMERIC, {MU_ID_KEYCRED_GET_ENCRYPTING_KEY}};
    static const mu_nodeid_t create_credential_id = {0u, MU_NODEID_NUMERIC, {MU_ID_KEYCRED_CREATE_CREDENTIAL}};
    static const mu_nodeid_t update_credential_id = {0u, MU_NODEID_NUMERIC, {MU_ID_KEYCRED_UPDATE_CREDENTIAL}};
    static const mu_nodeid_t delete_credential_id = {0u, MU_NODEID_NUMERIC, {MU_ID_KEYCRED_DELETE_CREDENTIAL}};

    opcua_statuscode_t s;
    s = mu_server_register_method_callback(server, &get_encrypting_key_id, handle_get_encrypting_key, NULL, NULL, 0,
                                           NULL, 0, true);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_server_register_method_callback(server, &create_credential_id, handle_create_credential, NULL, NULL, 0, NULL,
                                           0, true);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_server_register_method_callback(server, &update_credential_id, handle_update_credential, NULL, NULL, 0, NULL,
                                           0, true);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_server_register_method_callback(server, &delete_credential_id, handle_delete_credential, NULL, NULL, 0, NULL,
                                           0, true);
    return s;
}

#endif /* MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE */
