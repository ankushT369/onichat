#ifndef MAP_H
#define MAP_H

#include "khash.h"
#include "els.h"

KHASH_MAP_INIT_INT(client, connection*)
KHASH_SET_INIT_STR(strset)

void add_client_map(khash_t(client)* map, connection* c);
connection* get_client_map(khash_t(client)* map, fd_t fd);
void remove_client_map(khash_t(client)* map, fd_t fd);

int set_insert(khash_t(strset)* set, char* s);
int set_find(khash_t(strset)* set, const char* s);
int set_delete(khash_t(strset)* set, const char* s);

#endif // MAP_H
