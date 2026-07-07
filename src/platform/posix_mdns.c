/* src/platform/posix_mdns.c
 * POSIX host mDNS adapter — UDP multicast DNS-SD publishing.
 * Uses raw UDP multicast on 224.0.0.251:5353 per RFC 6762.
 * Available only on POSIX platforms (Linux, macOS).
 *
 * Sends PTR, SRV, TXT, and A records in a single unicast/multicast datagram
 * per RFC 6763, plus a goodbye packet (TTL=0) on unpublish per RFC 6762 §10.1.
 */
#include "posix_mdns.h"
#include "muc_opcua/status.h"

#if defined(__linux__) || defined(__APPLE__)
#define _GNU_SOURCE

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* mDNS multicast group and port (RFC 6762) */
#define MDNS_PORT 5353

/* DNS record type constants */
#define DNS_TYPE_PTR 12
#define DNS_TYPE_SRV 33
#define DNS_TYPE_TXT 16
#define DNS_TYPE_A   1
#define DNS_CLASS_IN 1

/* Per-packet state; stack-allocated during publish/unpublish */
typedef struct {
    unsigned char buf[512];
    size_t pos;
} dns_writer_t;

static void dns_write_u16(dns_writer_t *w, uint16_t v) {
    w->buf[w->pos++] = (unsigned char)(v >> 8);
    w->buf[w->pos++] = (unsigned char)(v & 0xFF);
}

static void dns_write_name(dns_writer_t *w, const char *name) {
    while (*name) {
        const char *dot = strchr(name, '.');
        size_t len = dot ? (size_t)(dot - name) : strlen(name);
        w->buf[w->pos++] = (unsigned char)len;
        memcpy(w->buf + w->pos, name, len);
        w->pos += len;
        name = dot ? dot + 1 : name + len;
    }
    w->buf[w->pos++] = 0;
}

static void dns_write_record(dns_writer_t *w, const char *name, uint16_t type, uint16_t cls, uint32_t ttl, uint16_t rdlen) {
    dns_write_name(w, name);
    dns_write_u16(w, type);
    dns_write_u16(w, cls);
    dns_write_u16(w, (uint16_t)(ttl >> 16));
    dns_write_u16(w, (uint16_t)(ttl & 0xFFFF));
    dns_write_u16(w, rdlen);
}

/* Context per adapter: holds socket fd */
typedef struct {
    int fd;
    struct sockaddr_in dst;
} mdns_ctx_t;

static void mdns_publish_impl(void *ctx, const char *hostname, uint16_t port,
                               const char *app_uri, const char *path) {
    (void)path;
    mdns_ctx_t *c = (mdns_ctx_t *)ctx;
    if (c == NULL || c->fd < 0 || hostname == NULL || *hostname == '\0')
        return;

    dns_writer_t w;
    memset(&w, 0, sizeof(w));

    /* DNS header: ID=0, flags=response+authoritative, 1 question, 3 answer RRs */
    dns_write_u16(&w, 0);
    dns_write_u16(&w, 0x8400);
    dns_write_u16(&w, 0);
    dns_write_u16(&w, 3);       /* answer RRs: PTR, SRV, TXT, A */
    dns_write_u16(&w, 0);
    dns_write_u16(&w, 0);

    /* PTR: _opcua-tcp._tcp.local -> hostname._opcua-tcp._tcp.local */
    dns_write_record(&w, "_opcua-tcp._tcp.local", DNS_TYPE_PTR, DNS_CLASS_IN, 120, 0);
    size_t ptr_rdlength_pos = w.pos;
    dns_write_u16(&w, 0);
    size_t ptr_start = w.pos;
    dns_write_name(&w, hostname);
    dns_write_name(&w, "_opcua-tcp._tcp.local");
    uint16_t ptr_len = (uint16_t)(w.pos - ptr_start);
    w.buf[ptr_rdlength_pos] = (unsigned char)(ptr_len >> 8);
    w.buf[ptr_rdlength_pos + 1] = (unsigned char)(ptr_len & 0xFF);

    /* SRV: hostname._opcua-tcp._tcp.local -> 0 0 port hostname.local */
    char srv_name[256];
    snprintf(srv_name, sizeof(srv_name), "%s._opcua-tcp._tcp.local", hostname);
    dns_write_record(&w, srv_name, DNS_TYPE_SRV, DNS_CLASS_IN, 120, 0);
    size_t srv_rdlength_pos = w.pos;
    dns_write_u16(&w, 0);
    size_t srv_start = w.pos;
    dns_write_u16(&w, 0);
    dns_write_u16(&w, 0);
    dns_write_u16(&w, port);
    char hostname_local[256];
    snprintf(hostname_local, sizeof(hostname_local), "%s.local", hostname);
    dns_write_name(&w, hostname_local);
    uint16_t srv_len = (uint16_t)(w.pos - srv_start);
    w.buf[srv_rdlength_pos] = (unsigned char)(srv_len >> 8);
    w.buf[srv_rdlength_pos + 1] = (unsigned char)(srv_len & 0xFF);

    /* TXT: hostname.local -> "path=/discovery" "applicationUri=<uri>" */
    unsigned char txt_data[512];
    size_t txt_len = 0;
    const char *path_kv = "path=/discovery";
    unsigned char path_kv_len = (unsigned char)strlen(path_kv);
    txt_data[txt_len++] = path_kv_len;
    memcpy(txt_data + txt_len, path_kv, (size_t)path_kv_len);
    txt_len += (size_t)path_kv_len;
    if (app_uri != NULL && *app_uri != '\0') {
        unsigned char app_len = (unsigned char)(strlen("applicationUri=") + strlen(app_uri));
        txt_data[txt_len++] = app_len;
        memcpy(txt_data + txt_len, "applicationUri=", strlen("applicationUri="));
        txt_len += strlen("applicationUri=");
        memcpy(txt_data + txt_len, app_uri, strlen(app_uri));
        txt_len += strlen(app_uri);
    }
    dns_write_record(&w, hostname_local, DNS_TYPE_TXT, DNS_CLASS_IN, 120, (uint16_t)txt_len);
    memcpy(w.buf + w.pos, txt_data, txt_len);
    w.pos += txt_len;

    /* A: hostname.local -> bound interface address */
    dns_write_record(&w, hostname_local, DNS_TYPE_A, DNS_CLASS_IN, 120, 4);
    struct sockaddr_in sin;
    socklen_t slen = sizeof(sin);
    if (getsockname(c->fd, (struct sockaddr *)&sin, &slen) == 0) {
        memcpy(w.buf + w.pos, &sin.sin_addr, 4);
    }
    w.pos += 4;

    sendto(c->fd, w.buf, w.pos, 0, (struct sockaddr *)&c->dst, sizeof(c->dst));
}

