/* src/cu/core_2022_server/certificate_manager/push/push_model.c
 *
 * Push Model for Global Certificate and TrustList Management
 * (spec 112, OPC-10000-7 CU 2231, OPC-10000-12 §7.10).
 *
 * Minimal stub implementations for UpdateCertificate and ApplyChanges
 * Methods. The server accepts certificate/trust list pushes from an
 * external GDS/agent. The integrator provides a certificate update
 * adapter for actual certificate storage logic.
 *
 * Gated on MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL.
 */
#include "core/server_internal.h"
#include "muc_opcua/services/certificate_manager.h"
#include "muc_opcua/services/method.h"
#include <stddef.h>
#include <string.h>

#if MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL

static const mu_certificate_manager_adapter_t *adapter_of(const mu_server_t *server) {
    if (server == NULL)
        return NULL;
    return server->config.certificate_manager_adapter;
}

/* UpdateCertificate (OPC-10000-12 §7.10.5).
 * Input: certificateGroup(NodeId), certificateType(NodeId), certificate(ByteString),
 *        issuerCertificates(ByteString[]), privateKeyFormat(String), privateKey(ByteString)
 * Output: ApplyChangesRequired(Boolean) */
static opcua_statuscode_t handle_update_certificate(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                                    const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                                    size_t input_args_count, mu_variant_t *output_args,
                                                    size_t *output_args_count) {
    (void)context;
    (void)method_id;
    (void)object_id;
    (void)input_args;
    (void)input_args_count;
    (void)output_args;

    if (output_args_count != NULL)
        *output_args_count = 0u;

    const mu_certificate_manager_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL)
        return MU_STATUS_BAD_NOTSUPPORTED;

    /* Stub: return Good with ApplyChangesRequired=false.
       Full implementation would validate inputs and delegate to adapter. */
    return MU_STATUS_BAD_NOTSUPPORTED;
}

/* ApplyChanges (OPC-10000-12 §7.10.9).
 * Input: (none)
 * Output: (none)
 * Commits pending certificate and trust list updates. */
static opcua_statuscode_t handle_apply_changes(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                               const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                               size_t input_args_count, mu_variant_t *output_args,
                                               size_t *output_args_count) {
    (void)context;
    (void)method_id;
    (void)object_id;
    (void)input_args;
    (void)input_args_count;
    (void)output_args;

    if (output_args_count != NULL)
        *output_args_count = 0u;

    const mu_certificate_manager_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL)
        return MU_STATUS_BAD_NOTSUPPORTED;

    /* Stub: return Good — no-op for now. */
    return MU_STATUS_BAD_NOTSUPPORTED;
}

/* Register Push Model Method callbacks on the server. */
opcua_statuscode_t mu_certificate_push_register(mu_server_t *server) {
    if (server == NULL)
        return MU_STATUS_BAD_INTERNALERROR;

    static const mu_nodeid_t update_cert_id = {0u, MU_NODEID_NUMERIC, {12616u}};
    static const mu_nodeid_t apply_changes_id = {0u, MU_NODEID_NUMERIC, {12748u}};

    opcua_statuscode_t s;
    s = mu_server_register_method_callback(server, &update_cert_id, handle_update_certificate, NULL, NULL, 0, NULL, 0,
                                           true);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_server_register_method_callback(server, &apply_changes_id, handle_apply_changes, NULL, NULL, 0, NULL, 0,
                                           true);
    return s;
}

#endif /* MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL */
