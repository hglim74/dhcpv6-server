// src/store/journal.h
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
  int fd;
} journal_t;

int journal_open(journal_t* j, const char* path);
int journal_append(journal_t* j, const void* rec, size_t len);
void journal_close(journal_t* j);
