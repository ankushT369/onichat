#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/ini.h"
#include "els.h"


static int handler(void* user, const char* section, const char* name, const char* value) {
    els_config* pconfig = (els_config*)user;

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("server", "backlog")) {
        pconfig->backlog = atoi(value);
    } else if (MATCH("server", "max_connection")) {
        pconfig->max_conn = atoi(value);
    } else {
        return 0;  /* unknown section/name, stop parsing */
    }

    return 1; // continue parsing
}

int main() {
    /* Configure server */
    const els_config config = {
        .host = "127.0.0.1",
        .port = 8080,
        .backlog = 0,
        .max_conn = 0
    };

    if (ini_parse("../../config.ini", handler, (void*)&config) < 0) {
        printf("Can't load 'test.ini'\n");
        return 1;
    }

    els* e = els_create(&config);
    els_run(e);

    return 0;
}
