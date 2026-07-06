/* tests/support/mock_transport.h
 * Shared mock transport helpers for OPC UA server integration tests.
 *
 * Pattern: provide a queue of inbound chunks and capture the server's last write.
 * Wire into mu_server_config_t.tcp_adapter to drive mu_server_poll() without a
 * real TCP stack.
 *
 * OPC-10000-6 §7.1.2  OPC UA TCP transport
 * OPC-10000-6 §7.1.2.3 Hello
 * OPC-10000-6 §7.1.2.4 Acknowledge
 */
#ifndef MOCK_TRANSPORT_H
#define MOCK_TRANSPORT_H

#include "muc_opcua/encoding.h"
#include "muc_opcua/platform.h"
#include "muc_opcua/status.h"
#include <string.h>

#define MOCK_MAX_INBOUND 8

typedef struct {
    int accept_count;
    opcua_byte_t inbound[MOCK_MAX_INBOUND][512];
    size_t inbound_len[MOCK_MAX_INBOUND];
    size_t inbound_count;
    size_t read_index;
    opcua_byte_t last_write[8192];
    size_t last_write_len;
} mock_t;

static inline void mock_enqueue(mock_t *m, const opcua_byte_t *bytes, size_t len) {
    (void)memcpy(m->inbound[m->inbound_count], bytes, len);
    m->inbound_len[m->inbound_count] = len;
    m->inbound_count++;
}

static inline opcua_statuscode_t mock_listen(void *c, const char *u) {
    (void)c;
    (void)u;
    return MU_STATUS_GOOD;
}

static inline void mock_shutdown(void *c) {
    (void)c;
}

static inline void mock_close(void *c, void *h) {
    (void)c;
    (void)h;
}

static inline opcua_statuscode_t mock_accept(void *c, void **handle) {
    mock_t *m = (mock_t *)c;
    m->accept_count++;
    *handle = (m->accept_count == 1) ? (void *)1 : NULL;
    return MU_STATUS_GOOD;
}

static inline opcua_statuscode_t mock_read(void *c, void *h, opcua_byte_t *buf, size_t cap, size_t *n) {
    mock_t *m = (mock_t *)c;
    (void)h;
    if (m->read_index >= m->inbound_count) {
        *n = 0;
        return MU_STATUS_GOOD;
    }
    size_t len = m->inbound_len[m->read_index];
    if (len > cap) {
        return MU_STATUS_BAD_UNEXPECTEDERROR;
    }
    (void)memcpy(buf, m->inbound[m->read_index], len);
    m->read_index++;
    *n = len;
    return MU_STATUS_GOOD;
}

static inline opcua_statuscode_t mock_write(void *c, void *h, const opcua_byte_t *buf, size_t len, size_t *n) {
    mock_t *m = (mock_t *)c;
    (void)h;
    (void)memcpy(m->last_write, buf, len);
    m->last_write_len = len;
    *n = len;
    return MU_STATUS_GOOD;
}

/* ---- wire-format helpers ---- */

/* Build an OPC UA MSG chunk (OPC-10000-6 §6.7.2.2).
 * Writes MSG framing: magic "MSGF" + size + channel_id + 12-byte security
 * header (token_id, seq, reqid) + body. Returns total chunk length. */
static inline size_t mock_build_msg(opcua_byte_t *out, size_t cap, opcua_uint32_t seq, opcua_uint32_t reqid,
                                    opcua_uint32_t channel_id, opcua_uint32_t token_id, const opcua_byte_t *body,
                                    size_t body_len) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, cap);
    out[0] = 'M';
    out[1] = 'S';
    out[2] = 'G';
    out[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, (opcua_uint32_t)(24 + body_len));
    mu_binary_write_uint32(&w, channel_id);
    mu_binary_write_uint32(&w, token_id);
    mu_binary_write_uint32(&w, seq);
    mu_binary_write_uint32(&w, reqid);
    (void)memcpy(out + 24, body, body_len);
    return 24 + body_len;
}

/* Read past a response's MSG chunk header + ResponseHeader, returning the
 * response type NodeId numeric identifier and a reader positioned at the
 * service body. */
static inline opcua_uint32_t mock_parse_response(const opcua_byte_t *buf, size_t len, mu_binary_reader_t *body) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, len);
    r.position = 24;
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    opcua_int64_t ts;
    opcua_uint32_t h;
    opcua_statuscode_t res;
    opcua_byte_t diag;
    opcua_byte_t enc;
    opcua_int32_t st;
    mu_nodeid_t addl;
    mu_binary_read_int64(&r, &ts);
    mu_binary_read_uint32(&r, &h);
    mu_binary_read_statuscode(&r, &res);
    mu_binary_read_byte(&r, &diag);
    mu_binary_read_int32(&r, &st);
    mu_binary_read_nodeid(&r, &addl);
    mu_binary_read_byte(&r, &enc);
    *body = r;
    return type.identifier.numeric;
}

/* Read the SecureChannelId from bytes 8–11 (LE) of an OPN response. */
static inline opcua_uint32_t mock_read_opn_channel_id(const opcua_byte_t *buf, size_t len) {
    if (len < 12) {
        return 1u;
    }
    return (opcua_uint32_t)buf[8] | ((opcua_uint32_t)buf[9] << 8u) | ((opcua_uint32_t)buf[10] << 16u) |
           ((opcua_uint32_t)buf[11] << 24u);
}

/* Patch the SecureChannelId field (offset 8, LE) in every enqueued MSG/CLO
 * chunk so they carry the server-assigned id rather than a placeholder. */
static inline void mock_patch_msg_channel_ids(mock_t *m, opcua_uint32_t channel_id) {
    for (size_t i = 0; i < m->inbound_count; ++i) {
        if (m->inbound_len[i] >= 12 && ((m->inbound[i][0] == 'M' && m->inbound[i][1] == 'S' &&
                                         m->inbound[i][2] == 'G' && m->inbound[i][3] == 'F') ||
                                        (m->inbound[i][0] == 'C' && m->inbound[i][1] == 'L' &&
                                         m->inbound[i][2] == 'O' && m->inbound[i][3] == 'F'))) {
            m->inbound[i][8] = (opcua_byte_t)(channel_id);
            m->inbound[i][9] = (opcua_byte_t)(channel_id >> 8u);
            m->inbound[i][10] = (opcua_byte_t)(channel_id >> 16u);
            m->inbound[i][11] = (opcua_byte_t)(channel_id >> 24u);
        }
    }
}

#endif /* MOCK_TRANSPORT_H */
