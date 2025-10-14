#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>

#include "protocol.h"
#include "log.h"


packet recv_data(fd_t client_fd) {
    packet pack; 
    /* read the length */
    size_t received = 0;
    ssize_t n;
    while (received < sizeof(pack.len)) {
        n = read(client_fd, ((char*)&pack.len) + received, sizeof(pack.len) - received);
        if (n <= 0) {
            perror("read len");
            close(client_fd);
            return pack;
        }
        received += n;
    }

    /* allocate and read the payload */
    if (pack.len <= 0 || pack.len > MAX_PAYLOAD) {
        zlog_warn(server, "invalid packet length: client_fd=%d len=%zu", client_fd, pack.len);
        pack.len = 0;
        close(client_fd);
        return pack;
    }

    char *payload = malloc(pack.len + 1);
    if (!payload) {
        fprintf(stderr, "malloc failed\n");
        close(client_fd);
        return pack;
    }

    received = 0;
    while (received < pack.len) {
        n = read(client_fd, payload + received, pack.len - received);
        if (n <= 0) {
            perror("read payload");
            free(payload);
            close(client_fd);
            //return;
        }
        received += n;
    }
    pack.len = (received == pack.len) ? pack.len : received;
    payload[pack.len] = '\0';  // null-terminate for safety

    pack.payload = payload;

    return pack;
}
