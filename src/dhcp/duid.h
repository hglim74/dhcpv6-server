#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
  uint8_t bytes[128];
  uint16_t len;
  uint64_t h;
} duid_t;

int duid_from_opt(duid_t* d, const uint8_t* v, uint16_t vlen, uint64_t seed);
int duid_equal(const duid_t* a, const duid_t* b);
