#pragma once
#include <stdarg.h>

typedef enum { LOG_ERR=0, LOG_WARN=1, LOG_INFO=2, LOG_DEBUG=3 } log_level_t;

void log_set_level(log_level_t lvl);
void log_printf(log_level_t lvl, const char* fmt, ...);
