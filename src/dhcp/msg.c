#include "dhcp/msg.h"
#include <string.h>

int dh6_parse_hdr(const uint8_t* pkt, size_t len, dh6_hdr_t* h, rd_t* body){
  if(len < 4) return -1;
  rd_t r = rd_make(pkt, len);
  if(rd_u8(&r, &h->msg_type) < 0) return -1;
  const uint8_t* tx;
  if(rd_bytes(&r, &tx, 3) < 0) return -1;
  memcpy(h->txid, tx, 3);
  *body = r;
  return 0;
}

int dh6_write_hdr(wr_t* w, uint8_t msg_type, const uint8_t txid[3]){
  if(wr_u8(w, msg_type) < 0) return -1;
  if(wr_bytes(w, txid, 3) < 0) return -1;
  return 0;
}
