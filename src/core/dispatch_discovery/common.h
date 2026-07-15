/* src/core/dispatch_discovery/common.h
 * Shared declarations for discovery service handlers. */
#ifndef DISPATCH_DISCOVERY_COMMON_H
#define DISPATCH_DISCOVERY_COMMON_H

#include "../../services/discovery.h"
#include "../../services/service_header.h"
#include "../server_internal.h"
#include "../service_dispatch.h"
#include "muc_opcua/address_space.h"
#include "muc_opcua/encoding.h"
#include <stddef.h>
#include <string.h>

#ifdef MUC_OPCUA_DISCOVERY_SHARED

bool string_matches_cstr(const mu_string_t *left, const char *right);
bool locale_id_is_usable(const mu_string_t *locale_id);
opcua_statuscode_t read_requested_locale_ids(mu_binary_reader_t *r, mu_string_t *selected_locale);
opcua_statuscode_t findservers_write_cstr(mu_binary_writer_t *w, const char *value);
opcua_statuscode_t findservers_write_localizedtext(mu_binary_writer_t *w, const char *text,
                                                   const mu_string_t *requested_locale);
opcua_statuscode_t findservers_application_description_encode(mu_binary_writer_t *w,
                                                              const mu_application_description_t *desc,
                                                              const mu_string_t *requested_locale);

#endif /* MUC_OPCUA_DISCOVERY_SHARED */
#endif /* DISPATCH_DISCOVERY_COMMON_H */
