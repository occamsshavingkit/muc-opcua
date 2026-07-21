/* src/cu/core_2022_server/discovery/register_server.c
 * RegisterServer service handler (OPC-10000-4 §5.4.5). */
#include "core/dispatch_discovery/common.h"

#ifdef MUC_OPCUA_CU_DISCOVERY_REGISTER

opcua_statuscode_t handle_register_server(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                          size_t *response_length) {
    (void)server;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_string_t server_uri;
    s = mu_binary_read_string(r, &server_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_string_t product_uri;
    s = mu_binary_read_string(r, &product_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_int32_t i32;
    s = mu_binary_read_int32(r, &i32);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (i32 > 0) {
        for (opcua_int32_t i = 0; i < i32; ++i) {
            mu_string_t ignored;
            s = mu_binary_read_string(r, &ignored);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
    }

    for (int j = 0; j < 4; ++j) {
        s = mu_binary_read_int32(r, &i32);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (i32 > 0) {
            for (opcua_int32_t i = 0; i < i32; ++i) {
                mu_string_t ignored;
                s = mu_binary_read_string(r, &ignored);
                if (s != MU_STATUS_GOOD) {
                    return s;
                }
            }
        }
    }

    opcua_byte_t b;
    s = mu_binary_read_byte(r, &b);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = write_response_prefix(w, MU_ID_REGISTERSERVERRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

#endif /* MUC_OPCUA_CU_DISCOVERY_REGISTER */
