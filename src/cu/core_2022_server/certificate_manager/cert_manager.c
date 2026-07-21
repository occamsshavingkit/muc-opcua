/* src/cu/core_2022_server/certificate_manager/cert_manager.c
 *
 * Certificate Manager Pull Model (spec 097, OPC-10000-7 CU 1631,
 * OPC-10000-12 §7.6-7.9).
 *
 * Implements the four Pull Model certificate management Method handlers
 * (StartSigningRequest, FinishRequest, GetRejectedList,
 * StartNewKeyPairRequest) and registers them on the server via the
 * Method Server Facet (spec 062). Each handler validates input arguments,
 * dispatches to the integrator-provided mu_certificate_manager_adapter_t,
 * and encodes the Call response per OPC-10000-4 §5.11.
 * With no adapter configured every method returns Bad_NotSupported.
 *
 * Type-system InstanceDeclarations for the ObjectTypes live in
 * src/address_space/base_nodes.c.
 */
#include "core/server_internal.h"
#include "muc_opcua/address_space.h"
#include "muc_opcua/services/certificate_manager.h"
#include "muc_opcua/services/method.h"
#include <stddef.h>
#include <string.h>

#if MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL

/* Resolve the adapter from the server's config. Returns NULL when no
 * adapter was supplied, in which case every method returns Bad_NotSupported. */
static const mu_certificate_manager_adapter_t *adapter_of(const mu_server_t *server) {
    if (server == NULL) {
        return NULL;
    }
    return server->config.certificate_manager_adapter;
}

/* Build a mu_variant_t scalar ByteString from a mu_bytestring_t. */
static mu_variant_t bytestring_variant(mu_bytestring_t b) {
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_BYTESTRING;
    v.value.bytestr = b;
    return v;
}

/* Build a mu_variant_t scalar UInt32. */
static mu_variant_t uint32_variant(uint32_t n) {
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_UINT32;
    v.value.ui32 = n;
    return v;
}

/* Extract a ByteString input argument by index. */
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

/* Extract a UInt32 input argument by index. */
static opcua_statuscode_t read_uint32_arg(const mu_variant_t *args, size_t count, size_t index, uint32_t *out) {
    if (index >= count) {
        return MU_STATUS_BAD_ARGUMENTSMISSING;
    }
    if (args[index].is_array || args[index].type != MU_TYPE_UINT32) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    *out = args[index].value.ui32;
    return MU_STATUS_GOOD;
}

/* StartSigningRequest (OPC-10000-12 §7.9.6).
 * Input:  CSR:ByteString, certificateGroupId:NodeId
 * Output: requestId:UInt32 */
static opcua_statuscode_t handle_start_signing_request(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                                       const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                                       size_t input_args_count, mu_variant_t *output_args,
                                                       size_t *output_args_count) {
    (void)context;
    (void)method_id;
    (void)object_id;

    if (output_args_count != NULL) {
        *output_args_count = 0u;
    }

    const mu_certificate_manager_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL || adapter->start_signing_request == NULL) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    mu_bytestring_t csr;
    opcua_statuscode_t s = read_bytestring_arg(input_args, input_args_count, 0u, &csr);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (csr.length == 0 || csr.data == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }

    /* certificateGroupId is always CertificateGroups(15624) for now. */
    mu_nodeid_t group_id = {0u, MU_NODEID_NUMERIC, {MU_ID_CERTIFICATE_GROUPS_DIR}};
    uint32_t request_id = 0;
    s = adapter->start_signing_request(adapter->context, &csr, group_id, &request_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (output_args != NULL && output_args_count != NULL && *output_args_count == 0u) {
        output_args[0] = uint32_variant(request_id);
        *output_args_count = 1u;
    }
    return MU_STATUS_GOOD;
}

/* FinishRequest (OPC-10000-12 §7.9.9).
 * Input:  requestId:UInt32
 * Output: certificate:ByteString, privateKey:ByteString, issuerCertificates:ByteString[] */
static opcua_statuscode_t handle_finish_request(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                                const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                                size_t input_args_count, mu_variant_t *output_args,
                                                size_t *output_args_count) {
    (void)context;
    (void)method_id;
    (void)object_id;

    if (output_args_count != NULL) {
        *output_args_count = 0u;
    }

    const mu_certificate_manager_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL || adapter->finish_request == NULL) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    uint32_t request_id;
    opcua_statuscode_t s = read_uint32_arg(input_args, input_args_count, 0u, &request_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_bytestring_t cert = {0, NULL};
    mu_bytestring_t key = {0, NULL};
    mu_bytestring_t issuers = {0, NULL};
    s = adapter->finish_request(adapter->context, request_id, &cert, &key, &issuers);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (output_args != NULL && output_args_count != NULL && *output_args_count == 0u) {
        output_args[0] = bytestring_variant(cert);
        output_args[1] = bytestring_variant(key);
        output_args[2] = bytestring_variant(issuers);
        *output_args_count = 3u;
    }
    return MU_STATUS_GOOD;
}

