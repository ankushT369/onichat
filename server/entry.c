#include <stdio.h>

#include "els.h"

int main() {
    /* Configure server */
    const els_config config = {
        .host = "127.0.0.1",
        .port = 8080,
        .backlog = 128
    };

    els* e = els_create(&config);
    els_run(e);

    return 0;
}
