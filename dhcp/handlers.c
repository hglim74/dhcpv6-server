#include "dhcp/handlers.h"
#include "dhcp/opt.h"
#include "util/buf.h"
#include "util/time.h"
#include "util/log.h"
#include "alloc/alloc.h"
#include <string.h>
#include <arpa/inet.h>

typedef struct {
  dh6_hdr_t hdr;

  duid_t client_id;
  int has_client;

  duid_t server_id;
  int has_server;

  uint16_t oro[32];
  size_t oro_cnt;

  // Rapid Commit (Option 14)
  int has_rapid_commit;

  // IA_NA request
  int has_ia_na;
  uint32_t na_iaid;
  int has_na_addr_hint;
  struct in6_addr na_addr_hint;

  // IA_PD request
  int has_ia_pd;
  uint32_t pd_iaid;
  int has_pd_hint_len;
  uint8_t pd_hint_len;
  int has_pd_hint_prefix;
  struct in6_addr pd_hint_prefix;
} req_t;

static int oro_wants(const req_t* r, uint16_t code){
  for(size_t i=0;i<r->oro_cnt;i++) if(r->oro[i]==code) return 1;
  return 0;
}

static int parse_ia_na(req_t* rq, const uint8_t* v, uint16_t vlen){
  rd_t r = rd_make(v, vlen);
  if(vlen < 12) return -1;
  uint32_t iaid, t1, t2;
  if(rd_u32(&r, &iaid)<0 || rd_u32(&r, &t1)<0 || rd_u32(&r, &t2)<0) return -1;
  (void)t1; (void)t2;

  rq->has_ia_na = 1;
  rq->na_iaid = iaid;

  while(r.off < r.n){
    dh6_opt_view_t ov;
    int rc = dh6_opt_next(&r, &ov);
    if(rc <= 0) break;
    if(ov.code == OPT_IAADDR && ov.vlen >= 16+4+4){
      memcpy(&rq->na_addr_hint, ov.val, 16);
      rq->has_na_addr_hint = 1;
    }
  }
  return 0;
}

static int parse_ia_pd(req_t* rq, const uint8_t* v, uint16_t vlen){
  rd_t r = rd_make(v, vlen);
  if(vlen < 12) return -1;
  uint32_t iaid, t1, t2;
  if(rd_u32(&r, &iaid)<0 || rd_u32(&r, &t1)<0 || rd_u32(&r, &t2)<0) return -1;
  (void)t1; (void)t2;

  rq->has_ia_pd = 1;
  rq->pd_iaid = iaid;

  while(r.off < r.n){
    dh6_opt_view_t ov;
    int rc = dh6_opt_next(&r, &ov);
    if(rc <= 0) break;
    if(ov.code == OPT_IAPREFIX && ov.vlen >= 4+4+1+16){
      // preferred/valid ignored for request hint
      uint8_t plen = ov.val[8];
      rq->has_pd_hint_len = 1;
      rq->pd_hint_len = plen;
      memcpy(&rq->pd_hint_prefix, ov.val+9, 16);
      rq->has_pd_hint_prefix = 1;
    }
  }
  return 0;
}

static int parse_req(server_ctx_t* s, const uint8_t* pkt, size_t len, req_t* rq){
  memset(rq, 0, sizeof(*rq));
  rd_t body;
  if(dh6_parse_hdr(pkt, len, &rq->hdr, &body) < 0) return -1;

  dh6_opt_view_t ov;
  while(1){
    int rc = dh6_opt_next(&body, &ov);
    if(rc < 0) return -1;
    if(rc == 0) break;

    switch(ov.code){
      case OPT_CLIENTID:
        if(duid_from_opt(&rq->client_id, ov.val, ov.vlen, s->duid_seed)==0) rq->has_client=1;
        break;
      case OPT_SERVERID:
        if(duid_from_opt(&rq->server_id, ov.val, ov.vlen, s->duid_seed)==0) rq->has_server=1;
        break;
      case OPT_ORO:
        if(ov.vlen % 2 == 0){
          size_t n = ov.vlen/2;
          if(n > 32) n = 32;
          for(size_t i=0;i<n;i++){
            rq->oro[i] = (uint16_t)(ov.val[2*i]<<8 | ov.val[2*i+1]);
          }
          rq->oro_cnt = n;
        }
        break;
      case OPT_RAPID_COMMIT:
        rq->has_rapid_commit = 1;
        break;
      case OPT_IA_NA:
        parse_ia_na(rq, ov.val, ov.vlen);
        break;
      case OPT_IA_PD:
        parse_ia_pd(rq, ov.val, ov.vlen);
        break;
      default:
        break;
    }
  }

  if(!rq->has_client) return -2;
  return 0;
}

