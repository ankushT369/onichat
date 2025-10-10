#include "map.h"

void add_client_map(khash_t(client)* map, connection* c) {
    int ret;
    khiter_t k = kh_put(client, map, c->client, &ret);
    kh_value(map, k) = c;
}

connection* get_client_map(khash_t(client)* map, fd_t fd) {
    khiter_t k = kh_get(client, map, fd);
    return (k != kh_end(map)) ? kh_value(map, k) : NULL;
}

void remove_client_map(khash_t(client)* map, fd_t fd) {
    khiter_t k = kh_get(client, map, fd);
    if (k != kh_end(map)) {
        kh_del(client, map, k);
    }
}

int set_insert(khash_t(strset)* set, char* s) {
    int ret;
    kh_put(strset, set, s, &ret);
    //khiter_t k = 
    if (ret == 0) {
        // already exists, do NOT free your pointer
        return 0;
    }
    return 1;  // inserted successfully
}

int set_find(khash_t(strset)* set, const char* s) {
    khiter_t k = kh_get(strset, set, s);
    return (k != kh_end(set));
}

int set_delete(khash_t(strset)* set, const char* s) {
    khiter_t k = kh_get(strset, set, s);
    if (k == kh_end(set)) return 0;  // not found

    /* IMP: don't uncomment this */
    //free((char*)kh_key(set, k));      // free the memory you passed
    kh_del(strset, set, k);
    return 1;
}
