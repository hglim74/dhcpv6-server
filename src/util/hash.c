#include "util/hash.h"

#define ROTL(x,b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define U8TO64_LE(p) \
  (((uint64_t)((p)[0]))       | ((uint64_t)((p)[1])<<8)  | ((uint64_t)((p)[2])<<16) | ((uint64_t)((p)[3])<<24) | \
   ((uint64_t)((p)[4])<<32)  | ((uint64_t)((p)[5])<<40) | ((uint64_t)((p)[6])<<48) | ((uint64_t)((p)[7])<<56))

static inline void sipround(uint64_t* v0, uint64_t* v1, uint64_t* v2, uint64_t* v3){
  *v0 += *v1; *v1 = ROTL(*v1,13); *v1 ^= *v0; *v0 = ROTL(*v0,32);
  *v2 += *v3; *v3 = ROTL(*v3,16); *v3 ^= *v2;
  *v0 += *v3; *v3 = ROTL(*v3,21); *v3 ^= *v0;
  *v2 += *v1; *v1 = ROTL(*v1,17); *v1 ^= *v2; *v2 = ROTL(*v2,32);
}

uint64_t siphash24(const uint8_t* data, size_t len, uint64_t k0, uint64_t k1){
  uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
  uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
  uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
  uint64_t v3 = 0x7465646279746573ULL ^ k1;

  const uint8_t* end = data + (len & ~((size_t)7));
  for(const uint8_t* p=data; p!=end; p+=8){
    uint64_t m = U8TO64_LE(p);
    v3 ^= m;
    sipround(&v0,&v1,&v2,&v3);
    sipround(&v0,&v1,&v2,&v3);
    v0 ^= m;
  }

  uint64_t b = ((uint64_t)len) << 56;
  const uint8_t* tail = end;
  switch(len & 7){
    case 7: b |= ((uint64_t)tail[6])<<48;
    case 6: b |= ((uint64_t)tail[5])<<40;
    case 5: b |= ((uint64_t)tail[4])<<32;
    case 4: b |= ((uint64_t)tail[3])<<24;
    case 3: b |= ((uint64_t)tail[2])<<16;
    case 2: b |= ((uint64_t)tail[1])<<8;
    case 1: b |= ((uint64_t)tail[0]);
    default: break;
  }

  v3 ^= b;
  sipround(&v0,&v1,&v2,&v3);
  sipround(&v0,&v1,&v2,&v3);
  v0 ^= b;

  v2 ^= 0xff;
  sipround(&v0,&v1,&v2,&v3);
  sipround(&v0,&v1,&v2,&v3);
  sipround(&v0,&v1,&v2,&v3);
  sipround(&v0,&v1,&v2,&v3);

  return v0 ^ v1 ^ v2 ^ v3;
}

uint64_t hash64_bytes(const uint8_t* data, size_t len, uint64_t seed){
  // convenience wrapper: split seed into keys
  uint64_t k0 = seed ^ 0x9e3779b97f4a7c15ULL;
  uint64_t k1 = (seed<<1) ^ 0xbf58476d1ce4e5b9ULL;
  return siphash24(data, len, k0, k1);
}
