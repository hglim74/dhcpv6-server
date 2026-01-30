#pragma once
#include <stdint.h>
#include <netinet/in.h>
#include "dhcp/duid.h"
#include "alloc/pool.h"
#include "store/lease_store.h"

// Address allocator
int alloc_addr64(const pool64_t* pool, const duid_t* duid, uint32_t iaid,
                 lease_store_t* st, struct in6_addr* out_addr);

// Prefix allocator
int alloc_prefix_pd(const pd_pool_t* pool, const duid_t* duid, uint32_t iaid,
                    uint8_t hint_len, int has_hint_len,
                    lease_store_t* st, struct in6_addr* out_prefix, uint8_t* out_plen);