static void calc_t1_t2(uint32_t valid, uint32_t* t1, uint32_t* t2){
  // RFC-ish defaults
  *t1 = (uint32_t)(valid / 2);
  *t2 = (uint32_t)((valid * 8) / 10);
  if(*t2 <= *t1) *t2 = *t1 + 1;
}

static int write_duid_opt(wr_t* w, uint16_t code, const duid_t* d){
  opt_mark_t m;
  if(opt_begin(w, code, &m)<0) return -1;
  if(wr_bytes(w, d->bytes, d->len)<0) return -1;
  if(opt_end(w, &m)<0) return -1;
  return 0;
}

static int write_dns_if_requested(server_ctx_t* s, const req_t* rq, wr_t* w){
  if(s->dns_cnt == 0) return 0;
  if(!oro_wants(rq, OPT_DNS)) return 0;
  opt_mark_t m;
  if(opt_begin(w, OPT_DNS, &m)<0) return -1;
  for(size_t i=0;i<s->dns_cnt;i++){
    if(wr_bytes(w, (const uint8_t*)&s->dns[i], 16)<0) return -1;
  }
  if(opt_end(w, &m)<0) return -1;
  return 0;
}

static int write_status_in_ia(wr_t* w, uint16_t status_code){
  // Status Code option: code(13), len, status(2), msg(n)
  // Keep msg empty for minimal.
  opt_mark_t m;
  if(opt_begin(w, OPT_STATUS, &m)<0) return -1;
  if(wr_u16(w, status_code)<0) return -1;
  // no message
  if(opt_end(w, &m)<0) return -1;
  return 0;
}

static int write_ia_na(wr_t* w, const req_t* rq, const lease_na_t* na, int ok, uint16_t fail_status){
  opt_mark_t m;
  if(opt_begin(w, OPT_IA_NA, &m)<0) return -1;

  uint32_t t1=0,t2=0;
  if(na) calc_t1_t2(na->valid_lft, &t1, &t2);

  uint32_t iaid = rq->na_iaid;
  if(wr_u32(w, iaid)<0) return -1;
  if(wr_u32(w, t1)<0) return -1;
  if(wr_u32(w, t2)<0) return -1;

  if(ok && na){
    // IAADDR
    opt_mark_t m2;
    if(opt_begin(w, OPT_IAADDR, &m2)<0) return -1;
    if(wr_bytes(w, (const uint8_t*)&na->addr, 16)<0) return -1;
    if(wr_u32(w, na->preferred_lft)<0) return -1;
    if(wr_u32(w, na->valid_lft)<0) return -1;
    if(opt_end(w, &m2)<0) return -1;
  }else{
    if(write_status_in_ia(w, fail_status)<0) return -1;
  }

  if(opt_end(w, &m)<0) return -1;
  return 0;
}

static int write_ia_pd(wr_t* w, const req_t* rq, const lease_pd_t* pd, int ok, uint16_t fail_status){
  opt_mark_t m;
  if(opt_begin(w, OPT_IA_PD, &m)<0) return -1;

  uint32_t t1=0,t2=0;
  if(pd) calc_t1_t2(pd->valid_lft, &t1, &t2);

  uint32_t iaid = rq->pd_iaid;
  if(wr_u32(w, iaid)<0) return -1;
  if(wr_u32(w, t1)<0) return -1;
  if(wr_u32(w, t2)<0) return -1;

  if(ok && pd){
    // IAPREFIX
    opt_mark_t m2;
    if(opt_begin(w, OPT_IAPREFIX, &m2)<0) return -1;
    if(wr_u32(w, pd->preferred_lft)<0) return -1;
    if(wr_u32(w, pd->valid_lft)<0) return -1;
    if(wr_u8(w, pd->prefix_len)<0) return -1;
    if(wr_bytes(w, (const uint8_t*)&pd->prefix, 16)<0) return -1;
    if(opt_end(w, &m2)<0) return -1;
  }else{
    if(write_status_in_ia(w, fail_status)<0) return -1;
  }

  if(opt_end(w, &m)<0) return -1;
  return 0;
}

static int serverid_is_ours(server_ctx_t* s, const duid_t* req_sid){
  return duid_equal(&s->server_duid, req_sid);
}

