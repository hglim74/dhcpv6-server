#pragma once
#include <stdint.h>
#include <stddef.h>
#include "util/buf.h"

typedef struct {
  uint16_t code;
  uint16_t vlen;
  const uint8_t* val;
} dh6_opt_view_t;

typedef struct { size_t len_pos; size_t val_pos; } opt_mark_t;

int dh6_opt_next(rd_t* r, dh6_opt_view_t* ov);

int opt_begin(wr_t* w, uint16_t code, opt_mark_t* m);
int opt_end(wr_t* w, opt_mark_t* m);

// commonly used option codes
enum {
  OPT_CLIENTID=1,
  OPT_SERVERID=2,
  OPT_IA_NA=3,
  OPT_IA_TA=4,
  OPT_IAADDR=5,
  OPT_ORO=6,
  OPT_PREFERENCE=7,
  OPT_ELAPSED=8,
  OPT_STATUS=13,
  OPT_RAPID_COMMIT=14,
  OPT_DNS=23,
  OPT_DOMAIN_SEARCH=24,
  OPT_IA_PD=25,
  OPT_IAPREFIX=26
};
