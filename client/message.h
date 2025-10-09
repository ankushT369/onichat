#ifndef MESSAGE_H
#define MESSAGE_H

#include <stddef.h>
#include <stdint.h>

/* messaging protocol */
typedef struct packet {
    char* username;
    size_t usrlen;
    char* msg;
    size_t msglen;
} packet;


enum command {
    CMD_GET_USERNAME = 0x01,
    CMD_SET_USERNAME = 0x02,
    CMD_GET_STATUS   = 0x03,
    CMD_PING         = 0x04
};

struct response {
    uint8_t status;    // 0 = OK, 1 = ERROR
    char data[16];      // optional payload, fixed-size
};


#endif // MESSAGE_H