static void init_na_lease_defaults(server_ctx_t* s, lease_na_t* l, lease_key_t key){
  memset(l, 0, sizeof(*l));
  l->key = key;
  l->preferred_lft = s->preferred_lft ? s->preferred_lft : s->na_pool.preferred_lft;
  l->valid_lft = s->valid_lft ? s->valid_lft : s->na_pool.valid_lft;
  l->subnet_id = s->na_pool.subnet_id;
  l->pool_id = s->na_pool.pool_id;
}

static void init_pd_lease_defaults(server_ctx_t* s, lease_pd_t* l, lease_key_t key){
  memset(l, 0, sizeof(*l));
  l->key = key;
  l->preferred_lft = s->preferred_lft ? s->preferred_lft : s->pd_pool.preferred_lft;
  l->valid_lft = s->valid_lft ? s->valid_lft : s->pd_pool.valid_lft;
  l->subnet_id = s->pd_pool.subnet_id;
  l->pool_id = s->pd_pool.pool_id;
}

static int prefix_match_bits(const struct in6_addr* a, const struct in6_addr* b, uint8_t plen){
  // returns 1 if first plen bits match
  const uint8_t* pa = a->s6_addr;
  const uint8_t* pb = b->s6_addr;

  uint8_t full = plen / 8;
  uint8_t rem  = plen % 8;

  if(full > 0 && memcmp(pa, pb, full) != 0) return 0;
  if(rem == 0) return 1;

  uint8_t mask = (uint8_t)(0xFF << (8 - rem));
  return (pa[full] & mask) == (pb[full] & mask);
}

int dh6_handle_packet(server_ctx_t* sctx,
                      const uint8_t* in, size_t in_len,
                      const struct sockaddr_in6* peer, int ifindex,
                      uint8_t* out, size_t out_cap, size_t* out_len,
                      struct sockaddr_in6* out_peer, int* out_ifindex)
{
  req_t rq;
  int prc = parse_req(sctx, in, in_len, &rq);
  if(prc < 0) return 0;

  uint64_t now = now_epoch_sec();
  sctx->store->v.gc(sctx->store, now);

  // ===== 3) Server-ID rules (RFC-faithful) =====
  // - REQUEST/RENEW/RELEASE: if Server-ID present and not ours -> IGNORE
  // - SOLICIT/REBIND/CONFIRM/INFOREQ: ignore Server-ID
  switch(rq.hdr.msg_type){
    case DHCP6_REQUEST:
    case DHCP6_RENEW:
    case DHCP6_RELEASE:
      if(rq.has_server && !serverid_is_ours(sctx, &rq.server_id)){
        return 0;
      }
      break;
    case DHCP6_SOLICIT:
    case DHCP6_REBIND:
    case DHCP6_CONFIRM:
    case DHCP6_INFOREQ:
    case DHCP6_DECLINE:
      // ignore server-id for minimal (DECLINE handling is by key/hint)
      break;
    default:
      break;
  }

  // ===== response type selection =====
  uint8_t resp_type = 0;
  switch(rq.hdr.msg_type){
    case DHCP6_SOLICIT:
      // ===== 1) Rapid Commit =====
      resp_type = rq.has_rapid_commit ? DHCP6_REPLY : DHCP6_ADVERTISE;
      break;

    case DHCP6_CONFIRM:
    case DHCP6_REQUEST:
    case DHCP6_RENEW:
    case DHCP6_REBIND:
    case DHCP6_INFOREQ:
    case DHCP6_RELEASE:
    case DHCP6_DECLINE:
      resp_type = DHCP6_REPLY;
      break;

    default:
      return 0;
  }

  // Determine whether we should allocate/renew bindings
  int wants_ia = (rq.hdr.msg_type != DHCP6_INFOREQ);

  // ===== 2) CONFIRM =====
  // CONFIRM must NOT allocate; it only validates "on-link"
  if(rq.hdr.msg_type == DHCP6_CONFIRM){
    wants_ia = 0;
  }

  // Prepare NA/PD results
  lease_na_t na = {0};
  lease_pd_t pd = {0};
  int na_ok = 0, pd_ok = 0;

  // Status codes: SUCCESS(0), NoAddrsAvail(2), NotOnLink(6)
  uint16_t na_fail = 2; // NoAddrsAvail
  uint16_t pd_fail = 2; // NoAddrsAvail (works for PD too in minimal interoperable deployments)

