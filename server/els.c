#include <stdbool.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "els.h"
#include "map.h"
#include "protocol.h"


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
    if (ret == 0) {
        //
    }

    memcpy(&e->addr->addr, res->ai_addr, res->ai_addrlen);
    e->addr->addrlen = res->ai_addrlen;

    fd_t server = socket(res->ai_family, SOCK_STREAM, 0);
    e->server = server;

    int opt = 1;
    setsockopt(e->server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ret = bind(e->server, (const struct sockaddr*)&e->addr->addr, e->addr->addrlen);

    listen(e->server, e->backlog);
    printf("server is running on: %s, %d, %d\n", e->host, e->server, e->backlog);

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

void get_username(khash_t(strset)* set, connection* c) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static int initialized = 0;
    if (!initialized) {
        srand(time(NULL));  // seed random once
        initialized = 1;
    }

    int unique = 0;
    while (!unique) {
        /* generate random length 6–16 */
        int len = 6 + rand() % 11;  // 6..16

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

r_packet recv_data(fd_t client_fd) {
    r_packet packet; 
    
    /* Step 2: read the length */
    size_t received = 0;
    ssize_t n;
    while (received < sizeof(packet.len)) {
        n = read(client_fd, ((char*)&packet.len) + received, sizeof(packet.len) - received);
        if (n <= 0) {
            perror("read len");
            close(client_fd);
            //return;
        }
        received += n;
    }

    /* allocate and read the payload */
    char *payload = malloc(packet.len + 1);
    if (!payload) {
        fprintf(stderr, "malloc failed\n");
        close(client_fd);
        //return;
    }

    received = 0;
    while (received < packet.len) {
        n = read(client_fd, payload + received, packet.len - received);
        if (n <= 0) {
            perror("read payload");
            free(payload);
            close(client_fd);
            //return;
        }
        received += n;
    }
    payload[packet.len] = '\0';  // null-terminate for safety

    packet.payload = payload;
    /* now you can use it */
    printf("[Client %d]: %s\n", client_fd, packet.payload);

    return packet;
}

void els_run(els* e) {
    khash_t(client) *conn_map = kh_init(client);
    khash_t(strset) *h = kh_init(strset);

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
                add_client_map(conn_map, c);
                printf("[+] Client connected\n");
            }
            else {
                /* Handle client */
                uint8_t status;
                ssize_t n = 0;
                int client_fd = events[i].data.fd;
                n = read(client_fd, &status, sizeof(status));

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
                    else {
                        r_packet rpck = recv_data(client_fd);

                        connection* s_conn = get_client_map(conn_map, client_fd);
                        s_packet spck;
                        spck.usrlen = s_conn->usrlen;
                        spck.username = s_conn->username;
                        spck.len = rpck.len;
                        spck.payload = rpck.payload;

                        for (fd_t j = 0; j < 100; j++) {
                            if (client_fd_arr[j] == 1 && j != client_fd) {
                                write(j, &spck.usrlen, sizeof(spck.usrlen));
                                write(j, spck.username, spck.usrlen);

                                write(j, &spck.len, sizeof(spck.len));
                                size_t sent = 0;
                                while (sent < spck.len) {
                                    ssize_t n = write(j, spck.payload + sent, spck.len - sent);
                                    if (n <= 0) { perror("write"); break; }
                                    sent += n;
                                }
                            }
                        }

                    }
                    /* message protocol
                    else {
                        buffer[n] = '\0';
                        printf("[Client %d]: %s\n", client_fd, buffer);
                        // send the msg to other clients use brute force for now later optimize it
                        for (fd_t j = 0; j < 100; j++) {
                            if (client_fd_arr[j] == 1 && j != client_fd) {
                                get_client_map(conn_map, j);
                                write(j, buffer, n);
                            }
                        }
                    }
                    */

                }
                else {
                    /* close the client connection */
                    client_fd_arr[client_fd] = 0;
                    epoll_ctl(e->epfd, EPOLL_CTL_DEL, client_fd, NULL);

                    connection* c = get_client_map(conn_map, client_fd);
                    set_delete(h, c->username);
                    remove_client_map(conn_map, client_fd);

                    free(c);

                    close(client_fd);
                    printf("[-] [Client %d] disconnected\n", client_fd);
                }
            }
        }

    }
}
