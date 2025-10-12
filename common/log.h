#ifndef LOG_H
#define LOG_H

#include <zlog.h>

extern zlog_category_t* server;

int log_init(const char* logconf);

#endif // LOG_H
