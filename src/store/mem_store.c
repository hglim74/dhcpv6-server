#include "store/mem_store.h"
#include "util/time.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  int used;
  lease_key_t key;
  lease_na_t na;
} na_slot_t;

typedef struct {
  int used;
  lease_key_t key;
  lease_pd_t pd;
} pd_slot_t;

typedef struct {
  int used;
  struct in6_addr addr;
  lease_key_t key;
} addr_slot_t;

typedef struct {
  int used;
  struct in6_addr prefix;
  uint8_t plen;
  lease_key_t key;
} pfx_slot_t;

typedef struct {
  size_t cap;

  na_slot_t* na;
  pd_slot_t* pd;
  addr_slot_t* addr_idx;
  pfx_slot_t* pfx_idx;

  // decline sets (simple arrays in same hash tables)
  addr_slot_t* declined_addr;
  pfx_slot_t* declined_pfx;

  // store decline until in key.iaid field reuse? -> no, store separate arrays:
  uint64_t* declined_addr_until;
  uint64_t* declined_pfx_until;
} mem_impl_t;

static uint64_t mix64(uint64_t x){
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return x;
}
static uint64_t hash_key(const lease_key_t* k){
  uint64_t x = k->duid_hash ^ (((uint64_t)k->iaid)<<1) ^ ((uint64_t)k->ia_type<<48);
  return mix64(x);
}
static uint64_t hash_in6(const struct in6_addr* a){
  const uint64_t* p = (const uint64_t*)a->s6_addr;
  return mix64(p[0] ^ p[1]);
}
static uint64_t hash_prefix(const struct in6_addr* pfx, uint8_t plen){
  return mix64(hash_in6(pfx) ^ ((uint64_t)plen<<32));
}

static int key_eq(const lease_key_t* a, const lease_key_t* b){
  return a->duid_hash==b->duid_hash && a->iaid==b->iaid && a->ia_type==b->ia_type;
}

static ssize_t find_na(mem_impl_t* m, const lease_key_t* key){
  uint64_t h = hash_key(key);
  for(size_t i=0;i<m->cap;i++){
    size_t idx = (h + i) % m->cap;
    if(!m->na[idx].used) return -1;
    if(key_eq(&m->na[idx].key, key)) return (ssize_t)idx;
  }
  return -1;
}
static ssize_t find_pd(mem_impl_t* m, const lease_key_t* key){
  uint64_t h = hash_key(key);
  for(size_t i=0;i<m->cap;i++){
    size_t idx = (h + i) % m->cap;
    if(!m->pd[idx].used) return -1;
    if(key_eq(&m->pd[idx].key, key)) return (ssize_t)idx;
  }
  return -1;
}
static ssize_t upsert_na(mem_impl_t* m, const lease_na_t* in){
  uint64_t h = hash_key(&in->key);
  for(size_t i=0;i<m->cap;i++){
    size_t idx = (h + i) % m->cap;
    if(!m->na[idx].used || key_eq(&m->na[idx].key, &in->key)){
      m->na[idx].used = 1;
      m->na[idx].key = in->key;
      m->na[idx].na  = *in;
      return (ssize_t)idx;
    }
  }
  return -1;
}
static ssize_t upsert_pd(mem_impl_t* m, const lease_pd_t* in){
  uint64_t h = hash_key(&in->key);
  for(size_t i=0;i<m->cap;i++){
    size_t idx = (h + i) % m->cap;
    if(!m->pd[idx].used || key_eq(&m->pd[idx].key, &in->key)){
      m->pd[idx].used = 1;
      m->pd[idx].key = in->key;
      m->pd[idx].pd  = *in;
      return (ssize_t)idx;
    }
  }
  return -1;
}

static int addr_index_put(mem_impl_t* m, const struct in6_addr* addr, const lease_key_t* key){
  uint64_t h = hash_in6(addr);
  for(size_t i=0;i<m->cap;i++){
    size_t idx = (h + i) % m->cap;
    if(!m->addr_idx[idx].used || in6_equal(&m->addr_idx[idx].addr, addr)){
      m->addr_idx[idx].used = 1;
      m->addr_idx[idx].addr = *addr;
      m->addr_idx[idx].key  = *key;
      return 0;
    }
  }
  return -1;
}
static int addr_index_del(mem_impl_t* m, const struct in6_addr* addr){
  uint64_t h = hash_in6(addr);
  for(size_t i=0;i<m->cap;i++){
    size_t idx = (h + i) % m->cap;
    if(!m->addr_idx[idx].used) return 0;
    if(in6_equal(&m->addr_idx[idx].addr, addr)){
      m->addr_idx[idx].used = 0;
      return 0;
    }
  }
  return 0;
}
static int pfx_index_put(mem_impl_t* m, const struct in6_addr* pfx, uint8_t plen, const lease_key_t* key){
  uint64_t h = hash_prefix(pfx, plen);
  for(size_t i=0;i<m->cap;i++){
    size_t idx = (h + i) % m->cap;
    if(!m->pfx_idx[idx].used ||
       (m->pfx_idx[idx].plen==plen && in6_equal(&m->pfx_idx[idx].prefix, pfx))){
      m->pfx_idx[idx].used = 1;
      m->pfx_idx[idx].prefix = *pfx;
      m->pfx_idx[idx].plen = plen;
      m->pfx_idx[idx].key  = *key;
      return 0;
    }
  }
  return -1;
}
static int pfx_index_del(mem_impl_t* m, const struct in6_addr* pfx, uint8_t plen){
  uint64_t h = hash_prefix(pfx, plen);
  for(size_t i=0;i<m->cap;i++){
    size_t idx = (h + i) % m->cap;
    if(!m->pfx_idx[idx].used) return 0;
    if(m->pfx_idx[idx].plen==plen && in6_equal(&m->pfx_idx[idx].prefix, pfx)){
      m->pfx_idx[idx].used = 0;
      return 0;
    }
  }
  return 0;
}

static int st_get_na(lease_store_t* st, const lease_key_t* key, lease_na_t* out){
  mem_impl_t* m = (mem_impl_t*)st->impl;
  ssize_t idx = find_na(m, key);
  if(idx < 0) return -1;
  *out = m->na[idx].na;
  return 0;
}
static int st_put_na(lease_store_t* st, const lease_na_t* in){
  mem_impl_t* m = (mem_impl_t*)st->impl;
  // update address index
  // delete old addr mapping if key exists and address changes
  lease_na_t old;
  if(st_get_na(st, &in->key, &old) == 0){
    if(!in6_equal(&old.addr, &in->addr)) addr_index_del(m, &old.addr);
  }
  if(upsert_na(m, in) < 0) return -1;
  return addr_index_put(m, &in->addr, &in->key);
}
static int st_del_na(lease_store_t* st, const lease_key_t* key){
  mem_impl_t* m = (mem_impl_t*)st->impl;
  ssize_t idx = find_na(m, key);
  if(idx < 0) return 0;
  addr_index_del(m, &m->na[idx].na.addr);
  m->na[idx].used = 0;
  return 0;
}

static int st_get_pd(lease_store_t* st, const lease_key_t* key, lease_pd_t* out){
  mem_impl_t* m = (mem_impl_t*)st->impl;
  ssize_t idx = find_pd(m, key);
  if(idx < 0) return -1;
  *out = m->pd[idx].pd;
  return 0;
}
static int st_put_pd(lease_store_t* st, const lease_pd_t* in){
  mem_impl_t* m = (mem_impl_t*)st->impl;
  lease_pd_t old;
  if(st_get_pd(st, &in->key, &old) == 0){
    if(old.prefix_len != in->prefix_len || !in6_equal(&old.prefix, &in->prefix)){
      pfx_index_del(m, &old.prefix, old.prefix_len);
    }
  }
  if(upsert_pd(m, in) < 0) return -1;
  return pfx_index_put(m, &in->prefix, in->prefix_len, &in->key);
}
static int st_del_pd(lease_store_t* st, const lease_key_t* key){
  mem_impl_t* m = (mem_impl_t*)st->impl;
  ssize_t idx = find_pd(m, key);
  if(idx < 0) return 0;
  pfx_index_del(m, &m->pd[idx].pd.prefix, m->pd[idx].pd.prefix_len);
  m->pd[idx].used = 0;
  return 0;
}

static int st_addr_in_use(lease_store_t* st, const struct in6_addr* addr){
  mem_impl_t* m = (mem_impl_t*)st->impl;
  uint64_t h = hash_in6(addr);
  for(size_t i=0;i<m->cap;i++){
    size_t idx = (h + i) % m->cap;
    if(!m->addr_idx[idx].used) return 0;
    if(in6_equal(&m->addr_idx[idx].addr, addr)) return 1;
  }
  return 0;
}
static int st_prefix_in_use(lease_store_t* st, const struct in6_addr* pfx, uint8_t plen){
  mem_impl_t* m = (mem_impl_t*)st->impl;
  uint64_t h = hash_prefix(pfx, plen);
  for(size_t i=0;i<m->cap;i++){
    size_t idx = (h + i) % m->cap;
    if(!m->pfx_idx[idx].used) return 0;
    if(m->pfx_idx[idx].plen==plen && in6_equal(&m->pfx_idx[idx].prefix, pfx)) return 1;
  }
  return 0;
}

// declined tables
static int st_is_addr_declined(lease_store_t* st, const struct in6_addr* addr, uint64_t now){
  mem_impl_t* m = (mem_impl_t*)st->impl;
  uint64_t h = hash_in6(addr);
  for(size_t i=0;i<m->cap;i++){
    size_t idx=(h+i)%m->cap;
    if(!m->declined_addr[idx].used) return 0;
    if(in6_equal(&m->declined_addr[idx].addr, addr)){
      return m->declined_addr_until[idx] > now;
    }
  }
  return 0;
}
static int st_is_prefix_declined(lease_store_t* st, const struct in6_addr* pfx, uint8_t plen, uint64_t now){
  mem_impl_t* m = (mem_impl_t*)st->impl;
  uint64_t h = hash_prefix(pfx, plen);
  for(size_t i=0;i<m->cap;i++){
    size_t idx=(h+i)%m->cap;
    if(!m->declined_pfx[idx].used) return 0;
    if(m->declined_pfx[idx].plen==plen && in6_equal(&m->declined_pfx[idx].prefix, pfx)){
      return m->declined_pfx_until[idx] > now;
    }
  }
  return 0;
}
static int st_decline_addr(lease_store_t* st, const struct in6_addr* addr, uint64_t until){
  mem_impl_t* m = (mem_impl_t*)st->impl;
  uint64_t h = hash_in6(addr);
  for(size_t i=0;i<m->cap;i++){
    size_t idx=(h+i)%m->cap;
    if(!m->declined_addr[idx].used || in6_equal(&m->declined_addr[idx].addr, addr)){
      m->declined_addr[idx].used=1;
      m->declined_addr[idx].addr=*addr;
      m->declined_addr_until[idx]=until;
      return 0;
    }
  }
  return -1;
}
static int st_decline_prefix(lease_store_t* st, const struct in6_addr* pfx, uint8_t plen, uint64_t until){
  mem_impl_t* m = (mem_impl_t*)st->impl;
  uint64_t h = hash_prefix(pfx, plen);
  for(size_t i=0;i<m->cap;i++){
    size_t idx=(h+i)%m->cap;
    if(!m->declined_pfx[idx].used ||
       (m->declined_pfx[idx].plen==plen && in6_equal(&m->declined_pfx[idx].prefix, pfx))){
      m->declined_pfx[idx].used=1;
      m->declined_pfx[idx].prefix=*pfx;
      m->declined_pfx[idx].plen=plen;
      m->declined_pfx_until[idx]=until;
      return 0;
    }
  }
  return -1;
}

