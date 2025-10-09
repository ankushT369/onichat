#include <stdbool.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "els.h"
#include "vendor/klib/khash.h"

epoll_event events[MAX_EVENT];
fd_t client_fd_arr[100] = {0};

struct els {
    fd_t server;
    fd_t epfd;
    bool running;

    const char* host;
    uint16_t port;
    int backlog;
    int max_conn;

    address* addr;
};

typedef struct connection {
    fd_t client;
    state c_st;

    const char* username;
    size_t usrlen;
} connection;


KHASH_MAP_INIT_INT(client, connection*);

/* Default configuration */
static const els_config ELS_DEFAULT_CONFIG = {
    .host = NULL,           /* Listen on all interfaces */
    .port = 8080,           /* Default port */
    .backlog = 128,         /* Reasonable backlog */
    .max_conn = 1000        /* Maximum connections */
};

els* els_create(const els_config* config) {
    if (!config)
        config = &ELS_DEFAULT_CONFIG;

    els* e = calloc(1, sizeof(els));
    e->addr = malloc(sizeof(address));

    e->host = config->host;
    e->port = config->port;
    e->backlog = config->backlog;


    struct addrinfo hints = {0}, *res = NULL;
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%u", e->port);

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int ret = getaddrinfo(e->host, port_str, &hints, &res);

    memcpy(&e->addr->addr, res->ai_addr, res->ai_addrlen);
    e->addr->addrlen = res->ai_addrlen;

    fd_t server = socket(res->ai_family, SOCK_STREAM, 0);
    e->server = server;

    int opt = 1;
    setsockopt(e->server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ret = bind(e->server, (const struct sockaddr*)&e->addr->addr, e->addr->addrlen);

    listen(e->server, e->backlog);
    printf("HEY in els.c: %s, %d, %d\n", e->host, e->server, e->backlog);

    /* creates an epoll instance */
    e->epfd = epoll_create1(EPOLL_CLOEXEC);

    /* configure for input events from server fd */
    struct epoll_event event = {
        .events = EPOLLIN,
        .data.fd = e->server
    };

    /* register the server socket to epoll instance */
    epoll_ctl(e->epfd, EPOLL_CTL_ADD, e->server, &event);

    e->running = true;
    freeaddrinfo(res);
    return e;
}

connection* els_accept(els* e, connection** conn) {
    connection* c = malloc(sizeof(connection));
    
    c->client = accept(e->server, (struct sockaddr*)&e->addr->addr, &e->addr->addrlen);
    //client_fd_arr[c->client] = 1;
    c->c_st = C_CON;

    struct epoll_event event = {
        .events = EPOLLIN,
        .data.fd = c->client
    };

    epoll_ctl(e->epfd, EPOLL_CTL_ADD, c->client, &event);

    *conn = c;

    return *conn;
}

void add_client(khash_t(client)* map, connection* c) {
    int ret;
    khiter_t k = kh_put(client, map, c->client, &ret);
    kh_value(map, k) = c;
}

connection* get_client(khash_t(client)* map, fd_t fd) {
    khiter_t k = kh_get(client, map, fd);
    return (k != kh_end(map)) ? kh_value(map, k) : NULL;
}

void send_message(fd_t client_fd, connection *c, const char *msg, size_t msg_len) {
    // Convert lengths to network byte order
    uint32_t username_len_net = htonl(c->usrlen);
    uint32_t msg_len_net = htonl(msg_len);

    // 1️⃣ send username length
    ssize_t sent = 0;
    while (sent < sizeof(username_len_net)) {
        ssize_t s = write(client_fd, ((char*)&username_len_net) + sent, sizeof(username_len_net) - sent);
        if (s <= 0) return; // error or disconnect
        sent += s;
    }

    // 2️⃣ send username
    sent = 0;
    while (sent < c->usrlen) {
        ssize_t s = write(client_fd, c->username + sent, c->usrlen - sent);
        if (s <= 0) return;
        sent += s;
    }

    // 3️⃣ send message length
    sent = 0;
    while (sent < sizeof(msg_len_net)) {
        ssize_t s = write(client_fd, ((char*)&msg_len_net) + sent, sizeof(msg_len_net) - sent);
        if (s <= 0) return;
        sent += s;
    }

    // 4️⃣ send message
    sent = 0;
    while (sent < msg_len) {
        ssize_t s = write(client_fd, msg + sent, msg_len - sent);
        if (s <= 0) return;
        sent += s;
    }
}


void els_run(els* e) {
    khash_t(client) *conn_map = kh_init(client);

    while (e->running) {
        int event_cnt = epoll_wait(e->epfd, events, MAX_EVENT, -1);
        /*
         *  comeback to it later 
         *  if (event_cnt < 0) {
         *      if (stop_server) break; 
         *      else continue; // some other signal 
         *      perror("epoll_wait");
         *      break;
         *  }
        */
        connection *c = NULL;

        for (size_t i = 0; i < (size_t)event_cnt; i++) {
            if (events[i].data.fd == e->server) {
                els_accept(e, &c);
                /* make the wrapper of hashmap here ..... */
                add_client(conn_map, c);
                printf("[+] Client connected\n");
            }
            else {
                /* Handle client */
                char buffer[BUFFER_SIZE];
                int client_fd = events[i].data.fd;
                ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
                
                if (n > 0) {
                    if (client_fd_arr[client_fd] == 0) {
                         printf("first auth: %s\n", buffer);
                         connection* c = get_client(conn_map, client_fd);
                         c->username = strndup(buffer, n);
                         c->usrlen = n;
                         client_fd_arr[client_fd] = 1;
                    }
                    else {
                        buffer[n] = '\0';
                        printf("[Client %d]: %s\n", client_fd, buffer);
                        /* send the msg to other clients */
                        for (fd_t j = 0; j < 100; j++) {
                            if (client_fd_arr[j] == 1 && j != client_fd) {
                                connection* c = get_client(conn_map, j);
                                //printf("len: %zu\n", c->usrlen);
                                //c->username = strndup(buffer, n);
                                write(j, buffer, n);
                                //send_message(j, c, buffer, n);
                            }
                        }
                    }
                }
                else {
                    /* close the client connection */
                    epoll_ctl(e->epfd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    printf("[-] [Client %d] disconnected\n", client_fd);
                }
            }
        }

    }
}
