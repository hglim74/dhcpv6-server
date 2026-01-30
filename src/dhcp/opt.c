#include "dhcp/opt.h"

int dh6_opt_next(rd_t* r, dh6_opt_view_t* ov){
  if(r->off == r->n) return 0;
  if(r->n - r->off < 4) return -1;

  uint16_t code, vlen;
  if(rd_u16(r, &code) < 0) return -1;
  if(rd_u16(r, &vlen) < 0) return -1;
  if(r->n - r->off < vlen) return -1;

  ov->code = code;
  ov->vlen = vlen;
  ov->val  = r->p + r->off;
  r->off  += vlen;
  return 1;
}

int opt_begin(wr_t* w, uint16_t code, opt_mark_t* m){
  if(wr_u16(w, code) < 0) return -1;
  m->len_pos = w->off;
  if(wr_u16(w, 0) < 0) return -1; // placeholder
  m->val_pos = w->off;
  return 0;
}

int opt_end(wr_t* w, opt_mark_t* m){
  size_t vlen = w->off - m->val_pos;
  if(vlen > 0xffff) return -1;
  w->p[m->len_pos]   = (uint8_t)(vlen >> 8);
  w->p[m->len_pos+1] = (uint8_t)(vlen & 0xff);
  return 0;
}
