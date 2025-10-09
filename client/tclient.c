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

char prompt[256];  // buffer to store the dynamic prompt
struct response resp;

/* Thread to continuously receive messages */
void *recv_loop(void *arg) {
    int sock = *(int *)arg;
    char buf[BUF_SIZE];

    while (1) {
        ssize_t n = read(sock, buf, sizeof(buf) - 1);
        if (n <= 0) {
            printf("\n🔌 Disconnected from server.\n");
            exit(0);
        }

        buf[n] = '\0';

        /* Temporarily clear the current input line drawn by readline */
        rl_save_prompt();
        rl_replace_line("", 0);
        rl_redisplay();

        /* Print the received message cleanly on a new line */
        printf("\n💬 %s\n", buf);
        fflush(stdout);

        /* Redraw the prompt and whatever user was typing */
        rl_restore_prompt();
        rl_redisplay();
    }

    return NULL;
}

int register_user(int sock) {
    uint8_t cmd = CMD_GET_USERNAME;

    ssize_t n = write(sock, &cmd, sizeof(cmd));
    n = read(sock, &resp, sizeof(resp));

    if (resp.status == 0) {
        printf("Server OK: %s\n", resp.data);
        return 1;
    }
    else {
        printf("Server ERROR: %s\n", resp.data);
        return -1;
    }
}

void construct_prompt(char* username, pmt_color color) {
    /* get current time */
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char timestr[16];
    sprintf(timestr, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

    /* construct the prompt */
    snprintf(prompt, sizeof(prompt), "%s[%s %s]%s ", color.start, username, timestr, color.end);

}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <onion_address> <port> <username>\n", argv[0]);
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

    /* register user will use register user protocol */
    if (register_user(sock) == -1) {
        return EXIT_FAILURE;
    }

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, recv_loop, &sock);

    construct_prompt(username, color);

    char *line;
    while ((line = readline(prompt)) != NULL) {
        construct_prompt(username, color);

        if (strlen(line) == 0) {
            free(line);
            continue;
        }
        add_history(line);

        if (strcmp(line, "/quit") == 0) {
            free(line);
            break;
        }

        write(sock, line, strlen(line));
        free(line);
    }

    close(sock);
    return 0;
}
