/* src/core/dispatch_discovery/get_endpoints.c
 * GetEndpoints service handler (OPC-10000-4 5.5.4.2). */
#include "common.h"

#ifdef MUC_OPCUA_SERVICE_DISCOVERY

static bool getendpoints_endpoint_matches(const mu_string_t *endpoint_url, const mu_endpoint_description_t *endpoint) {
    if (endpoint_url == NULL || endpoint_url->length <= 0) {
        return true;
    }

    return endpoint != NULL && string_matches_cstr(endpoint_url, endpoint->endpoint_url);
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
    s = findservers_application_description_encode(w, &desc->server, requested_locale);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    {
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

#define MU_GETENDPOINTS_PROFILE_CACHE_MAX 8

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
    s = read_requested_locale_ids(r, &requested_locale);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

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
        if (wire_count < -1) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        if (wire_count <= 0) {
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
    s = mu_binary_write_int32(w, (opcua_int32_t)filtered_count);
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

#endif /* MUC_OPCUA_SERVICE_DISCOVERY */
