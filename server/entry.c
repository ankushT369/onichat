#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ini.h"
#include "els.h"
#include "log.h"


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
    /* configure logs */
    if (!log_init("zlog.conf")) {
        fprintf(stderr, "CRITICAL: Failed to initialize logging system\n");
        return EXIT_FAILURE;
    }

    zlog_info(server, "Logging system initialized successfully");

    /* Configure server */
    const els_config config = {
        .host = "127.0.0.1",
        .port = 8080,
        .backlog = 0,
        .max_conn = 0
    };

    if (ini_parse("config.ini", handler, (void*)&config) < 0) {
        zlog_warn(server, "Can't load 'test.ini' default config will be considered\n");
        return 1;
    }
    zlog_info(server, "Parsed configurations.");

    els* e = els_create(&config);
    els_run(e);

    return 0;
}
