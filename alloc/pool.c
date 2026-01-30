#include "alloc/pool.h"
#include <string.h>
#include <arpa/inet.h>

// prefix64: upper 64 bits used, host fills lower 64 bits (network order)
void pool64_make_addr(struct in6_addr* out, const struct in6_addr* prefix64, uint64_t host){
  *out = *prefix64;
  uint8_t* p = out->s6_addr;
  // lower 64 bits set to host (big endian)
  p[8]  = (uint8_t)(host >> 56);
  p[9]  = (uint8_t)(host >> 48);
  p[10] = (uint8_t)(host >> 40);
  p[11] = (uint8_t)(host >> 32);
  p[12] = (uint8_t)(host >> 24);
  p[13] = (uint8_t)(host >> 16);
  p[14] = (uint8_t)(host >> 8);
  p[15] = (uint8_t)(host);
}

// base_prefix/base_len -> delegated_len prefix by setting next bits from block_index
// We treat IPv6 as 128-bit bitstring.
static inline int get_bit(const uint8_t a[16], int bit){
  int byte = bit/8;
  int off = 7 - (bit%8);
  return (a[byte] >> off) & 1;
}
static inline void set_bit(uint8_t a[16], int bit, int v){
  int byte = bit/8;
  int off = 7 - (bit%8);
  if(v) a[byte] |= (1u<<off);
  else  a[byte] &= ~(1u<<off);
}
void pdpool_make_prefix(struct in6_addr* out_prefix, const struct in6_addr* base_prefix,
                        uint8_t base_len, uint8_t delegated_len, uint64_t block_index)
{
  *out_prefix = *base_prefix;

  // fill bits [base_len, delegated_len) from block_index MSB->LSB
  int bits = (int)delegated_len - (int)base_len;
  for(int i=0;i<bits;i++){
    int bitpos = (int)base_len + i;
    int shift = bits - 1 - i;
    int b = (block_index >> shift) & 1;
    set_bit(out_prefix->s6_addr, bitpos, b);
  }
  // zero remaining bits [delegated_len, 128)
  for(int bit=(int)delegated_len; bit<128; bit++){
    set_bit(out_prefix->s6_addr, bit, 0);
  }
}
