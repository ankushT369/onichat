#include <stdbool.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "els.h"
#include "log.h"

#define MAX_CLIENT 100000

epoll_event events[MAX_EVENT];
fd_t client_fd_arr[MAX_CLIENT] = {0};

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

/* Default configuration */
static const els_config ELS_DEFAULT_CONFIG = {
    .host = NULL,           /* Listen on all interfaces */
    .port = 8080,           /* Default port */
    .backlog = 128,         /* Reasonable backlog */
    .max_conn = 1000        /* Maximum connections */
};

static inline server_packet prepare_send_packet(connection* conn, packet rpack) {
    server_packet packet = { 
        .usrlen = conn->usrlen,
        .username = conn->username,
        .len = rpack.len,
        .payload = rpack.payload
    };

    return packet; 
}

/* TODO: Handle freeing memory */
els* els_create(const els_config* config) {
    if (!config) {
        zlog_warn(server, "Configuration file not found!! loaded with default configuration\n");
        config = &ELS_DEFAULT_CONFIG;
    }

    els* e = calloc(1, sizeof(els));
    e->addr = malloc(sizeof(address));
    if (!e || !e->addr) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

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
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    memcpy(&e->addr->addr, res->ai_addr, res->ai_addrlen);
    e->addr->addrlen = res->ai_addrlen;

    fd_t server_fd = socket(res->ai_family, SOCK_STREAM, 0);
    e->server = server_fd;

    int opt = 1;
    ret = setsockopt(e->server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret != 0) {
        fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    ret = bind(e->server, (const struct sockaddr*)&e->addr->addr, e->addr->addrlen);
    if (ret != 0) {
        fprintf(stderr, "bind failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }


    ret = listen(e->server, e->backlog);
    if (ret != 0) {
        fprintf(stderr, "listen failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* creates an epoll instance */
    e->epfd = epoll_create1(EPOLL_CLOEXEC);
    if (e->epfd == -1) {
        fprintf(stderr, "epoll_create1 failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* configure for input events from server fd */
    struct epoll_event event = {
        .events = EPOLLIN,
        .data.fd = e->server
    };

    /* register the server socket to epoll instance */
    ret = epoll_ctl(e->epfd, EPOLL_CTL_ADD, e->server, &event);
    if (ret == -1) {
        fprintf(stderr, "epoll_ctl failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    e->running = true;
    freeaddrinfo(res);

    zlog_info(server, "Server started successfully: listening on %s:%u, backlog=%d",
        e->host, e->port, e->backlog);
    return e;
}

void els_destroy(els* e) {
    close(e->server);
}

void get_username(khash_t(strset)* set, connection* c) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static int initialized = 0;
    if (!initialized) {
        srand(time(NULL));  // seed random once
        initialized = 1;
    }

    int unique = 0;
    while (!unique) {
        /* generate random length 5–15 */
        int len = 5 + rand() % 11;  // 5..15

        /* fill the buffer */
        for (int i = 0; i < len; i++) {
            c->username[i] = charset[rand() % (sizeof(charset) - 1)];
        }
        c->username[len] = '\0';
        c->usrlen = len;

        /* check uniqueness */
        if (!set_find(set, c->username)) {
            set_insert(set, c->username);  // store pointer in the set
            unique = 1;
        }
    }
}

void els_accept(els* e, connection** conn) {
    connection* c = malloc(sizeof(connection));
    
    c->client = accept(e->server, (struct sockaddr*)&e->addr->addr, &e->addr->addrlen);
    if (c->client == -1) {
        fprintf(stderr, "accept failed: %s\n", strerror(errno));
        zlog_warn(server, "Unable to accept the client");
        free(c);
        *conn = NULL;
    }

    epoll_event event = {
        .events = EPOLLIN,
        .data.fd = c->client
    };

    int ret = epoll_ctl(e->epfd, EPOLL_CTL_ADD, c->client, &event);
    if (ret == -1) {
        if (errno == EEXIST) {
            // maybe it's already registered, try modifying instead
            epoll_ctl(e->epfd, EPOLL_CTL_MOD, c->client, &event);
        } else {
            fprintf(stderr, "epoll_ctl failed: %s\n", strerror(errno));
            close(c->client);
            free(c);
            *conn = NULL;
        }
    }

    *conn = c;
}

void els_cleanup(fd_t client_fd, khash_t(strset) *h, khash_t(client)* conn_map, fd_t epfd) {
    /* close the client connection */
    client_fd_arr[client_fd] = 0;
    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);

    connection* c = get_client_map(conn_map, client_fd);
    set_delete(h, c->username);
    remove_client_map(conn_map, client_fd);

    free(c);
    zlog_info(server, "[Clinet: %d] disconnected", client_fd);
    close(client_fd);
}

void els_broadcast(fd_t client_fd, server_packet packet) {
    for (fd_t j = 0; j < MAX_CLIENT; j++) {
        if (client_fd_arr[j] == 1 && j != client_fd) {
            write(j, &packet.usrlen, sizeof(packet.usrlen));
            write(j, packet.username, packet.usrlen);

            write(j, &packet.len, sizeof(packet.len));
            size_t sent = 0;
            while (sent < packet.len) {
                ssize_t n = write(j, packet.payload + sent, packet.len - sent);
                if (n <= 0) { perror("write"); break; }
                sent += n;
            }
        }
    }
}

void els_run(els* e) {
    khash_t(client) *conn_map = kh_init(client);
    khash_t(strset) *h = kh_init(strset);

    while (e->running) {
        int event_cnt = epoll_wait(e->epfd, events, MAX_EVENT, -1);
        
        if (event_cnt < 0) {
            //if (stop_server) break; 
            //else continue; // some other signal 
            perror("epoll_wait");
            break;
        }

        connection *c = NULL;

        for (size_t i = 0; i < (size_t)event_cnt; i++) {
            if (events[i].data.fd == e->server) {
                els_accept(e, &c);
                if (c != NULL) {
                    zlog_info(server, "Client connected");
                }
                add_client_map(conn_map, c);
            }
            else {
                /* Handle client */
                uint8_t status;
                int client_fd = events[i].data.fd;
                ssize_t n = read(client_fd, &status, sizeof(status));

                if (n > 0) {
                    /* register protocol */ 
                    if (status == CMD_GET_USERNAME && client_fd_arr[client_fd] == 0) {
                        connection* c = get_client_map(conn_map, client_fd);
                        /* generate random user name which doesnt exist in server */
                        get_username(h, c);
                        
                        struct response resp;
                        resp.status = 0;
                        strncpy(resp.data, c->username, c->usrlen);
                        resp.data[sizeof(resp.data) - 1] = '\0';

                        client_fd_arr[client_fd] = 1;

                        write(client_fd, &resp, sizeof(resp));
                    }
                    /* message protocol */
                    else {
                        packet rpack = recv_data(client_fd);
                        /* check validity and clean the memeory */
                        if (rpack.len == 0) {
                            els_cleanup(client_fd, h, conn_map, e->epfd);
                            continue;
                        }
                        connection* s_conn = get_client_map(conn_map, client_fd);
                        server_packet spack = prepare_send_packet(s_conn, rpack);

                        /* NOTE: Later to be optimise */
                        els_broadcast(client_fd, spack);
                    }
                }
                else {
                    /* close the client connection */
                    els_cleanup(client_fd, h, conn_map, e->epfd);
                }
            }
        }

    }
}
