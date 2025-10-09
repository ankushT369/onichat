#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

enum command {
    CMD_GET_USERNAME = 0x01,
    CMD_SET_USERNAME = 0x02,
    CMD_GET_STATUS   = 0x03,
    CMD_PING         = 0x04
};

struct response {
    uint8_t status;     // 0 = OK, 1 = ERROR
    char data[16];      // optional payload, fixed-size
};

#endif // PROTOCOL_H
