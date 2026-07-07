/* src/platform/posix_client_transport.c */
#include "muc_opcua/client.h"

#ifdef MUC_OPCUA_IS_HOST
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static opcua_statuscode_t parse_endpoint(const char *endpoint_url, char *host, size_t host_size, char *port,
                                         size_t port_size) {
    const char *prefix = "opc.tcp://";
    size_t prefix_len = strlen(prefix);
    if (endpoint_url == NULL || strncmp(endpoint_url, prefix, prefix_len) != 0) {
        return MU_STATUS_BAD_TCPENDPOINTURLINVALID;
    }
    const char *host_start = endpoint_url + prefix_len;
    const char *port_start = strrchr(host_start, ':');
    if (port_start == NULL || port_start == host_start) {
        return MU_STATUS_BAD_TCPENDPOINTURLINVALID;
    }
    size_t host_len = (size_t)(port_start - host_start);
    if (host_len == 0 || host_len >= host_size || strlen(port_start + 1) >= port_size) {
        return MU_STATUS_BAD_TCPENDPOINTURLINVALID;
    }
    (void)memcpy(host, host_start, host_len);
    host[host_len] = '\0';
    (void)strncpy(port, port_start + 1, port_size - 1);
    port[port_size - 1] = '\0';
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t posix_connect(void *context, const char *endpoint_url, opcua_uint32_t timeout_ms) {
    (void)timeout_ms;
    mu_client_posix_transport_t *ctx = (mu_client_posix_transport_t *)context;
    char host[128];
    char port[16];
    opcua_statuscode_t st = parse_endpoint(endpoint_url, host, sizeof(host), port, sizeof(port));
    if (st != MU_STATUS_GOOD) {
        return st;
    }

    struct addrinfo hints;
    struct addrinfo *result = NULL;
    (void)memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &result) != 0) {
        return MU_STATUS_BAD_NOTFOUND;
    }

    int fd = -1;
    for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        (void)close(fd);
        fd = -1;
    }
    freeaddrinfo(result);
    if (fd < 0) {
        return MU_STATUS_BAD_CONNECTIONCLOSED;
    }
    ctx->fd = fd;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t posix_send(void *context, const opcua_byte_t *buffer, size_t length, size_t *bytes_sent,
                                     opcua_uint32_t timeout_ms) {
    (void)timeout_ms;
    mu_client_posix_transport_t *ctx = (mu_client_posix_transport_t *)context;
    if (ctx == NULL || ctx->fd < 0 || buffer == NULL || bytes_sent == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    ssize_t written = send(ctx->fd, buffer, length, 0);
    if (written < 0) {
        return errno == EPIPE ? MU_STATUS_BAD_CONNECTIONCLOSED : MU_STATUS_BAD_TCPINTERNALERROR;
    }
    *bytes_sent = (size_t)written;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t posix_recv(void *context, opcua_byte_t *buffer, size_t capacity, size_t *bytes_read,
                                     opcua_uint32_t timeout_ms) {
    mu_client_posix_transport_t *ctx = (mu_client_posix_transport_t *)context;
    if (ctx == NULL || ctx->fd < 0 || buffer == NULL || bytes_read == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(ctx->fd, &rfds);
    struct timeval tv;
    tv.tv_sec = (long)(timeout_ms / 1000u);
    tv.tv_usec = (long)((timeout_ms % 1000u) * 1000u);
    int ready = select(ctx->fd + 1, &rfds, NULL, NULL, &tv);
    if (ready == 0) {
        *bytes_read = 0;
        return MU_STATUS_GOOD;
    }
    if (ready < 0) {
        return MU_STATUS_BAD_TCPINTERNALERROR;
    }
    ssize_t got = recv(ctx->fd, buffer, capacity, 0);
    if (got == 0) {
        return MU_STATUS_BAD_CONNECTIONCLOSED;
    }
    if (got < 0) {
        return MU_STATUS_BAD_TCPINTERNALERROR;
    }
    *bytes_read = (size_t)got;
    return MU_STATUS_GOOD;
}

static void posix_close(void *context) {
    mu_client_posix_transport_t *ctx = (mu_client_posix_transport_t *)context;
    if (ctx != NULL && ctx->fd >= 0) {
        (void)close(ctx->fd);
        ctx->fd = -1;
    }
}

opcua_statuscode_t mu_client_posix_transport_init(mu_client_transport_t *transport,
                                                  mu_client_posix_transport_t *storage) {
    if (transport == NULL || storage == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    storage->fd = -1;
    transport->context = storage;
    transport->connect = posix_connect;
    transport->send = posix_send;
    transport->recv = posix_recv;
    transport->close = posix_close;
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_IS_HOST */
