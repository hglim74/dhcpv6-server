#include "dhcp/duid.h"
#include "util/hash.h"
#include <string.h>

int duid_from_opt(duid_t* d, const uint8_t* v, uint16_t vlen, uint64_t seed){
  if(vlen == 0 || vlen > sizeof(d->bytes)) return -1;
  memcpy(d->bytes, v, vlen);
  d->len = vlen;
  d->h = hash64_bytes(d->bytes, d->len, seed);
  return 0;
}

int duid_equal(const duid_t* a, const duid_t* b){
  if(a->len != b->len) return 0;
  if(a->h != b->h) return 0; // fast reject (collision extremely unlikely but possible)
  return memcmp(a->bytes, b->bytes, a->len) == 0;
}
