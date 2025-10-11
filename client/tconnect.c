#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "tconnect.h"


/* Simple SOCKS5 connection through Tor to .onion server */
int connect_via_tor(const client_conf conf) {
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

    /* SOCKS5 handshake */
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

    size_t domain_len = strlen(conf.url);
    req[len++] = (unsigned char)domain_len;
    memcpy(req + len, conf.url, domain_len);
    len += domain_len;

    req[len++] = (conf.port >> 8) & 0xFF;
    req[len++] = conf.port & 0xFF;

    write(sock, req, len);

    unsigned char reply[10];
    if (read(sock, reply, sizeof(reply)) < 4 || reply[1] != 0x00) {
        fprintf(stderr, "SOCKS5 connection failed\n");
        close(sock);
        return -1;
    }

    printf("✅ Connected to %s via Tor!\n", conf.url);
    return sock;
}

int reconnect(const client_conf conf) {
    printf("Reconnecting to %s:%d (retries=%d, delay=%d sec)\n",
           conf.url, conf.port, conf.retry_no, conf.retry_time);

    // Copy config by value (safe since there are no heap pointers)
    client_conf c = conf;

    if (!c.retry) {
        printf("Retry disabled in config.\n");
        return -1;
    }

    for (int attempt = 1; attempt <= c.retry_no; attempt++) {
        printf("Attempt %d/%d...\n", attempt, c.retry_no);
        int sock = connect_via_tor(c);
        if (sock != -1) {
            rl_save_prompt();
            rl_replace_line("", 0);
            rl_redisplay();

            fflush(stdout);

            rl_restore_prompt();
            rl_redisplay();
            return sock;
        }

        printf("Failed, retrying in %d seconds...\n", c.retry_time);
        sleep(c.retry_time);
    }

    printf("❌ All reconnect attempts failed.\n");
    return -1;
}


