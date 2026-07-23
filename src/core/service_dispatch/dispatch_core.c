#include "common.h"

#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
#include "../../services/subscription.h"
#endif

static opcua_statuscode_t read_auth_token_from_request(const opcua_byte_t *request_body, size_t request_length,
                                                       opcua_uint32_t *auth_token) {
    mu_binary_reader_t reader;
    mu_nodeid_t token;
    opcua_statuscode_t s;

    *auth_token = 0u;
    mu_binary_reader_init(&reader, request_body, request_length);
    s = mu_binary_read_nodeid(&reader, &token);
    if (s != MU_STATUS_GOOD) {
        return MU_STATUS_GOOD;
    }
    if (token.identifier_type != MU_NODEID_NUMERIC || token.namespace_index != 0u) {
        return MU_STATUS_GOOD;
    }

    *auth_token = token.identifier.numeric;
    return MU_STATUS_GOOD;
}

#if defined(MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS) && MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS
static opcua_uint32_t read_return_diagnostics_from_request(const opcua_byte_t *request_body, size_t request_length) {
    mu_binary_reader_t reader;
    mu_request_header_t header;

    mu_binary_reader_init(&reader, request_body, request_length);
    if (mu_request_header_decode(&reader, &header) != MU_STATUS_GOOD) {
        return 0u;
    }
    return header.return_diagnostics;
}
#endif

/* Service dispatch table. Entries MUST appear in ascending order of
   service.request_id: find_service_descriptor() performs a binary search.
   Grounding: OPC-10000-4 §4.1 (Service Set model) and §7.1 — Services are
   dispatched by the numeric identifier of their request NodeId, and the
   specification assigns those identifiers in ascending order per Service Set.
   Each entry retains its own feature-guard so any subset produced by the
   preprocessor remains a sorted subsequence. */
