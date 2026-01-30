#pragma once
#include <stdint.h>
#include <stddef.h>

uint64_t siphash24(const uint8_t* data, size_t len, uint64_t k0, uint64_t k1);
uint64_t hash64_bytes(const uint8_t* data, size_t len, uint64_t seed);
