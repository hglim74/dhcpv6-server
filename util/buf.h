#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct { const uint8_t* p; size_t n; size_t off; } rd_t;
typedef struct { uint8_t* p; size_t n; size_t off; } wr_t;

static inline rd_t rd_make(const uint8_t* p, size_t n){ rd_t r={p,n,0}; return r; }
static inline wr_t wr_make(uint8_t* p, size_t n){ wr_t w={p,n,0}; return w; }

int rd_u8(rd_t* r, uint8_t* out);
int rd_u16(rd_t* r, uint16_t* out);
int rd_u32(rd_t* r, uint32_t* out);
int rd_bytes(rd_t* r, const uint8_t** out, size_t len);

int wr_u8(wr_t* w, uint8_t v);
int wr_u16(wr_t* w, uint16_t v);
int wr_u32(wr_t* w, uint32_t v);
int wr_bytes(wr_t* w, const uint8_t* src, size_t len);
