#ifndef TCONNECT_H
#define TCONNECT_H

#include <stdbool.h>
#include <stdint.h>

#define TOR_SOCKS_IP "127.0.0.1"
#define TOR_SOCKS_PORT 9050


typedef struct client_conf {
    char url[64];
    uint16_t port;

    bool persist;

    bool retry;

    int retry_time;
    int retry_no;
} client_conf;

int connect_via_tor(const client_conf conf);
int reconnect(const client_conf conf);

#endif // TCONNECT_H
