#ifndef LOG_H
#define LOG_H

#ifndef __ANDROID__
#include <zlog.h>

extern zlog_category_t* server;

int log_init(const char* logconf);

#endif // __ANDROID__

#endif // LOG_H
