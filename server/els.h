#ifndef ELS_H
#define ELS_H

#include <stdint.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#define MAX_EVENT 100
#define BUFFER_SIZE 4096

typedef int fd_t;
typedef struct address address;
typedef struct epoll_event epoll_event;

typedef struct els els;

typedef enum client_state {
    C_DIS,
    C_CON,
} state;

typedef struct els_config {
    const char* host;
    uint16_t port;
    int backlog;
    int max_conn;
} els_config;


typedef struct connection {
    fd_t client;
    state c_st;

    char username[16];
    size_t usrlen;
} connection;


struct address {
    struct sockaddr_storage addr;
    socklen_t addrlen;
};


els* els_create(const els_config* config);
void els_destroy(els* e);

connection* els_accept(els* e, connection** conn);

void els_run(els*);


#endif // EPS_H