/* GetRejectedList (OPC-10000-12 §7.9.10).
 * Input:  (none)
 * Output: certificates:ByteString[] */
static opcua_statuscode_t handle_get_rejected_list(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                                   const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                                   size_t input_args_count, mu_variant_t *output_args,
                                                   size_t *output_args_count) {
    (void)context;
    (void)method_id;
    (void)object_id;
    (void)input_args;
    (void)input_args_count;

    if (output_args_count != NULL) {
        *output_args_count = 0u;
    }

    const mu_certificate_manager_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL || adapter->get_rejected_list == NULL) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    /* Single output: the rejected list encoded as ByteString. */
    uint8_t buf[512];
    size_t buf_len = sizeof(buf);
    opcua_statuscode_t s = adapter->get_rejected_list(adapter->context, buf, &buf_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (output_args != NULL && output_args_count != NULL && *output_args_count == 0u && buf_len > 0) {
        mu_bytestring_t b = {(int)buf_len, buf};
        output_args[0] = bytestring_variant(b);
        *output_args_count = 1u;
    }
    return MU_STATUS_GOOD;
}

/* StartNewKeyPairRequest (OPC-10000-12 §7.9.7).
 * Input:  keySpec:ByteString, certificateGroupId:NodeId
 * Output: requestId:UInt32 */
static opcua_statuscode_t handle_start_new_key_pair_request(mu_server_t *server, void *context,
                                                            const mu_nodeid_t *object_id, const mu_nodeid_t *method_id,
                                                            const mu_variant_t *input_args, size_t input_args_count,
                                                            mu_variant_t *output_args, size_t *output_args_count) {
    (void)context;
    (void)method_id;
    (void)object_id;

    if (output_args_count != NULL) {
        *output_args_count = 0u;
    }

    const mu_certificate_manager_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL || adapter->start_new_key_pair == NULL) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    mu_bytestring_t key_spec;
    opcua_statuscode_t s = read_bytestring_arg(input_args, input_args_count, 0u, &key_spec);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (key_spec.length == 0 || key_spec.data == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }

    mu_nodeid_t group_id = {0u, MU_NODEID_NUMERIC, {MU_ID_CERTIFICATE_GROUPS_DIR}};
    uint32_t request_id = 0;
    s = adapter->start_new_key_pair(adapter->context, &key_spec, group_id, &request_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (output_args != NULL && output_args_count != NULL && *output_args_count == 0u) {
        output_args[0] = uint32_variant(request_id);
        *output_args_count = 1u;
    }
    return MU_STATUS_GOOD;
}

/* Register all four Pull Model certificate management Method callbacks.
 * Called automatically from mu_server_init() when the CU is enabled.
 * Consumes 4 of the MU_MAX_REGISTERED_METHODS slots. */
opcua_statuscode_t mu_certificate_manager_register(mu_server_t *server) {
    if (server == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    static const mu_nodeid_t start_signing_id = {0u, MU_NODEID_NUMERIC, {MU_ID_CM_START_SIGNING_REQUEST}};
    static const mu_nodeid_t start_new_key_id = {0u, MU_NODEID_NUMERIC, {MU_ID_CM_START_NEW_KEY_PAIR_REQUEST}};
    static const mu_nodeid_t finish_id = {0u, MU_NODEID_NUMERIC, {MU_ID_CM_FINISH_REQUEST}};
    static const mu_nodeid_t get_rejected_id = {0u, MU_NODEID_NUMERIC, {MU_ID_CM_GET_REJECTED_LIST}};

    opcua_statuscode_t s;
    s = mu_server_register_method_callback(server, &start_signing_id, handle_start_signing_request, NULL, NULL, 0, NULL,
                                           0, true);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_server_register_method_callback(server, &start_new_key_id, handle_start_new_key_pair_request, NULL, NULL, 0,
                                           NULL, 0, true);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_server_register_method_callback(server, &finish_id, handle_finish_request, NULL, NULL, 0, NULL, 0, true);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_server_register_method_callback(server, &get_rejected_id, handle_get_rejected_list, NULL, NULL, 0, NULL, 0,
                                           true);
    return s;
}

#endif /* MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL */
