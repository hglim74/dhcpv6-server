#define _POSIX_C_SOURCE 200809L

#include "util/log.h"
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

static log_level_t g_lvl = LOG_INFO;

void log_set_level(log_level_t lvl){
  g_lvl = lvl;
}

static const char* lvl_s(log_level_t l){
  switch(l){
    case LOG_ERR: return "ERR";
    case LOG_WARN: return "WRN";
    case LOG_INFO: return "INF";
    case LOG_DEBUG: return "DBG";
    default: return "UNK";
  }
}

void log_printf(log_level_t lvl, const char* fmt, ...){
  if(lvl > g_lvl) return;

  time_t t = time(NULL);
  struct tm tm;
  localtime_r(&t, &tm);

  char tbuf[32];
  strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);

  fprintf(stderr, "%s [%s] ", tbuf, lvl_s(lvl));

  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  fputc('\n', stderr);
}
