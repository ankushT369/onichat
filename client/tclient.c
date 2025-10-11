//TODO: change now it doesn't handles the parsing
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "color.h"
#include "tconnect.h"
#include "ini.h"
#include "protocol.h"

#define BUF_SIZE 4096

char display_prompt[64]; // for printing after user hits Enter

char prompt[256];  // buffer to store the dynamic prompt
struct response resp;
pmt_color gcolor;

int sock = -1;

struct client_conf config = {
    .url[0] = '\0',
    .port = 0,
    .persist = false,
    .retry = false,
    .retry_time = 0, 
    .retry_no = 0,
};

/* Thread to continuously receive messages */
void *recv_loop(void *arg) {
    while (1) {
        server_packet pack;
        ssize_t n;
        size_t received = 0;

        /* Read username length */
        received = 0;
        while (received < sizeof(pack.usrlen)) {
            n = read(sock, ((char*)&pack.usrlen) + received, sizeof(pack.usrlen) - received);
            if (n <= 0) {
                printf("\n🔌 Disconnected from server.\n");
                sock = reconnect(config);
                if (sock == -1)
                    exit(0);
                recv_loop(NULL);
                //exit(0);
            }
            received += n;
        }

        /* Read username */
        pack.username = malloc(pack.usrlen + 1);
        if (!pack.username) {
            perror("malloc username");
            exit(1);
        }
        received = 0;
        while (received < pack.usrlen) {
            n = read(sock, pack.username + received, pack.usrlen - received);
            if (n <= 0) {
                printf("\n🔌 Disconnected from server.\n");
                sock = reconnect(config);
                if (sock == -1)
                    exit(0);

                recv_loop(NULL);
            }
            received += n;
        }
        pack.username[pack.usrlen] = '\0';

        /* Read payload length */
        received = 0;
        while (received < sizeof(pack.len)) {
            n = read(sock, ((char*)&pack.len) + received, sizeof(pack.len) - received);
            if (n <= 0) {
                printf("\n🔌 Disconnected from server.\n");
                reconnect(config);
                if (sock == -1)
                    exit(0);
                recv_loop(NULL);
                //exit(0);
            }
            received += n;
        }

        /* Read payload */
        pack.payload = malloc(pack.len + 1);
        if (!pack.payload) {
            perror("malloc payload");
            exit(1);
        }
        received = 0;
        while (received < pack.len) {
            n = read(sock, pack.payload + received, pack.len - received);
            if (n <= 0) {
                printf("\n🔌 Disconnected from server.\n");
                reconnect(config);
                if (sock == -1)
                    exit(0);
                recv_loop(NULL);
                //exit(0);
            }
            received += n;
        }
        pack.payload[pack.len] = '\0';

        char reply_prompt[128];
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char timestr[16];
        sprintf(timestr, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

        /* construct the prompt */
        snprintf(reply_prompt, 128, "[%s %s]", pack.username, timestr);

        /* Display message with username */
        rl_save_prompt();
        rl_replace_line("", 0);
        rl_redisplay();

        printf("%s %s\n", reply_prompt, pack.payload);
        fflush(stdout);

        rl_restore_prompt();
        rl_redisplay();

        free(pack.username);
        free(pack.payload);

    }

    return NULL;
}

int register_user(int sock) {
    uint8_t cmd = CMD_GET_USERNAME;
    write(sock, &cmd, sizeof(cmd));
    read(sock, &resp, sizeof(resp));

    if (resp.status == 0) {
        printf("Server assigned username: %s\n", resp.data);
        return 1;
    }
    else {
        printf("Server ERROR: %s\n", resp.data);
        return -1;
    }
}

void construct_prompt(char* username, pmt_color color) {
    // Construct clean prompt for readline
    snprintf(prompt, sizeof(prompt), "%s[%s]%s ", color.start, username, color.end);
}

void print_with_timestamp(char* username, pmt_color color, const char* line) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char timestr[16];
    sprintf(timestr, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

    // Move cursor up one line and clear it
    printf("\033[F\033[2K");

    // Print line with timestamp
    printf("%s[%s %s]%s %s\n", color.start, username, timestr, color.end, line);
}

size_t send_data(char* data, int sock) {
    packet pack;

    pack.status = CMD_SEND_MESSAGE;
    pack.len = strlen(data);
    pack.payload = malloc(pack.len + 1);

    strncpy(pack.payload, data, pack.len);

    write(sock, &pack.status, sizeof(pack.status));
    write(sock, &pack.len, sizeof(pack.len));

    size_t sent = 0;
    while (sent < pack.len) {
        ssize_t n = write(sock, pack.payload + sent, pack.len - sent);
        if (n <= 0) { perror("write"); exit(1); }
        sent += n;
    }

    return sent;
}

static int handler(void* user, const char* section, const char* name, const char* value) {
    client_conf* c = (client_conf*)user;

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("client", "persist")) {
        c->persist = (strcmp(value, "true") == 0);
    } else if (MATCH("client", "retry")) {
        c->retry = (strcmp(value, "true") == 0);
    } else if (MATCH("client", "retry_time")) {
        c->retry_time = atoi(value);
    } else if (MATCH("client", "retry_no")) {
        c->retry_no = atoi(value);
    } else if (MATCH("client", "url")) {
        strncpy(c->url, value, sizeof(c->url) - 1);
        c->url[sizeof(c->url) - 1] = '\0'; // safety null-terminate
    } else if (MATCH("client", "port")) {
        c->port = (uint16_t)atoi(value);
    } else {
        return 0;  /* unknown section/name, stop parsing */
    }

    return 1; // continue parsing
}

int main(int argc, char *argv[]) {
    if (argc != 1) {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        return 1;
    }

    if (ini_parse("../../config.ini", handler, (void*)&config) < 0) {
        printf("Can't load 'test.ini'\n");
        return 1;
    }

    /* color scheme */
    pmt_color color = rand_color_init();

    char *username = "me";

    sock = connect_via_tor(config);
    if (sock < 0) return 1;

    /* register user will use register protocol */
    if (register_user(sock) == -1) {
        return EXIT_FAILURE;
    }

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, recv_loop, NULL);

    construct_prompt(username, color);  // clean prompt

    char *line;
    while ((line = readline(prompt)) != NULL) {
        if (strlen(line) > 0) {
            add_history(line);

            if (strcmp(line, "/quit") == 0) {
                free(line);
                break;
            }

            // Send message
            size_t n = send_data(line, sock);
            if (n == 0) {

            }

            // Overwrite typed line with timestamped version
            print_with_timestamp(username, color, line);
        }

        free(line);
        // Reset clean prompt for next input
        construct_prompt(username, color);
    }

    close(sock);
    return 0;
}
