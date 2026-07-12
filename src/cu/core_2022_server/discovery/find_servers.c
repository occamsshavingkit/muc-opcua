/* src/core/dispatch_discovery/find_servers.c
 * FindServers service handler (OPC-10000-4 5.5.2.2). */
#include "core/dispatch_discovery/common.h"

#ifdef MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS

static bool findservers_endpoint_matches(const mu_string_t *endpoint_url, const char *discovery_url) {
    if (endpoint_url == NULL || endpoint_url->length <= 0) {
        return true;
    }

    return string_matches_cstr(endpoint_url, discovery_url);
}

static opcua_statuscode_t findservers_server_uri_filter_matches(mu_binary_reader_t *r, const char *application_uri,
                                                                bool *matches) {
    opcua_int32_t count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (count <= 0) {
        *matches = true;
        return MU_STATUS_GOOD;
    }

    *matches = false;
    for (opcua_int32_t i = 0; i < count; ++i) {
        mu_string_t server_uri;
        s = mu_binary_read_string(r, &server_uri);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (string_matches_cstr(&server_uri, application_uri)) {
            *matches = true;
        }
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t
findservers_server_type_filter_matches(mu_binary_reader_t *r, mu_application_type_t application_type, bool *matches) {
    if (r->position == r->length) {
        *matches = true;
        return MU_STATUS_GOOD;
    }
    if (r->position > r->length) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    opcua_int32_t count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (count <= 0) {
        *matches = true;
        return MU_STATUS_GOOD;
    }

    *matches = false;
    for (opcua_int32_t i = 0; i < count; ++i) {
        opcua_uint32_t server_type;
        s = mu_binary_read_uint32(r, &server_type);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (server_type == (opcua_uint32_t)application_type) {
            *matches = true;
        }
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t handle_find_servers(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                       size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_string_t endpoint_url;
    s = mu_binary_read_string(r, &endpoint_url);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_string_t requested_locale;
    s = read_requested_locale_ids(r, &requested_locale);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_application_description_t app;
    s = mu_discovery_get_application_description(&server->config, &app);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    bool server_uri_matches = false;
    s = findservers_server_uri_filter_matches(r, app.application_uri, &server_uri_matches);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    bool server_type_matches = false;
    s = findservers_server_type_filter_matches(r, app.application_type, &server_type_matches);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    bool include_app =
        findservers_endpoint_matches(&endpoint_url, app.discovery_url) && server_uri_matches && server_type_matches;

    s = write_response_prefix(w, MU_ID_FINDSERVERSRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_CU_TIME_SYNC
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int32(w, include_app ? 1 : 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (include_app) {
        s = findservers_application_description_encode(w, &app, &requested_locale);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

#endif /* MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS */
