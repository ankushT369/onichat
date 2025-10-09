#ifndef MESSAGE_H
#define MESSAGE_H

#include <stddef.h>

typedef struct packet {
    char* username;
    size_t usrlen;
    char* msg;
    size_t msglen;
} packet;


#endif // MESSAGE_H
