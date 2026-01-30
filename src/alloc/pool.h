#pragma once
#include <stdint.h>
#include <netinet/in.h>

// /64 기반 host-range 주소 풀
typedef struct {
  uint32_t pool_id;
  uint32_t subnet_id;

  struct in6_addr prefix64; // upper 64 bits significant
  uint64_t host_start;
  uint64_t host_end;

  uint32_t preferred_lft;
  uint32_t valid_lft;

  uint64_t secret;
} pool64_t;

// PD 풀: base prefix에서 delegated_len 단위로 위임
typedef struct {
  uint32_t pool_id;
  uint32_t subnet_id;

  struct in6_addr base_prefix;
  uint8_t base_len;       // e.g. 40, 48
  uint8_t delegated_len;  // e.g. 56, 60

  uint32_t preferred_lft;
  uint32_t valid_lft;

  uint64_t secret;
} pd_pool_t;

void pool64_make_addr(struct in6_addr* out, const struct in6_addr* prefix64, uint64_t host);
void pdpool_make_prefix(struct in6_addr* out_prefix, const struct in6_addr* base_prefix,
                        uint8_t base_len, uint8_t delegated_len, uint64_t block_index);
