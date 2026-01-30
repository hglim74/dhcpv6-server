#include "util/buf.h"
#include <string.h>

int rd_u8(rd_t* r, uint8_t* out){
  if(r->off + 1 > r->n) return -1;
  *out = r->p[r->off++];
  return 0;
}
int rd_u16(rd_t* r, uint16_t* out){
  if(r->off + 2 > r->n) return -1;
  *out = (uint16_t)( (r->p[r->off]<<8) | r->p[r->off+1] );
  r->off += 2;
  return 0;
}
int rd_u32(rd_t* r, uint32_t* out){
  if(r->off + 4 > r->n) return -1;
  const uint8_t* p = r->p + r->off;
  *out = ((uint32_t)p[0]<<24) | ((uint32_t)p[1]<<16) | ((uint32_t)p[2]<<8) | (uint32_t)p[3];
  r->off += 4;
  return 0;
}
int rd_bytes(rd_t* r, const uint8_t** out, size_t len){
  if(r->off + len > r->n) return -1;
  *out = r->p + r->off;
  r->off += len;
  return 0;
}

int wr_u8(wr_t* w, uint8_t v){
  if(w->off + 1 > w->n) return -1;
  w->p[w->off++] = v;
  return 0;
}
int wr_u16(wr_t* w, uint16_t v){
  if(w->off + 2 > w->n) return -1;
  w->p[w->off++] = (uint8_t)(v>>8);
  w->p[w->off++] = (uint8_t)(v&0xff);
  return 0;
}
int wr_u32(wr_t* w, uint32_t v){
  if(w->off + 4 > w->n) return -1;
  w->p[w->off++] = (uint8_t)(v>>24);
  w->p[w->off++] = (uint8_t)(v>>16);
  w->p[w->off++] = (uint8_t)(v>>8);
  w->p[w->off++] = (uint8_t)(v);
  return 0;
}
int wr_bytes(wr_t* w, const uint8_t* src, size_t len){
  if(w->off + len > w->n) return -1;
  memcpy(w->p + w->off, src, len);
  w->off += len;
  return 0;
}
