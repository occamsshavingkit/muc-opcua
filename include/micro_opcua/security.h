/* include/micro_opcua/security.h */
#ifndef MICRO_OPCUA_SECURITY_H
#define MICRO_OPCUA_SECURITY_H

#include "micro_opcua/opcua_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * TrustList for basic Application Authentication (OPC 10000-4, section 5.6.2).
 * Used for exact byte-for-byte matching of client certificates.
 */
typedef struct {
    const opcua_byte_t **certificates; /* Array of pointers to DER-encoded certificates */
    const size_t *lengths;             /* Array of lengths corresponding to certificates */
    size_t count;                      /* Number of certificates in the TrustList */
} mu_trust_list_t;

/* Matches a certificate against the TrustList. Returns MU_STATUS_GOOD if trusted. */
opcua_statuscode_t mu_trust_list_match(const mu_trust_list_t *trust_list, const opcua_byte_t *cert_data,
                                       size_t cert_length);

#ifdef __cplusplus
}
#endif

#endif /* MICRO_OPCUA_SECURITY_H */
