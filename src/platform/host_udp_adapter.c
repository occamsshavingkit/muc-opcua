#include "muc_opcua/platform.h"
#include "muc_opcua/status.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

/* We just store the socket FD in the context directly to keep it simple,
 * or use an intptr_t. Let's assume context points to an int. */

opcua_statuscode_t mu_host_udp_init(void *context, uint16_t port) {
    (void)port;
    if (!context) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    int broadcastEnable = 1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (const void *)&broadcastEnable, sizeof(broadcastEnable));

    *(int *)context = fd;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_host_udp_send(void *context, const opcua_byte_t *buffer, size_t buffer_size, const char *address,
                                    uint16_t port) {
    if (!context)
        return MU_STATUS_BAD_INTERNALERROR;
    int fd = *(int *)context;
    if (fd < 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    struct sockaddr_in dest_addr;
    (void)memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

#ifdef _WIN32
    dest_addr.sin_addr.s_addr = inet_addr(address);
#else
    inet_pton(AF_INET, address, &dest_addr.sin_addr);
#endif

    sendto(fd, (const char *)buffer, buffer_size, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    return MU_STATUS_GOOD;
}

void mu_host_udp_shutdown(void *context) {
    if (context) {
        int fd = *(int *)context;
        if (fd >= 0) {
#ifdef _WIN32
            closesocket(fd);
            WSACleanup();
#else
            close(fd);
#endif
            *(int *)context = -1;
        }
    }
}