  // ===== CONFIRM on-link evaluation =====
  int na_onlink = 0;
  int pd_onlink = 0;
  if(rq.hdr.msg_type == DHCP6_CONFIRM){
    if(rq.has_ia_na && rq.has_na_addr_hint){
      // on-link if addr matches our /64 prefix (minimal policy)
      struct in6_addr masked = rq.na_addr_hint;
      memset(&masked.s6_addr[8], 0, 8);
      if(memcmp(&masked, &sctx->na_pool.prefix64, 16) == 0){
        na_onlink = 1;
      }
    }
    if(rq.has_ia_pd && rq.has_pd_hint_prefix){
      // on-link if requested prefix is within our base_prefix/base_len
      if(prefix_match_bits(&rq.pd_hint_prefix, &sctx->pd_pool.base_prefix, sctx->pd_pool.base_len)){
        pd_onlink = 1;
      }
    }
  }

  // ===== Normal processing (alloc/renew) =====
  if(wants_ia){
    // IA_NA
    if(rq.has_ia_na){
      lease_key_t key = lease_key_make(&rq.client_id, rq.na_iaid, IA_NA);
      init_na_lease_defaults(sctx, &na, key);

      lease_na_t existing;
      if(sctx->store->v.get_na(sctx->store, &key, &existing) == 0){
        if((existing.state == LS_OFFERED && existing.hold_until > now) ||
           (existing.state == LS_ALLOCATED && existing.valid_until > now)){
          na = existing;
          na_ok = 1;
        }
      }

      if(!na_ok){
        struct in6_addr addr;
        if(alloc_addr64(&sctx->na_pool, &rq.client_id, rq.na_iaid, sctx->store, &addr) == 0){
          na.addr = addr;
          na.preferred_until = now + na.preferred_lft;
          na.valid_until = now + na.valid_lft;

          if(rq.hdr.msg_type == DHCP6_SOLICIT){
            if(rq.has_rapid_commit){
              // Rapid Commit -> commit immediately
              na.state = LS_ALLOCATED;
              na.hold_until = 0;
            }else{
              na.state = LS_OFFERED;
              na.hold_until = now + sctx->offer_ttl;
            }
          }else if(rq.hdr.msg_type == DHCP6_REQUEST || rq.hdr.msg_type == DHCP6_RENEW || rq.hdr.msg_type == DHCP6_REBIND){
            na.state = LS_ALLOCATED;
            na.hold_until = 0;
          }

          if(rq.hdr.msg_type==DHCP6_SOLICIT || rq.hdr.msg_type==DHCP6_REQUEST ||
             rq.hdr.msg_type==DHCP6_RENEW || rq.hdr.msg_type==DHCP6_REBIND){
            sctx->store->v.put_na(sctx->store, &na);
            na_ok = 1;
          }
        }
      }
    }

    // IA_PD
    if(rq.has_ia_pd){
      lease_key_t key = lease_key_make(&rq.client_id, rq.pd_iaid, IA_PD);
      init_pd_lease_defaults(sctx, &pd, key);

      lease_pd_t existing;
      if(sctx->store->v.get_pd(sctx->store, &key, &existing) == 0){
        if((existing.state == LS_OFFERED && existing.hold_until > now) ||
           (existing.state == LS_ALLOCATED && existing.valid_until > now)){
          pd = existing;
          pd_ok = 1;
        }
      }

      if(!pd_ok){
        struct in6_addr pfx;
        uint8_t plen;
        int has_hint_len = rq.has_pd_hint_len;
        uint8_t hint_len = rq.pd_hint_len;

        if(alloc_prefix_pd(&sctx->pd_pool, &rq.client_id, rq.pd_iaid,
                           hint_len, has_hint_len,
                           sctx->store, &pfx, &plen) == 0){
          pd.prefix = pfx;
          pd.prefix_len = plen;
          pd.preferred_until = now + pd.preferred_lft;
          pd.valid_until = now + pd.valid_lft;

          if(rq.hdr.msg_type == DHCP6_SOLICIT){
            if(rq.has_rapid_commit){
              pd.state = LS_ALLOCATED;
              pd.hold_until = 0;
            }else{
              pd.state = LS_OFFERED;
              pd.hold_until = now + sctx->offer_ttl;
            }
          }else if(rq.hdr.msg_type == DHCP6_REQUEST || rq.hdr.msg_type == DHCP6_RENEW || rq.hdr.msg_type == DHCP6_REBIND){
            pd.state = LS_ALLOCATED;
            pd.hold_until = 0;
          }

          if(rq.hdr.msg_type==DHCP6_SOLICIT || rq.hdr.msg_type==DHCP6_REQUEST ||
             rq.hdr.msg_type==DHCP6_RENEW || rq.hdr.msg_type==DHCP6_REBIND){
            sctx->store->v.put_pd(sctx->store, &pd);
            pd_ok = 1;
          }
        }
      }
    }
  }

