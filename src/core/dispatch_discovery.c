/* src/core/dispatch_discovery.c
 * Discovery service handlers extracted from service_dispatch.c (T009).
 * Implements GetEndpoints (OPC-10000-4 5.5.4.2) and FindServers (5.5.2.2).
 * Discovery services compile under MUC_OPCUA_SERVICE_DISCOVERY. */
#include "../services/discovery.h"
#include "../services/service_header.h"
#include "muc_opcua/address_space.h"
#include "muc_opcua/encoding.h"
#include "server_internal.h"
#include "service_dispatch.h"
#include <stddef.h>
#include <string.h>

#ifdef MUC_OPCUA_SERVICE_DISCOVERY
static bool string_matches_cstr(const mu_string_t *left, const char *right) {
    if (left == NULL || right == NULL || left->length < 0 || left->data == NULL) {
        return false;
    }
    size_t right_len = strlen(right);
    return (size_t)left->length == right_len && memcmp(left->data, right, right_len) == 0;
}

static bool findservers_endpoint_matches(const mu_string_t *endpoint_url, const char *discovery_url) {
    if (endpoint_url == NULL || endpoint_url->length <= 0) {
        return true;
    }

    /* OPC-10000-4 section 5.5.2.2: endpointUrl identifies the DiscoveryEndpoint
       the Client used; only matching server descriptions are returned. */
    return string_matches_cstr(endpoint_url, discovery_url);
}

static bool getendpoints_endpoint_matches(const mu_string_t *endpoint_url, const mu_endpoint_description_t *endpoint) {
    if (endpoint_url == NULL || endpoint_url->length <= 0) {
        return true;
    }

    /* OPC-10000-4 section 5.5.4.2: endpointUrl selects the session endpoint URL
       descriptions returned by GetEndpoints. */
    return endpoint != NULL && string_matches_cstr(endpoint_url, endpoint->endpoint_url);
}

static bool locale_id_is_usable(const mu_string_t *locale_id) {
    return locale_id != NULL && locale_id->length > 0 && locale_id->data != NULL;
}

