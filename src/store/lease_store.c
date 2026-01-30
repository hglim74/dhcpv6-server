#include "store/lease_store.h"
#include <string.h>

lease_key_t lease_key_make(const duid_t* duid, uint32_t iaid, uint16_t ia_type){
  lease_key_t k;
  k.duid_hash = duid->h;
  k.iaid = iaid;
  k.ia_type = ia_type;
  return k;
}

int in6_equal(const struct in6_addr* a, const struct in6_addr* b){
  return memcmp(a, b, sizeof(*a)) == 0;
}
