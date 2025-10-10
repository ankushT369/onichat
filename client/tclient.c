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
#include "message.h"

#define BUF_SIZE 4096

char display_prompt[64]; // for printing after user hits Enter

char prompt[256];  // buffer to store the dynamic prompt
struct response resp;
pmt_color gcolor;

/* Thread to continuously receive messages */
void *recv_loop(void *arg) {
    int sock = *(int *)arg;

    while (1) {
        r_packet packet;
        ssize_t n;
        size_t received = 0;

        /* Read username length */
        received = 0;
        while (received < sizeof(packet.usrlen)) {
            n = read(sock, ((char*)&packet.usrlen) + received, sizeof(packet.usrlen) - received);
            if (n <= 0) {
                printf("\n🔌 Disconnected from server.\n");
                exit(0);
            }
            received += n;
        }

        /* Read username */
        packet.username = malloc(packet.usrlen + 1);
        if (!packet.username) {
            perror("malloc username");
            exit(1);
        }
        received = 0;
        while (received < packet.usrlen) {
            n = read(sock, packet.username + received, packet.usrlen - received);
            if (n <= 0) {
                printf("\n🔌 Disconnected from server.\n");
                exit(0);
            }
            received += n;
        }
        packet.username[packet.usrlen] = '\0';

        /* Read payload length */
        received = 0;
        while (received < sizeof(packet.len)) {
            n = read(sock, ((char*)&packet.len) + received, sizeof(packet.len) - received);
            if (n <= 0) {
                printf("\n🔌 Disconnected from server.\n");
                exit(0);
            }
            received += n;
        }

        /* Read payload */
        packet.payload = malloc(packet.len + 1);
        if (!packet.payload) {
            perror("malloc payload");
            exit(1);
        }
        received = 0;
        while (received < packet.len) {
            n = read(sock, packet.payload + received, packet.len - received);
            if (n <= 0) {
                printf("\n🔌 Disconnected from server.\n");
                exit(0);
            }
            received += n;
        }
        packet.payload[packet.len] = '\0';

        char reply_prompt[128];
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char timestr[16];
        sprintf(timestr, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

        /* construct the prompt */
        snprintf(reply_prompt, 128, "[%s %s]", packet.username, timestr);

        /* Display message with username */
        rl_save_prompt();
        rl_replace_line("", 0);
        rl_redisplay();

        printf("%s %s\n", reply_prompt, packet.payload);
        fflush(stdout);

        rl_restore_prompt();
        rl_redisplay();

        free(packet.username);
        free(packet.payload);
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
    s_packet packet;

    packet.status = CMD_SEND_MESSAGE;
    packet.len = strlen(data);
    packet.payload = malloc(packet.len + 1);

    strncpy(packet.payload, data, packet.len);

    write(sock, &packet.status, sizeof(packet.status));
    write(sock, &packet.len, sizeof(packet.len));

    size_t sent = 0;
    while (sent < packet.len) {
        ssize_t n = write(sock, packet.payload + sent, packet.len - sent);
        if (n <= 0) { perror("write"); exit(1); }
        sent += n;
    }

    return sent;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <onion_address> <port>\n", argv[0]);
        return 1;
    }

    /* color scheme */
    pmt_color color = rand_color_init();

    /* tor parsing */
    const char *onion = argv[1];
    int port = atoi(argv[2]);
    char *username = "me";

    int sock = connect_via_tor(onion, port);
    if (sock < 0) return 1;

    /* register user will use register protocol */
    if (register_user(sock) == -1) {
        return EXIT_FAILURE;
    }

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, recv_loop, &sock);

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
