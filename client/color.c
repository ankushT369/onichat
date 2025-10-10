#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "color.h"

bg_color bg = { .r = 0, .g = 0, .b = 0};

/* Helper: convert hex string to int */
int hex_to_int(const char* hex) {
    int val;
    sscanf(hex, "%x", &val);
    return val;
}

/* Query terminal background color using OSC 11 */
int get_terminal_bg_color(int *r, int *g, int *b) {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    /* Send OSC 11 query */
    printf("\033]11;?\007");
    fflush(stdout);

    /* Read response */
    char buf[128] = {0};
    long unsigned int i = 0;
    char c;
    /* Wait for response: format is ESC ] 11 ; rgb:XXXX/YYYY/ZZZZ BEL */
    while (i < sizeof(buf)-1 && read(STDIN_FILENO, &c, 1) == 1) {
        buf[i++] = c;
        if (c == '\007') break; // BEL ends the sequence
    }
    buf[i] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    /* Parse rgb:XXXX/YYYY/ZZZZ */
    char *p = strstr(buf, "rgb:");
    if (!p) return 0;

    char rstr[5], gstr[5], bstr[5];
    if (sscanf(p, "rgb:%4[^/]/%4[^/]/%4s", rstr, gstr, bstr) != 3) return 0;

    *r = hex_to_int(rstr);
    *g = hex_to_int(gstr);
    *b = hex_to_int(bstr);

    return 1;
}

/* Generate random RGB contrasting with background */
void random_contrasting_color(char *buf, size_t size, int bg_r, int bg_g, int bg_b) {
    int r, g, b;
    double bg_brightness = 0.299*bg_r + 0.587*bg_g + 0.114*bg_b;
    double brightness;

    do {
        r = rand() % 256;
        g = rand() % 256;
        b = rand() % 256;
        brightness = 0.299*r + 0.587*g + 0.114*b;
    } while (fabs(brightness - bg_brightness) < 100); // ensure good contrast

    snprintf(buf, size, "\001\033[38;2;%d;%d;%dm\002", r, g, b);
}

/* init color scheme */
pmt_color rand_color_init() {
    /* color scheme */
    srand(time(NULL));
    pmt_color color;

    if (!get_terminal_bg_color(&bg.r, &bg.g, &bg.b)) {
        // fallback: assume dark background
        bg.r = bg.g = bg.b = 0;
    }

    random_contrasting_color(color.start, sizeof(color.start), bg.r, bg.g, bg.b);
    color.end = "\001\033[0m\002";

    return color;
}
