#ifndef MUC_OPCUA_CU_DIAGNOSTICS_H
#define MUC_OPCUA_CU_DIAGNOSTICS_H

#include "muc_opcua/config.h" // IWYU pragma: keep
#include "services/session.h"

#include <stdbool.h>

#if MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS && MUC_OPCUA_CU_USER_TOKEN_JWT
void mu_diagnostics_session_identity(const mu_session_t *session, opcua_byte_t *out_kind,
                                     const opcua_byte_t **out_identity, opcua_byte_t *out_identity_len);
opcua_byte_t mu_diagnostics_session_role_count(const mu_session_t *session);
bool mu_diagnostics_session_role_id(const mu_session_t *session, opcua_byte_t index, opcua_uint32_t *out_role_id);
#endif

#endif