static const mu_service_descriptor_t g_supported_services[] = {
#ifdef MUC_OPCUA_DISCOVERY_FIND_SERVERS_ENABLED
    {{MU_ID_FINDSERVERSREQUEST, MU_ID_FINDSERVERSRESPONSE, false}, handle_find_servers},
#endif
#ifdef MUC_OPCUA_CU_DISCOVERY_GET_ENDPOINTS
    {{MU_ID_GETENDPOINTSREQUEST, MU_ID_GETENDPOINTSRESPONSE, false}, handle_get_endpoints},
#endif
#ifdef MUC_OPCUA_CU_DISCOVERY_REGISTER
    {{MU_ID_REGISTERSERVERREQUEST, MU_ID_REGISTERSERVERRESPONSE, false}, handle_register_server},
/* RegisterServer2's request id (12211) is larger than every other service
   here, so it cannot sit next to RegisterServer without breaking the
   ascending-order invariant the binary search relies on. It is registered
   at the tail of the table instead. */
#endif
    {{MU_ID_OPENSECURECHANNELREQUEST, MU_ID_OPENSECURECHANNELRESPONSE, false}, handle_open_secure_channel},
    {{MU_ID_CLOSESECURECHANNELREQUEST, MU_ID_CLOSESECURECHANNELRESPONSE, false}, handle_close_secure_channel},
    {{MU_ID_CREATESESSIONREQUEST, MU_ID_CREATESESSIONRESPONSE, false}, handle_create_session},
    {{MU_ID_ACTIVATESESSIONREQUEST, MU_ID_ACTIVATESESSIONRESPONSE, false}, handle_activate_session},
    {{MU_ID_CLOSESESSIONREQUEST, MU_ID_CLOSESESSIONRESPONSE, true}, handle_close_session},
#ifdef MUC_OPCUA_CU_SESSION_GENERAL_SERVICE
    {{MU_ID_CANCELREQUEST, MU_ID_CANCELRESPONSE, true}, handle_cancel},
#endif
#ifdef MUC_OPCUA_CU_NODEMANAGEMENT
    {{MU_ID_ADDNODESREQUEST, MU_ID_ADDNODESRESPONSE, true}, handle_add_nodes},
    {{MU_ID_ADDREFERENCESREQUEST, MU_ID_ADDREFERENCESRESPONSE, true}, handle_add_references},
    {{MU_ID_DELETENODESREQUEST, MU_ID_DELETENODESRESPONSE, true}, handle_delete_nodes},
    {{MU_ID_DELETEREFERENCESREQUEST, MU_ID_DELETEREFERENCESRESPONSE, true}, handle_delete_references},
#endif
#ifdef MUC_OPCUA_CU_VIEW_BASIC_2
    {{MU_ID_BROWSEREQUEST, MU_ID_BROWSERESPONSE, true}, handle_browse},
    {{MU_ID_BROWSENEXTREQUEST, MU_ID_BROWSENEXTRESPONSE, true}, handle_browse_next},
#endif
#ifdef MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH
    {{MU_ID_TRANSLATEBROWSEPATHSTONODEIDSREQUEST, MU_ID_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE, true},
     handle_translate_browse_paths},
#endif
#ifdef MUC_OPCUA_CU_VIEW_REGISTERNODES
    {{MU_ID_REGISTERNODESREQUEST, MU_ID_REGISTERNODESRESPONSE, true}, handle_register_nodes},
    {{MU_ID_UNREGISTERNODESREQUEST, MU_ID_UNREGISTERNODESRESPONSE, true}, handle_unregister_nodes},
#endif
#ifdef MUC_OPCUA_CU_QUERY
    {{MU_ID_QUERYFIRSTREQUEST, MU_ID_QUERYFIRSTRESPONSE, true}, handle_query_first},
    {{MU_ID_QUERYNEXTREQUEST, MU_ID_QUERYNEXTRESPONSE, true}, handle_query_next},
#endif
#ifdef MUC_OPCUA_CU_ATTRIBUTE_READ
    {{MU_ID_READREQUEST, MU_ID_READRESPONSE, true}, handle_read},
#endif
#ifdef MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET
    {{MU_ID_HISTORYREADREQUEST, MU_ID_HISTORYREADRESPONSE, true}, handle_history_read},
#endif
#ifdef MUC_OPCUA_SERVICE_WRITE
    {{MU_ID_WRITEREQUEST, MU_ID_WRITERESPONSE, true}, handle_write},
#endif
#ifdef MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET
    {{MU_ID_HISTORYUPDATEREQUEST, MU_ID_HISTORYUPDATEREQUEST, true}, handle_history_update},
#endif
#if MU_DISPATCH_CALL_ENABLED
    {{MU_ID_CALLREQUEST, MU_ID_CALLRESPONSE, true}, handle_call},
#endif
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
    {{MU_ID_CREATEMONITOREDITEMSREQUEST, MU_ID_CREATEMONITOREDITEMSRESPONSE, true}, handle_create_monitored_items},
    {{MU_ID_MODIFYMONITOREDITEMSREQUEST, MU_ID_MODIFYMONITOREDITEMSRESPONSE, true}, handle_modify_monitored_items},
    {{MU_ID_SETMONITORINGMODEREQUEST, MU_ID_SETMONITORINGMODERESPONSE, true}, handle_set_monitoring_mode},
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
    {{MU_ID_SETTRIGGERINGREQUEST, MU_ID_SETTRIGGERINGRESPONSE, true}, handle_set_triggering},
#endif
    {{MU_ID_DELETEMONITOREDITEMSREQUEST, MU_ID_DELETEMONITOREDITEMSRESPONSE, true}, handle_delete_monitored_items},
    {{MU_ID_CREATESUBSCRIPTIONREQUEST, MU_ID_CREATESUBSCRIPTIONRESPONSE, true}, handle_create_subscription},
    {{MU_ID_MODIFYSUBSCRIPTIONREQUEST, MU_ID_MODIFYSUBSCRIPTIONRESPONSE, true}, handle_modify_subscription},
    {{MU_ID_SETPUBLISHINGMODEREQUEST, MU_ID_SETPUBLISHINGMODERESPONSE, true}, handle_set_publishing_mode},
    {{MU_ID_PUBLISHREQUEST, MU_ID_PUBLISHRESPONSE, true}, handle_publish},
    {{MU_ID_REPUBLISHREQUEST, MU_ID_REPUBLISHRESPONSE, true}, handle_republish},
#if MUC_OPCUA_CU_REDUNDANCY
    {{MU_ID_TRANSFERSUBSCRIPTIONSREQUEST, MU_ID_TRANSFERSUBSCRIPTIONSRESPONSE, true}, handle_transfer_subscriptions},
#endif
    {{MU_ID_DELETESUBSCRIPTIONSREQUEST, MU_ID_DELETESUBSCRIPTIONSRESPONSE, true}, handle_delete_subscriptions},
#endif
#ifdef MUC_OPCUA_CU_DISCOVERY_REGISTER
    /* RegisterServer2 tail entry -- see the note by RegisterServer above. Its
       request id (12211, the _Encoding_DefaultBinary NodeId) is the numerically
       largest in the table, so the ascending-order invariant places it last. */
    {{MU_ID_REGISTERSERVER2REQUEST, MU_ID_REGISTERSERVER2RESPONSE, false}, handle_register_server},
#endif
};

