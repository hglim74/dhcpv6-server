#pragma once
#include <stdint.h>
#include <stddef.h>
#include <netinet/in.h>
#include "dhcp/msg.h"
#include "dhcp/duid.h"
#include "store/lease_store.h"
#include "alloc/pool.h"

typedef struct {
  duid_t server_duid;
  uint64_t duid_seed; // for hashing duid from option

  // per-interface policy (minimal: single policy)
  pool64_t na_pool;
  pd_pool_t pd_pool;

  struct in6_addr dns[2];
  size_t dns_cnt;

  // lifetimes policy (can override pool fields if desired)
  uint32_t preferred_lft;
  uint32_t valid_lft;

  // OFFERED TTL and DECLINED quarantine
  uint32_t offer_ttl;      // seconds
  uint32_t decline_ttl;    // seconds

  lease_store_t* store;
} server_ctx_t;

// handle one packet; returns 1 if response produced, 0 if ignore/drop, <0 on error.
int dh6_handle_packet(server_ctx_t* sctx,
                      const uint8_t* in, size_t in_len,
                      const struct sockaddr_in6* peer, int ifindex,
                      uint8_t* out, size_t out_cap, size_t* out_len,
                      struct sockaddr_in6* out_peer, int* out_ifindex);