  // RELEASE
  if(rq.hdr.msg_type == DHCP6_RELEASE){
    if(rq.has_ia_na){
      lease_key_t k = lease_key_make(&rq.client_id, rq.na_iaid, IA_NA);
      sctx->store->v.del_na(sctx->store, &k);
    }
    if(rq.has_ia_pd){
      lease_key_t k = lease_key_make(&rq.client_id, rq.pd_iaid, IA_PD);
      sctx->store->v.del_pd(sctx->store, &k);
    }
  }

  // DECLINE (quarantine)
  if(rq.hdr.msg_type == DHCP6_DECLINE){
    uint64_t until = now + sctx->decline_ttl;
    if(rq.has_ia_na && rq.has_na_addr_hint){
      sctx->store->v.decline_addr(sctx->store, &rq.na_addr_hint, until);
      lease_key_t k = lease_key_make(&rq.client_id, rq.na_iaid, IA_NA);
      sctx->store->v.del_na(sctx->store, &k);
    }
    if(rq.has_ia_pd && rq.has_pd_hint_prefix && rq.has_pd_hint_len){
      sctx->store->v.decline_prefix(sctx->store, &rq.pd_hint_prefix, rq.pd_hint_len, until);
      lease_key_t k = lease_key_make(&rq.client_id, rq.pd_iaid, IA_PD);
      sctx->store->v.del_pd(sctx->store, &k);
    }
  }

  // Build response
  wr_t w = wr_make(out, out_cap);
  if(dh6_write_hdr(&w, resp_type, rq.hdr.txid) < 0) return -1;

  // MUST include ServerID + ClientID in ADVERTISE/REPLY
  if(write_duid_opt(&w, OPT_SERVERID, &sctx->server_duid) < 0) return -1;
  if(write_duid_opt(&w, OPT_CLIENTID, &rq.client_id) < 0) return -1;

  // Options only reply (INFOREQ)
  if(rq.hdr.msg_type == DHCP6_INFOREQ){
    if(write_dns_if_requested(sctx, &rq, &w) < 0) return -1;
  }else if(rq.hdr.msg_type == DHCP6_CONFIRM){
    // CONFIRM: do NOT allocate; only SUCCESS / NotOnLink
    if(write_dns_if_requested(sctx, &rq, &w) < 0) return -1;

    if(rq.has_ia_na){
      uint16_t st = na_onlink ? 0 : 6; // SUCCESS : NotOnLink
      if(write_ia_na(&w, &rq, NULL, 0, st) < 0) return -1;
    }
    if(rq.has_ia_pd){
      uint16_t st = pd_onlink ? 0 : 6; // SUCCESS : NotOnLink
      if(write_ia_pd(&w, &rq, NULL, 0, st) < 0) return -1;
    }
  }else{
    // Provide DNS if requested
    if(write_dns_if_requested(sctx, &rq, &w) < 0) return -1;

    // Include IA_NA/IA_PD if present in request (RFC-friendly behavior)
    if(rq.has_ia_na){
      if(write_ia_na(&w, &rq, na_ok?&na:NULL, na_ok, na_fail) < 0) return -1;
    }
    if(rq.has_ia_pd){
      if(write_ia_pd(&w, &rq, pd_ok?&pd:NULL, pd_ok, pd_fail) < 0) return -1;
    }
  }

  *out_len = w.off;

  // ===== 4) Multicast / Unicast response rule (RFC-faithful minimal) =====
  // - ADVERTISE: multicast to ff02::1:2
  // - SOLICIT+RapidCommit => REPLY: multicast to ff02::1:2
  // - otherwise: unicast to source address (peer)
  *out_peer = *peer;
  out_peer->sin6_port = htons(546);
  *out_ifindex = ifindex;

  if(resp_type == DHCP6_ADVERTISE ||
     (rq.hdr.msg_type == DHCP6_SOLICIT && rq.has_rapid_commit && resp_type == DHCP6_REPLY)){
    inet_pton(AF_INET6, "ff02::1:2", &out_peer->sin6_addr);
    // link-local multicast should carry scope
    out_peer->sin6_scope_id = (uint32_t)ifindex;
  }

  return 1;
}