static const size_t g_num_supported_services = sizeof(g_supported_services) / sizeof(g_supported_services[0]);

/* Binary search over g_supported_services[] (sorted by service.request_id).
   Reduces dispatch lookup from O(N) (~15 comparisons across the full table) to
   O(log N) (≤6 comparisons worst case for the largest profile; ≤5 for typical
   profiles). Grounding: OPC-10000-4 §4.1, §7.1. */
static const mu_service_descriptor_t *find_service_descriptor(opcua_uint32_t request_id) {
    size_t lo = 0;
    size_t hi = g_num_supported_services;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) / 2u);
        opcua_uint32_t mid_id = g_supported_services[mid].service.request_id;
        if (mid_id == request_id) {
            return &g_supported_services[mid];
        }
        if (mid_id < request_id) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return NULL;
}

const mu_service_handler_t *mu_get_service_handler(opcua_uint32_t request_id) {
    const mu_service_descriptor_t *descriptor = find_service_descriptor(request_id);
    return descriptor != NULL ? &descriptor->service : NULL;
}

opcua_statuscode_t mu_service_dispatch(mu_server_t *server, opcua_uint32_t request_id, const opcua_byte_t *request_body,
                                       size_t request_length, opcua_byte_t *response_body, size_t *response_length) {
    if (!server || !request_body || !response_body || !response_length) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    server->active_session = NULL;
#if defined(MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS) && MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS
    server->return_diagnostics = 0u;
#endif

    const mu_service_descriptor_t *descriptor = find_service_descriptor(request_id);
    if (descriptor == NULL) {
        return MU_STATUS_BAD_SERVICEUNSUPPORTED;
    }

    if (request_id != MU_ID_OPENSECURECHANNELREQUEST) {
        if (!server_secure_channel.is_open) {
            return MU_STATUS_BAD_SECURECHANNELIDINVALID;
        }
    }

    if (descriptor->service.requires_session) {
        opcua_uint32_t auth_token = 0u;
        opcua_statuscode_t s = read_auth_token_from_request(request_body, request_length, &auth_token);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (auth_token == 0u) {
            mu_diagnostics_request_rejected(server, false);
            return MU_STATUS_BAD_SESSIONIDINVALID;
        }
        mu_session_t *session = mu_session_find_by_token(server->sessions, MU_INTERN_MAX_SESSIONS, auth_token);
        if (session == NULL) {
            mu_diagnostics_request_rejected(server, false);
            if (mu_session_find_closed_by_token(server->sessions, MU_INTERN_MAX_SESSIONS, auth_token) != NULL) {
                return MU_STATUS_BAD_SESSIONCLOSED;
            }
            return MU_STATUS_BAD_SESSIONIDINVALID;
        }
        s = mu_session_validate_secure_channel(session, server_secure_channel.channel_id);
        if (s != MU_STATUS_GOOD) {
            /* wrong SecureChannel for this Session -> a security-related rejection. */
            mu_diagnostics_request_rejected(server, true);
            return s;
        }
        if (session->state != MU_SESSION_STATE_ACTIVATED) {
            mu_diagnostics_request_rejected(server, false);
            return MU_STATUS_BAD_SESSIONNOTACTIVATED;
        }
        server->active_session = session;
        if (server->config.time_adapter.get_tick_ms != NULL) {
            session->last_activity_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
        }
    }

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, request_body, request_length);
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, response_body, *response_length);

    if (descriptor->handler != NULL) {
#if defined(MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS) && MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS
        server->return_diagnostics = read_return_diagnostics_from_request(request_body, request_length);
#endif
        return descriptor->handler(server, &reader, &writer, response_length);
    }

    *response_length = 0;
    return MU_STATUS_GOOD;
}
