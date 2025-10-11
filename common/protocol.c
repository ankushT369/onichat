#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "protocol.h"


packet recv_data(fd_t client_fd) {
    packet pack; 
    
    /* Step 2: read the length */
    size_t received = 0;
    ssize_t n;
    while (received < sizeof(pack.len)) {
        n = read(client_fd, ((char*)&pack.len) + received, sizeof(pack.len) - received);
        if (n <= 0) {
            perror("read len");
            close(client_fd);
            //return;
        }
        received += n;
    }

    /* allocate and read the payload */
    char *payload = malloc(pack.len + 1);
    if (!payload) {
        fprintf(stderr, "malloc failed\n");
        close(client_fd);
        //return;
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
    payload[pack.len] = '\0';  // null-terminate for safety

    pack.payload = payload;
    /* now you can use it */
    printf("[Client %d]: %s\n", client_fd, pack.payload);

    return pack;
}
