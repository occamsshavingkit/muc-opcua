/* src/core/status.c */
#include "micro_opcua/status.h"

const char* mu_status_name(opcua_statuscode_t status)
{
    switch (status) {
        case MU_STATUS_GOOD: return "Good";
        case MU_STATUS_BAD_UNEXPECTEDERROR: return "Bad_UnexpectedError";
        case MU_STATUS_BAD_INTERNALERROR: return "Bad_InternalError";
        case MU_STATUS_BAD_OUTOFMEMORY: return "Bad_OutOfMemory";
        case MU_STATUS_BAD_ENCODINGERROR: return "Bad_EncodingError";
        case MU_STATUS_BAD_DECODINGERROR: return "Bad_DecodingError";
        case MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED: return "Bad_EncodingLimitsExceeded";
        case MU_STATUS_BAD_TIMEOUT: return "Bad_Timeout";
        case MU_STATUS_BAD_SERVICEUNSUPPORTED: return "Bad_ServiceUnsupported";
        case MU_STATUS_BAD_NOTREADABLE: return "Bad_NotReadable";
        case MU_STATUS_BAD_SECURITYCHECKSFAILED: return "Bad_SecurityChecksFailed";
        case MU_STATUS_BAD_REQUESTTOOLARGE: return "Bad_RequestTooLarge";
        case MU_STATUS_BAD_RESPONSETOOLARGE: return "Bad_ResponseTooLarge";
        case MU_STATUS_BAD_IDENTITYTOKENINVALID: return "Bad_IdentityTokenInvalid";
        case MU_STATUS_BAD_SESSIONIDINVALID: return "Bad_SessionIdInvalid";

        /* TCP Specific StatusCodes */
        case MU_STATUS_BAD_TCPSERVERTOOBUSY: return "Bad_TcpServerTooBusy";
        case MU_STATUS_BAD_TCPMESSAGETYPEINVALID: return "Bad_TcpMessageTypeInvalid";
        case MU_STATUS_BAD_TCPSECURECHANNELUNKNOWN: return "Bad_TcpSecureChannelUnknown";
        case MU_STATUS_BAD_TCPMESSAGETOOLARGE: return "Bad_TcpMessageTooLarge";
        case MU_STATUS_BAD_TCPNOTENOUGHRESOURCES: return "Bad_TcpNotEnoughResources";
        case MU_STATUS_BAD_TCPINTERNALERROR: return "Bad_TcpInternalError";
        case MU_STATUS_BAD_TCPENDPOINTURLINVALID: return "Bad_TcpEndpointUrlInvalid";

        default: return "Unknown_StatusCode";
    }
}
