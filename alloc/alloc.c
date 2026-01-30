#include "alloc/alloc.h"
#include "util/hash.h"
#include "util/time.h"

static uint64_t h64_duid_iaid(const duid_t* duid, uint32_t iaid, uint64_t secret){
  uint64_t h = hash64_bytes(duid->bytes, duid->len, secret);
  h ^= ((uint64_t)iaid << 32) | (uint64_t)iaid;
  return h;
}

int alloc_addr64(const pool64_t* pool, const duid_t* duid, uint32_t iaid,
                 lease_store_t* st, struct in6_addr* out_addr)
{
  uint64_t now = now_epoch_sec();

  uint64_t seed = h64_duid_iaid(duid, iaid, pool->secret);
  uint64_t range = (pool->host_end - pool->host_start) + 1;

  for(uint64_t probe=0; probe<1024 && probe<range; probe++){
    uint64_t host = pool->host_start + ((seed + probe) % range);
    struct in6_addr cand;
    pool64_make_addr(&cand, &pool->prefix64, host);

    if(st->v.is_addr_declined(st, &cand, now)) continue;
    if(st->v.addr_in_use(st, &cand)) continue;

    *out_addr = cand;
    return 0;
  }
  return -1;
}

int alloc_prefix_pd(const pd_pool_t* pool, const duid_t* duid, uint32_t iaid,
                    uint8_t hint_len, int has_hint_len,
                    lease_store_t* st, struct in6_addr* out_prefix, uint8_t* out_plen)
{
  uint64_t now = now_epoch_sec();

  uint8_t plen = pool->delegated_len;
  // minimal “RFC-friendly” hint handling: accept only if matches our supported len
  if(has_hint_len && hint_len == pool->delegated_len){
    plen = hint_len;
  }

  int bits = (int)plen - (int)pool->base_len;
  if(bits <= 0 || bits > 63) return -1; // keep within uint64_t indexing for minimal impl

  uint64_t blocks = 1ULL << bits;
  uint64_t seed = h64_duid_iaid(duid, iaid, pool->secret) % blocks;

  for(uint64_t probe=0; probe<1024 && probe<blocks; probe++){
    uint64_t idx = (seed + probe) % blocks;
    struct in6_addr cand;
    pdpool_make_prefix(&cand, &pool->base_prefix, pool->base_len, plen, idx);

    if(st->v.is_prefix_declined(st, &cand, plen, now)) continue;
    if(st->v.prefix_in_use(st, &cand, plen)) continue;

    *out_prefix = cand;
    *out_plen = plen;
    return 0;
  }
  return -1;
}
