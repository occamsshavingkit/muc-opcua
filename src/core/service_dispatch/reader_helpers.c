#include "common.h"

opcua_statuscode_t ensure_reader_bytes_remaining(const mu_binary_reader_t *r, size_t length) {
    if (length > 0u) {
        if (r->position > r->length || length > (r->length - r->position)) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t skip_extension_object_body(mu_binary_reader_t *r, size_t length) {
    opcua_statuscode_t s = ensure_reader_bytes_remaining(r, length);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    r->position += length;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t ensure_array_items_min_remaining(const mu_binary_reader_t *r, opcua_int32_t count,
                                                    size_t min_item_size) {
    if (count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (count <= 0) {
        return MU_STATUS_GOOD;
    }
    if (r->position > r->length) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if ((size_t)count > (r->length - r->position) / min_item_size) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    return MU_STATUS_GOOD;
}

void mu_service_dispatch_set_opn_security_policy(mu_server_t *server, const mu_string_t *security_policy) {
    if (server == NULL) {
        return;
    }

    if (security_policy == NULL) {
        server->opn_pending_security_policy.length = -1;
        server->opn_pending_security_policy.data = NULL;
        return;
    }

    server->opn_pending_security_policy = *security_policy;
}

const mu_string_t *current_opn_security_policy(const mu_server_t *server) {
    if (server == NULL) {
        return NULL;
    }
    return &server->opn_pending_security_policy;
}

void mu_service_dispatch_set_opn_client_cert(mu_server_t *server, const mu_bytestring_t *client_cert) {
    if (server == NULL) {
        return;
    }

    if (client_cert == NULL) {
        server->opn_pending_client_cert.length = -1;
        server->opn_pending_client_cert.data = NULL;
        return;
    }

    server->opn_pending_client_cert = *client_cert;
#ifdef MUC_OPCUA_SECURITY
    if (client_cert->length > 0 && client_cert->data != NULL &&
        (size_t)client_cert->length <= sizeof(server->channel_client_cert)) {
        memcpy(server->channel_client_cert, client_cert->data, (size_t)client_cert->length);
        server->channel_client_cert_len = (size_t)client_cert->length;
    } else {
        server->channel_client_cert_len = 0;
    }
#endif
}

#ifdef MUC_OPCUA_SECURITY
const mu_bytestring_t *current_opn_client_cert(const mu_server_t *server) {
    if (server == NULL) {
        return NULL;
    }
    return &server->opn_pending_client_cert;
}
#endif
