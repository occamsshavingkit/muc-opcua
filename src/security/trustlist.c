/* src/security/trustlist.c */
#include "micro_opcua/security.h"
#include "micro_opcua/status.h"
#include <string.h>

opcua_statuscode_t mu_trust_list_match(const mu_trust_list_t *trust_list, const opcua_byte_t *cert_data,
                                       size_t cert_length) {
    if (trust_list == NULL || trust_list->count == 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    if (cert_data == NULL || cert_length == 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    for (size_t i = 0; i < trust_list->count; ++i) {
        if (trust_list->lengths[i] == cert_length && memcmp(trust_list->certificates[i], cert_data, cert_length) == 0) {
            return MU_STATUS_GOOD; /* Exact match found */
        }
    }

    return MU_STATUS_BAD_SECURITYCHECKSFAILED;
}
