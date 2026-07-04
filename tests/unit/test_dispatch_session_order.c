/* tests/unit/test_dispatch_session_order.c
 *
 * RED-phase test for T008 (User Story 3). Drives the CreateSession dispatch
 * path with a valid request but an undersized response buffer so that a
 * response-body encode step fails AFTER the session slot has been allocated.
 * The session slot must be reclaimed (no leaked slot in mu_max_sessions).
 *
 * Grounding: OPC-10000-4 §5.7.3 (CreateSession — server shall return the
 * CreateSession response; encode failure after session allocation must not
 * silently leak a session slot). The Server allocates a SessionId and
 * AuthenticationToken in CreateSession; if the subsequent response encoding
 * fails the allocated slot must be returned to the free pool so a later
 * CreateSession does not observe Bad_TooManySessions for a slot it never
 * received (OPC-10000-4 §5.7.2.2 — Bad_TooManySession is reported only when
 * the maximum number of supported Sessions has been reached).
 *
 * This test is expected to FAIL until T010 adds the cleanup path in
 * handle_create_session (src/core/service_dispatch.c). */
#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "../../src/services/session.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static opcua_datetime_t fake_time(void *c) {
    (void)c;
    return 0;
}

/* Deterministic entropy: every byte 0x42. This yields a first session_id of
 * 0x42424242 (numeric NodeId > 65535 -> 7-byte full numeric encoding) and a
 * first auth_token of 0xE7E7E7E7 (likewise 7 bytes). */
static opcua_statuscode_t fake_entropy(void *c, opcua_byte_t *buf, size_t len) {
    (void)c;
    if (buf) {
        (void)memset(buf, 0x42, len);
    }
    return MU_STATUS_GOOD;
}

/* Write a RequestHeader (OPC-10000-4 §7.32) with the given requestHandle. */
static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t handle) {
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_nodeid_t auth_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_nodeid(w, &auth_id);                     /* authenticationToken */
    mu_binary_write_int64(w, 0);                             /* timestamp */
    mu_binary_write_uint32(w, handle);                       /* requestHandle */
    mu_binary_write_uint32(w, 0);                            /* returnDiagnostics */
    mu_binary_write_string(w, &null_str);                    /* auditEntryId */
    mu_binary_write_uint32(w, 0);                            /* timeoutHint */
    mu_binary_write_extension_object_header(w, &null_id, 0); /* additionalHeader */
}

/* Build a full CreateSessionRequest body (positioned after the request-type
 * NodeId, as mu_service_dispatch delivers it): RequestHeader +
 * ClientDescription + ServerUri + EndpointUrl + SessionName + ClientNonce +
 * ClientCertificate + RequestedSessionTimeout + MaxResponseMessageSize. */
static size_t build_create_session_body(opcua_byte_t *buf, size_t cap) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, cap);
    write_request_header(&w, 7);
    mu_string_t ns = {-1, NULL};
    mu_bytestring_t nb = {-1, NULL};
    mu_binary_write_string(&w, &ns);     /* applicationUri */
    mu_binary_write_string(&w, &ns);     /* productUri */
    mu_binary_write_byte(&w, 0x00);      /* applicationName LocalizedText (empty) */
    mu_binary_write_uint32(&w, 1);       /* applicationType = CLIENT */
    mu_binary_write_string(&w, &ns);     /* gatewayServerUri */
    mu_binary_write_string(&w, &ns);     /* discoveryProfileUri */
    mu_binary_write_int32(&w, 0);        /* discoveryUrls[] */
    mu_binary_write_string(&w, &ns);     /* ServerUri */
    mu_binary_write_string(&w, &ns);     /* EndpointUrl */
    mu_binary_write_string(&w, &ns);     /* SessionName */
    mu_binary_write_bytestring(&w, &nb); /* ClientNonce */
    mu_binary_write_bytestring(&w, &nb); /* ClientCertificate */
    mu_binary_write_double(&w, 1200000.0); /* RequestedSessionTimeout */
    mu_binary_write_uint32(&w, 0);       /* MaxResponseMessageSize */
    return w.position;
}

/* Response prefix footprint (write_response_prefix) is 28 bytes:
 *   response-type NodeId i=462 (FourByte: 4) + ResponseHeader (24) =
 *   8 (timestamp) + 4 (requestHandle) + 4 (serviceResult) + 1 (diagInfo) +
 *   4 (stringTable) + 3 (additionalHeader). A 48-byte buffer therefore lets
 *   write_response_prefix succeed, then lets the SessionId (7) and
 *   AuthenticationToken (7) NodeIds encode, but the RevisedSessionTimeout
 *   Double (8 bytes) cannot fit (28 + 7 + 7 = 42 used, 6 remaining). The
 *   encoding failure is thus at a body field written AFTER the session slot
 *   has been transitioned to MU_SESSION_STATE_CREATED by
 *   mu_session_create_with_identifiers. */
#define SESSION_ORDER_RESP_BUF_LEN 48u

void test_create_session_encode_failure_reclaims_session_slot(void) {
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    mu_session_init(&server.sessions[0]);
    mu_session_init(&server.sessions[1]);
    server.config.time_adapter.get_time = fake_time;
    server.config.entropy_adapter.generate_random = fake_entropy;

    /* Sanity: both slots start free. */
    TEST_ASSERT_NOT_NULL(mu_session_find_free(server.sessions, MU_MAX_SESSIONS));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CLOSED, server.sessions[0].state);

    opcua_byte_t req[256];
    size_t req_len = build_create_session_body(req, sizeof(req));

    opcua_byte_t resp[SESSION_ORDER_RESP_BUF_LEN];
    size_t resp_len = sizeof(resp);
    opcua_statuscode_t s =
        mu_service_dispatch(&server, MU_ID_CREATESESSIONREQUEST, req, req_len, resp, &resp_len);

    /* OPC-10000-4 §5.3: an undersized response buffer yields an encoding
     * fault (Bad_EncodingError / Bad_EncodingLimitsExceeded), not Good. */
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, s);

    /* OPC-10000-4 §5.7.3: the response encoding failed AFTER the session was
     * allocated. The slot MUST be reclaimed so it is not leaked. A leaked
     * slot would remain in MU_SESSION_STATE_CREATED. This assertion FAILS
     * (RED) until T010 adds the cleanup path. */
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CLOSED, server.sessions[0].state);

    /* No leaked slot: a free slot must still be available for the next
     * CreateSession. */
    TEST_ASSERT_NOT_NULL(mu_session_find_free(server.sessions, MU_MAX_SESSIONS));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_session_encode_failure_reclaims_session_slot);
    return UNITY_END();
}
