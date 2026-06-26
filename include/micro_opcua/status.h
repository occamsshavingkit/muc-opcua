/* include/micro_opcua/status.h */
#ifndef MICRO_OPCUA_STATUS_H
#define MICRO_OPCUA_STATUS_H

#include "micro_opcua/opcua_types.h"

/* Common OPC UA StatusCodes (OPC 10000-4, 7.38.2 / OPC 10000-6, 7.1.5) */
#define MU_STATUS_GOOD                          0x00000000
#define MU_STATUS_BAD_UNEXPECTEDERROR           0x80010000
#define MU_STATUS_BAD_INTERNALERROR             0x80020000
#define MU_STATUS_BAD_OUTOFMEMORY               0x80030000
#define MU_STATUS_BAD_ENCODINGERROR             0x80060000
#define MU_STATUS_BAD_DECODINGERROR             0x80070000
#define MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED    0x80080000
#define MU_STATUS_BAD_TIMEOUT                   0x800A0000
#define MU_STATUS_BAD_SERVICEUNSUPPORTED        0x800B0000
#define MU_STATUS_BAD_NOTREADABLE               0x803A0000
#define MU_STATUS_BAD_SECURITYCHECKSFAILED      0x80130000
#define MU_STATUS_BAD_REQUESTTOOLARGE           0x80B80000
#define MU_STATUS_BAD_RESPONSETOOLARGE          0x80B90000
#define MU_STATUS_BAD_SESSIONIDINVALID        ((opcua_statuscode_t)0x80250000)
#define MU_STATUS_BAD_IDENTITYTOKENINVALID    ((opcua_statuscode_t)0x80200000)
#define MU_STATUS_BAD_SECURECHANNELIDINVALID  ((opcua_statuscode_t)0x80210000)
#define MU_STATUS_BAD_SECURITYPOLICYREJECTED  ((opcua_statuscode_t)0x80550000)
#define MU_STATUS_BAD_SESSIONCLOSED           ((opcua_statuscode_t)0x80260000)
#define MU_STATUS_BAD_NODEIDUNKNOWN           ((opcua_statuscode_t)0x80340000)
#define MU_STATUS_BAD_TOOMANYOPERATIONS       ((opcua_statuscode_t)0x80100000)
#define MU_STATUS_BAD_NOCONTINUATIONPOINTS    ((opcua_statuscode_t)0x80140000)
#define MU_STATUS_BAD_ATTRIBUTEIDINVALID      ((opcua_statuscode_t)0x80350000)
#define MU_STATUS_BAD_CERTIFICATEINVALID      ((opcua_statuscode_t)0x80120000)

/* TCP Specific StatusCodes */
#define MU_STATUS_BAD_TCPSERVERTOOBUSY          0x807D0000
#define MU_STATUS_BAD_TCPMESSAGETYPEINVALID     0x807E0000
#define MU_STATUS_BAD_TCPSECURECHANNELUNKNOWN   0x807F0000
#define MU_STATUS_BAD_TCPMESSAGETOOLARGE        0x80800000
#define MU_STATUS_BAD_TCPNOTENOUGHRESOURCES     0x80810000
#define MU_STATUS_BAD_TCPINTERNALERROR          0x80820000
#define MU_STATUS_BAD_TCPENDPOINTURLINVALID     0x80830000

#ifdef __cplusplus
extern "C" {
#endif

/* Helper prototype */
const char* mu_status_name(opcua_statuscode_t status);
bool mu_is_unsupported_service(opcua_uint32_t request_id);

#ifdef __cplusplus
}
#endif

#endif /* MICRO_OPCUA_STATUS_H */
