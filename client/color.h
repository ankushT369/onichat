#ifndef COLOR_H
#define COLOR_H

#include <stddef.h>

typedef struct pmt_color {
    char start[64];
    const char* end;
} pmt_color;


typedef struct bg_color {
    int r;
    int g;
    int b;
} bg_color;


pmt_color rand_color_init();
int hex_to_int(const char* hex);
int get_terminal_bg_color(int *r, int *g, int *b);
void random_contrasting_color(char *buf, size_t size, int bg_r, int bg_g, int bg_b);

#endif // COLOR_H
