// src/store/journal.c
#include "store/journal.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int journal_open(journal_t* j, const char* path){
  j->fd = open(path, O_CREAT|O_APPEND|O_WRONLY, 0644);
  return (j->fd < 0) ? -1 : 0;
}
int journal_append(journal_t* j, const void* rec, size_t len){
  if(j->fd < 0) return -1;
  ssize_t n = write(j->fd, rec, len);
  return (n == (ssize_t)len) ? 0 : -1;
}
void journal_close(journal_t* j){
  if(j->fd >= 0) close(j->fd);
  j->fd = -1;
}