static void mdns_unpublish_impl(void *ctx) {
    mdns_ctx_t *c = (mdns_ctx_t *)ctx;
    if (c == NULL || c->fd < 0)
        return;

    dns_writer_t w;
    memset(&w, 0, sizeof(w));

    /* Goodbye: same PTR record with TTL=0 (RFC 6762 §10.1) */
    dns_write_u16(&w, 0);
    dns_write_u16(&w, 0x8400);
    dns_write_u16(&w, 0);
    dns_write_u16(&w, 1);
    dns_write_u16(&w, 0);
    dns_write_u16(&w, 0);

    dns_write_record(&w, "_opcua-tcp._tcp.local", DNS_TYPE_PTR, DNS_CLASS_IN, 0, 0);
    size_t rdlen_pos = w.pos;
    dns_write_u16(&w, 0);
    size_t rdata_start = w.pos;
    dns_write_name(&w, "_services._dns-sd._udp.local");
    uint16_t rdlen = (uint16_t)(w.pos - rdata_start);
    w.buf[rdlen_pos] = (unsigned char)(rdlen >> 8);
    w.buf[rdlen_pos + 1] = (unsigned char)(rdlen & 0xFF);

    sendto(c->fd, w.buf, w.pos, 0, (struct sockaddr *)&c->dst, sizeof(c->dst));
}

opcua_statuscode_t mu_host_mdns_adapter_init(mu_mdns_adapter_t *adapter) {
    if (adapter == NULL)
        return MU_STATUS_BAD_INTERNALERROR;

    mdns_ctx_t *ctx = (mdns_ctx_t *)calloc(1, sizeof(mdns_ctx_t));
    if (ctx == NULL)
        return MU_STATUS_BAD_OUTOFMEMORY;

    ctx->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->fd < 0) {
        free(ctx);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    int reuse = 1;
    setsockopt(ctx->fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(MDNS_PORT);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(ctx->fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        close(ctx->fd);
        free(ctx);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* Set destination — mDNS multicast group 224.0.0.251:5353 */
    memset(&ctx->dst, 0, sizeof(ctx->dst));
    ctx->dst.sin_family = AF_INET;
    ctx->dst.sin_port = htons(MDNS_PORT);
    ctx->dst.sin_addr.s_addr = htonl(0xE00000FB);

    adapter->context = ctx;
    adapter->publish = mdns_publish_impl;
    adapter->unpublish = mdns_unpublish_impl;
    return MU_STATUS_GOOD;
}

#else /* !(__linux__ || __APPLE__) */

opcua_statuscode_t mu_host_mdns_adapter_init(mu_mdns_adapter_t *adapter) {
    (void)adapter;
    return MU_STATUS_BAD_INTERNALERROR;
}

#endif
