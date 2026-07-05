/* src/core/dispatch_discovery/common.c
 * Shared helper implementations for discovery service handlers. */
#include "common.h"

#ifdef MUC_OPCUA_SERVICE_DISCOVERY

bool string_matches_cstr(const mu_string_t *left, const char *right) {
    if (left == NULL || right == NULL || left->length < 0 || left->data == NULL) {
        return false;
    }
    size_t right_len = strlen(right);
    return (size_t)left->length == right_len && memcmp(left->data, right, right_len) == 0;
}

bool locale_id_is_usable(const mu_string_t *locale_id) {
    return locale_id != NULL && locale_id->length > 0 && locale_id->data != NULL;
}

opcua_statuscode_t read_requested_locale_ids(mu_binary_reader_t *r, mu_string_t *selected_locale) {
    opcua_int32_t count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
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

opcua_statuscode_t findservers_write_cstr(mu_binary_writer_t *w, const char *value) {
    if (value == NULL) {
        mu_string_t null_str = {-1, NULL};
        return mu_binary_write_string(w, &null_str);
    }
    mu_string_t str = {(opcua_int32_t)strlen(value), (const opcua_byte_t *)value};
    return mu_binary_write_string(w, &str);
}

opcua_statuscode_t findservers_write_localizedtext(mu_binary_writer_t *w, const char *text,
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

opcua_statuscode_t findservers_application_description_encode(mu_binary_writer_t *w,
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
        return s;
    }
    s = findservers_write_cstr(w, NULL);
    if (s != MU_STATUS_GOOD) {
        return s;
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

#endif /* MUC_OPCUA_SERVICE_DISCOVERY */