static opcua_statuscode_t read_requested_locale_ids(mu_binary_reader_t *r, mu_string_t *selected_locale) {
    opcua_int32_t count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    /* OPC-10000-6 section 5.2.5: -1 encodes a null array; lower values are malformed. */
    if (count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    *selected_locale = (mu_string_t){-1, NULL};
    if (count <= 0) {
        return MU_STATUS_GOOD;
    }

    for (opcua_int32_t i = 0; i < count; ++i) {
        mu_string_t locale_id;
        s = mu_binary_read_string(r, &locale_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (!locale_id_is_usable(selected_locale) && locale_id_is_usable(&locale_id)) {
            *selected_locale = locale_id;
        }
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t findservers_server_uri_filter_matches(mu_binary_reader_t *r, const char *application_uri,
                                                                bool *matches) {
    opcua_int32_t count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    /* OPC-10000-6 section 5.2.5: -1 encodes a null array; lower values are malformed. */
    if (count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    /* OPC-10000-4 section 5.5.2.2: empty serverUris[] returns all known Servers. */
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
    /* OPC-10000-6 section 5.2.5: -1 encodes a null array; lower values are malformed. */
    if (count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    /* OPC-10000-4 section 5.5.2.2: empty serverTypes[] returns all known Servers. */
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

static opcua_statuscode_t findservers_write_cstr(mu_binary_writer_t *w, const char *value) {
    if (value == NULL) {
        mu_string_t null_str = {-1, NULL};
        return mu_binary_write_string(w, &null_str);
    }
    mu_string_t str = {(opcua_int32_t)strlen(value), (const opcua_byte_t *)value};
    return mu_binary_write_string(w, &str);
}

static opcua_statuscode_t findservers_write_localizedtext(mu_binary_writer_t *w, const char *text,
                                                          const mu_string_t *requested_locale) {
    if (text == NULL) {
        return mu_binary_write_byte(w, 0x00u);
    }

    const bool include_locale = locale_id_is_usable(requested_locale);
    opcua_statuscode_t s = mu_binary_write_byte(w, include_locale ? 0x03u : 0x02u);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (include_locale) {
        s = mu_binary_write_string(w, requested_locale);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
    return findservers_write_cstr(w, text);
}

static opcua_statuscode_t findservers_application_description_encode(mu_binary_writer_t *w,
                                                                     const mu_application_description_t *desc,
                                                                     const mu_string_t *requested_locale) {
    if (!locale_id_is_usable(requested_locale)) {
        return mu_application_description_encode(w, desc);
    }

    opcua_statuscode_t s;
    s = findservers_write_cstr(w, desc->application_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = findservers_write_cstr(w, desc->product_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    /* OPC-10000-4 sections 5.5.2.2 and 5.4: FindServers localeIds[] select
       the locale used for ApplicationDescription.applicationName. */
    s = findservers_write_localizedtext(w, desc->application_name, requested_locale);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_uint32(w, (opcua_uint32_t)desc->application_type);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = findservers_write_cstr(w, NULL);
    if (s != MU_STATUS_GOOD) {
        return s; /* gatewayServerUri */
    }
    s = findservers_write_cstr(w, NULL);
    if (s != MU_STATUS_GOOD) {
        return s; /* discoveryProfileUri */
    }

    if (desc->discovery_url != NULL) {
        s = mu_binary_write_int32(w, 1);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = findservers_write_cstr(w, desc->discovery_url);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    } else {
        s = mu_binary_write_int32(w, 0);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t endpoint_matches_profile_filter(mu_binary_reader_t *r,
                                                          const mu_endpoint_description_t *endpoint, bool *matches) {
    opcua_int32_t profile_count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &profile_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (profile_count <= 0) {
        *matches = true;
        return MU_STATUS_GOOD;
    }

    *matches = false;
    for (opcua_int32_t i = 0; i < profile_count; ++i) {
        mu_string_t profile_uri;
        s = mu_binary_read_string(r, &profile_uri);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (string_matches_cstr(&profile_uri, endpoint->transport_profile_uri)) {
            *matches = true;
        }
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t getendpoints_endpoint_description_encode(mu_binary_writer_t *w,
                                                                   const mu_endpoint_description_t *desc,
                                                                   const mu_string_t *requested_locale) {
    if (!locale_id_is_usable(requested_locale)) {
        return mu_endpoint_description_encode(w, desc);
    }

    opcua_statuscode_t s = findservers_write_cstr(w, desc->endpoint_url);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    /* OPC-10000-4 sections 5.5.4.2 and 5.4: GetEndpoints localeIds[]
       select the locale used for EndpointDescription.server.applicationName. */
    s = findservers_application_description_encode(w, &desc->server, requested_locale);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    { /* serverCertificate (ByteString; null when the server has no certificate) */
        mu_bytestring_t cert = {desc->server_certificate ? (opcua_int32_t)desc->server_certificate_len : -1,
                                desc->server_certificate};
        s = mu_binary_write_bytestring(w, &cert);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
    s = mu_binary_write_uint32(w, (opcua_uint32_t)desc->security_mode);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = findservers_write_cstr(w, desc->security_policy_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_write_int32(w, (opcua_int32_t)desc->num_user_identity_tokens);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (size_t i = 0; i < desc->num_user_identity_tokens; ++i) {
        const mu_user_token_policy_t *t = &desc->user_identity_tokens[i];
        s = findservers_write_cstr(w, t->policy_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = mu_binary_write_uint32(w, (opcua_uint32_t)t->token_type);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = findservers_write_cstr(w, t->issued_token_type);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = findservers_write_cstr(w, t->issuer_endpoint_url);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = findservers_write_cstr(w, t->security_policy_uri);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    s = findservers_write_cstr(w, desc->transport_profile_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    return mu_binary_write_byte(w, desc->security_level);
}

/* Bound for the cached profileUris[] filter in handle_get_endpoints. OPC-10000-7
   defines a small set of transport profiles; 8 covers all realistic client
   filters with margin. Larger wire lists fall back to per-endpoint re-parsing. */
#define MU_GETENDPOINTS_PROFILE_CACHE_MAX 8

/* GetEndpoints (OPC-10000-4 section 5.5.4.2): return endpoints matching the requested
   endpointUrl and transport profile URI filters, with localeIds[] selecting
   localized ApplicationDescription text per OPC-10000-4 section 5.4. */
opcua_statuscode_t handle_get_endpoints(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
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
    s = read_requested_locale_ids(r, &requested_locale); /* localeIds[] */
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    /* OPC-10000-4 section 5.5.4.2: profileUris[] filters returned endpoints by
       transport profile URI. Parse the wire array once into a local cache so each
       endpoint compares against cached strings instead of re-reading the wire
       array per endpoint (previously N+1 re-parses; now 1 parse plus N cheap
       comparisons). profile_filter_pos is retained for the overflow fallback. */
    const size_t profile_filter_pos = r->position;
    mu_string_t profile_cache[MU_GETENDPOINTS_PROFILE_CACHE_MAX];
    size_t profile_count = 0;
    bool profile_all_match = false;
    bool profile_overflow = false;
    {
        opcua_int32_t wire_count;
        s = mu_binary_read_int32(r, &wire_count);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        /* OPC-10000-6 section 5.2.5: -1 encodes a null array; lower values are malformed. */
        if (wire_count < -1) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        if (wire_count <= 0) {
            /* Spec: "All Endpoints are returned if the list is empty." Skip caching. */
            profile_all_match = true;
        } else {
            opcua_int32_t to_cache = wire_count;
            if (to_cache > (opcua_int32_t)MU_GETENDPOINTS_PROFILE_CACHE_MAX) {
                to_cache = (opcua_int32_t)MU_GETENDPOINTS_PROFILE_CACHE_MAX;
                profile_overflow = true;
            }
            for (opcua_int32_t i = 0; i < wire_count; ++i) {
                mu_string_t uri;
                s = mu_binary_read_string(r, &uri);
                if (s != MU_STATUS_GOOD) {
                    return s;
                }
                if (i < to_cache) {
                    profile_cache[(size_t)i] = uri;
                }
            }
            profile_count = (size_t)to_cache;
        }
    }

    mu_endpoint_description_t eps[MU_DISCOVERY_MAX_ENDPOINTS];
    size_t count = 0;
    s = mu_discovery_get_endpoints(&server->config, eps, MU_DISCOVERY_MAX_ENDPOINTS, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    bool include[MU_DISCOVERY_MAX_ENDPOINTS] = {false};
    size_t filtered_count = 0;
    const size_t bounded_count = count > MU_DISCOVERY_MAX_ENDPOINTS ? MU_DISCOVERY_MAX_ENDPOINTS : count;
    for (size_t i = 0; i < bounded_count; ++i) {
        bool matches = profile_all_match;
        if (!profile_all_match) {
            if (profile_overflow) {
                /* Wire list exceeded the cache; fall back to re-parsing per endpoint. */
                r->position = profile_filter_pos;
                r->status = MU_STATUS_GOOD;
                s = endpoint_matches_profile_filter(r, &eps[i], &matches);
                if (s != MU_STATUS_GOOD) {
                    return s;
                }
            } else {
                matches = false;
                for (size_t j = 0; j < profile_count; ++j) {
                    if (string_matches_cstr(&profile_cache[j], eps[i].transport_profile_uri)) {
                        matches = true;
                        break;
                    }
                }
            }
        }
        if (matches && getendpoints_endpoint_matches(&endpoint_url, &eps[i])) {
            include[i] = true;
            ++filtered_count;
        }
    }

    s = write_response_prefix(w, MU_ID_GETENDPOINTSRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int32(w, (opcua_int32_t)filtered_count); /* Endpoints[] */
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (size_t i = 0; i < bounded_count; ++i) {
        if (!include[i]) {
            continue;
        }
        s = getendpoints_endpoint_description_encode(w, &eps[i], &requested_locale);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* FindServers (OPC-10000-4 §5.5.2.2): validate the mandatory request
   parameters, then return this server's ApplicationDescription. */
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
    s = read_requested_locale_ids(r, &requested_locale); /* localeIds[] */
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_application_description_t app;
    s = mu_discovery_get_application_description(&server->config, &app);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    bool server_uri_matches = false;
    s = findservers_server_uri_filter_matches(r, app.application_uri, &server_uri_matches); /* serverUris[] */
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    bool server_type_matches = false;
    s = findservers_server_type_filter_matches(r, app.application_type, &server_type_matches); /* serverTypes[] */
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    bool include_app =
        findservers_endpoint_matches(&endpoint_url, app.discovery_url) && server_uri_matches && server_type_matches;

    s = write_response_prefix(w, MU_ID_FINDSERVERSRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int32(w, include_app ? 1 : 0); /* Servers[] */
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

#endif /* MUC_OPCUA_SERVICE_DISCOVERY */
