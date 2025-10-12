#include "log.h"

zlog_category_t* server = NULL;

int log_init(const char *logconf) {
    if(zlog_init(logconf)) {
        fprintf(stderr, "Failed to initialize zlog\n");
        return 0;
    }

    server = zlog_get_category("server");

    if(!server) {
        fprintf(stderr, "server get catagory failed\n");
        zlog_fini();
        return 0;
    }

    return 1;
}