static void st_gc(lease_store_t* st, uint64_t now){
  mem_impl_t* m = (mem_impl_t*)st->impl;

  // GC NA leases
  for(size_t i=0;i<m->cap;i++){
    if(!m->na[i].used) continue;
    lease_na_t* l = &m->na[i].na;
    if(l->state == LS_OFFERED && l->hold_until <= now){
      addr_index_del(m, &l->addr);
      m->na[i].used = 0;
      continue;
    }
    if(l->state == LS_ALLOCATED && l->valid_until <= now){
      addr_index_del(m, &l->addr);
      m->na[i].used = 0;
      continue;
    }
  }

  // GC PD leases
  for(size_t i=0;i<m->cap;i++){
    if(!m->pd[i].used) continue;
    lease_pd_t* l = &m->pd[i].pd;
    if(l->state == LS_OFFERED && l->hold_until <= now){
      pfx_index_del(m, &l->prefix, l->prefix_len);
      m->pd[i].used = 0;
      continue;
    }
    if(l->state == LS_ALLOCATED && l->valid_until <= now){
      pfx_index_del(m, &l->prefix, l->prefix_len);
      m->pd[i].used = 0;
      continue;
    }
  }

  // decline expiry clear (best-effort)
  for(size_t i=0;i<m->cap;i++){
    if(m->declined_addr[i].used && m->declined_addr_until[i] <= now){
      m->declined_addr[i].used = 0;
    }
    if(m->declined_pfx[i].used && m->declined_pfx_until[i] <= now){
      m->declined_pfx[i].used = 0;
    }
  }
}

int mem_store_init(lease_store_t* st, size_t cap){
  mem_impl_t* m = calloc(1, sizeof(*m));
  if(!m) return -1;
  m->cap = cap;

  m->na = calloc(cap, sizeof(na_slot_t));
  m->pd = calloc(cap, sizeof(pd_slot_t));
  m->addr_idx = calloc(cap, sizeof(addr_slot_t));
  m->pfx_idx  = calloc(cap, sizeof(pfx_slot_t));

  m->declined_addr = calloc(cap, sizeof(addr_slot_t));
  m->declined_pfx  = calloc(cap, sizeof(pfx_slot_t));
  m->declined_addr_until = calloc(cap, sizeof(uint64_t));
  m->declined_pfx_until  = calloc(cap, sizeof(uint64_t));
  if(!m->na || !m->pd || !m->addr_idx || !m->pfx_idx ||
     !m->declined_addr || !m->declined_pfx || !m->declined_addr_until || !m->declined_pfx_until){
    free(m->na); free(m->pd); free(m->addr_idx); free(m->pfx_idx);
    free(m->declined_addr); free(m->declined_pfx);
    free(m->declined_addr_until); free(m->declined_pfx_until);
    free(m);
    return -1;
  }

  st->impl = m;
  st->v.get_na = st_get_na;
  st->v.put_na = st_put_na;
  st->v.del_na = st_del_na;
  st->v.get_pd = st_get_pd;
  st->v.put_pd = st_put_pd;
  st->v.del_pd = st_del_pd;
  st->v.addr_in_use = st_addr_in_use;
  st->v.prefix_in_use = st_prefix_in_use;
  st->v.is_addr_declined = st_is_addr_declined;
  st->v.is_prefix_declined = st_is_prefix_declined;
  st->v.decline_addr = st_decline_addr;
  st->v.decline_prefix = st_decline_prefix;
  st->v.gc = st_gc;
  return 0;
}

void mem_store_free(lease_store_t* st){
  mem_impl_t* m = (mem_impl_t*)st->impl;
  if(!m) return;
  free(m->na); free(m->pd); free(m->addr_idx); free(m->pfx_idx);
  free(m->declined_addr); free(m->declined_pfx);
  free(m->declined_addr_until); free(m->declined_pfx_until);
  free(m);
  st->impl = NULL;
}
