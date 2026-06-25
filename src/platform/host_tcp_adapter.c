/* src/platform/host_tcp_adapter.c */
#include "host_tcp_adapter.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>

struct host_tcp_context {
    int server_fd;
};

static opcua_statuscode_t host_listen(void *context, const char *endpoint_url) {
    struct host_tcp_context *ctx = (struct host_tcp_context *)context;
    
    int port = 4840;
    
    /* Naive extraction of port from "opc.tcp://host:port" */
    const char *port_str = strrchr(endpoint_url, ':');
    if (port_str && port_str != endpoint_url + 4) { /* Skip 'opc.tcp://' */
        port = atoi(port_str + 1);
    }
    
    ctx->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->server_fd < 0) return MU_STATUS_BAD_INTERNALERROR;
    
    int opt = 1;
    setsockopt(ctx->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    int flags = fcntl(ctx->server_fd, F_GETFL, 0);
    fcntl(ctx->server_fd, F_SETFL, flags | O_NONBLOCK);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(ctx->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(ctx->server_fd);
        ctx->server_fd = -1;
        return MU_STATUS_BAD_TCPENDPOINTURLINVALID;
    }
    
    if (listen(ctx->server_fd, 5) < 0) {
        close(ctx->server_fd);
        ctx->server_fd = -1;
        return MU_STATUS_BAD_INTERNALERROR;
    }
    
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t host_accept(void *context, void **connection_handle) {
    struct host_tcp_context *ctx = (struct host_tcp_context *)context;
    if (ctx->server_fd < 0) return MU_STATUS_BAD_INTERNALERROR;
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(ctx->server_fd, (struct sockaddr *)&client_addr, &addr_len);
    
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *connection_handle = NULL;
            return MU_STATUS_GOOD;
        }
        return MU_STATUS_BAD_INTERNALERROR;
    }
    
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    *connection_handle = (void *)(intptr_t)client_fd;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t host_read(void *context, void *connection_handle, opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_read) {
    (void)context;
    int fd = (int)(intptr_t)connection_handle;
    
    ssize_t res = read(fd, buffer, buffer_size);
    if (res < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *bytes_read = 0;
            return MU_STATUS_GOOD;
        }
        return MU_STATUS_BAD_COMMUNICATIONERROR;
    } else if (res == 0) {
        return MU_STATUS_BAD_COMMUNICATIONERROR;
    }
    
    *bytes_read = (size_t)res;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t host_write(void *context, void *connection_handle, const opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_written) {
    (void)context;
    int fd = (int)(intptr_t)connection_handle;
    
    ssize_t res = write(fd, buffer, buffer_size);
    if (res < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *bytes_written = 0;
            return MU_STATUS_GOOD;
        }
        return MU_STATUS_BAD_COMMUNICATIONERROR;
    }
    
    *bytes_written = (size_t)res;
    return MU_STATUS_GOOD;
}

static void host_close_connection(void *context, void *connection_handle) {
    (void)context;
    int fd = (int)(intptr_t)connection_handle;
    if (fd >= 0) {
        close(fd);
    }
}

static void host_shutdown(void *context) {
    struct host_tcp_context *ctx = (struct host_tcp_context *)context;
    if (ctx && ctx->server_fd >= 0) {
        close(ctx->server_fd);
        ctx->server_fd = -1;
    }
    free(ctx);
}

opcua_statuscode_t mu_host_tcp_adapter_init(mu_tcp_adapter_t *adapter) {
    if (!adapter) return MU_STATUS_BAD_INTERNALERROR;
    
    struct host_tcp_context *ctx = malloc(sizeof(struct host_tcp_context));
    if (!ctx) return MU_STATUS_BAD_OUTOFMEMORY;
    
    ctx->server_fd = -1;
    
    adapter->context = ctx;
    adapter->listen = host_listen;
    adapter->accept = host_accept;
    adapter->read = host_read;
    adapter->write = host_write;
    adapter->close_connection = host_close_connection;
    adapter->shutdown = host_shutdown;
    
    return MU_STATUS_GOOD;
}
