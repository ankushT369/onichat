#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

enum command {
    CMD_GET_USERNAME = 0x01,
    CMD_SEND_MESSAGE = 0x02,
    CMD_GET_STATUS   = 0x03,
    CMD_PING         = 0x04
};

/* register protocol */
struct response {
    uint8_t status;     // 0 = OK, 1 = ERROR
    char data[16];      // optional payload, fixed-size
};

/* message protocol */
typedef struct recv_packet {
    uint8_t status;
    size_t len;
    char* payload;
} r_packet;

typedef struct send_packet {
    size_t usrlen;
    char* username;
    size_t len;
    char* payload;
} s_packet;


#endif // PROTOCOL_H
