#pragma once
#include <stdint.h>
#include <netinet/in.h>
#include "dhcp/duid.h"

typedef enum { IA_NA=3, IA_PD=25 } ia_type_t;
typedef enum { LS_OFFERED=1, LS_ALLOCATED=2, LS_DECLINED=3 } lease_state_t;

typedef struct {
  uint64_t duid_hash;
  uint32_t iaid;
  uint16_t ia_type;
} lease_key_t;

typedef struct {
  lease_key_t key;
  struct in6_addr addr;
  uint32_t preferred_lft, valid_lft;
  uint64_t preferred_until, valid_until;
  uint32_t subnet_id, pool_id;
  lease_state_t state;
  uint64_t hold_until; // for offered/declined TTL
} lease_na_t;

typedef struct {
  lease_key_t key;
  struct in6_addr prefix;
  uint8_t prefix_len;
  uint32_t preferred_lft, valid_lft;
  uint64_t preferred_until, valid_until;
  uint32_t subnet_id, pool_id;
  lease_state_t state;
  uint64_t hold_until;
} lease_pd_t;

typedef struct lease_store lease_store_t;

typedef struct {
  int (*get_na)(lease_store_t*, const lease_key_t*, lease_na_t* out);
  int (*put_na)(lease_store_t*, const lease_na_t* in);
  int (*del_na)(lease_store_t*, const lease_key_t*);

  int (*get_pd)(lease_store_t*, const lease_key_t*, lease_pd_t* out);
  int (*put_pd)(lease_store_t*, const lease_pd_t* in);
  int (*del_pd)(lease_store_t*, const lease_key_t*);

  int (*addr_in_use)(lease_store_t*, const struct in6_addr*);
  int (*prefix_in_use)(lease_store_t*, const struct in6_addr*, uint8_t plen);

  int (*is_addr_declined)(lease_store_t*, const struct in6_addr*, uint64_t now);
  int (*is_prefix_declined)(lease_store_t*, const struct in6_addr*, uint8_t plen, uint64_t now);

  int (*decline_addr)(lease_store_t*, const struct in6_addr*, uint64_t until);
  int (*decline_prefix)(lease_store_t*, const struct in6_addr*, uint8_t plen, uint64_t until);

  void (*gc)(lease_store_t*, uint64_t now);
} lease_store_vtbl_t;

struct lease_store {
  lease_store_vtbl_t v;
  void* impl;
};

lease_key_t lease_key_make(const duid_t* duid, uint32_t iaid, uint16_t ia_type);
int in6_equal(const struct in6_addr* a, const struct in6_addr* b);
