#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>

#include "tconnect.h"


/* Simple SOCKS5 connection through Tor to .onion server */
int connect_via_tor(const char *onion_addr, int dest_port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in tor_addr = {0};
    tor_addr.sin_family = AF_INET;
    tor_addr.sin_port = htons(TOR_SOCKS_PORT);
    inet_pton(AF_INET, TOR_SOCKS_IP, &tor_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&tor_addr, sizeof(tor_addr)) < 0) {
        perror("connect to tor");
        close(sock);
        return -1;
    }

    /* 1. SOCKS5 handshake */
    unsigned char handshake[] = {0x05, 0x01, 0x00};  // ver=5, nmethods=1, method=no auth
    write(sock, handshake, sizeof(handshake));

    unsigned char resp[2];
    if (read(sock, resp, 2) != 2 || resp[0] != 0x05 || resp[1] != 0x00) {
        fprintf(stderr, "SOCKS5 handshake failed\n");
        close(sock);
        return -1;
    }

    /* Request Tor to connect to .onion:port */
    unsigned char req[1024];
    size_t len = 0;

    req[len++] = 0x05;  // version
    req[len++] = 0x01;  // cmd = connect
    req[len++] = 0x00;  // reserved
    req[len++] = 0x03;  // atyp = domain name

    size_t domain_len = strlen(onion_addr);
    req[len++] = (unsigned char)domain_len;
    memcpy(req + len, onion_addr, domain_len);
    len += domain_len;

    req[len++] = (dest_port >> 8) & 0xFF;
    req[len++] = dest_port & 0xFF;

    write(sock, req, len);

    unsigned char reply[10];
    if (read(sock, reply, sizeof(reply)) < 4 || reply[1] != 0x00) {
        fprintf(stderr, "SOCKS5 connection failed\n");
        close(sock);
        return -1;
    }

    printf("✅ Connected to %s via Tor!\n", onion_addr);
    return sock;
}
