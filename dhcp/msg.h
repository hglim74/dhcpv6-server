#pragma once
#include <stdint.h>
#include <stddef.h>
#include "util/buf.h"

typedef struct {
  uint8_t msg_type;
  uint8_t txid[3];
} dh6_hdr_t;

enum {
  DHCP6_SOLICIT=1,
  DHCP6_ADVERTISE=2,
  DHCP6_REQUEST=3,
  DHCP6_CONFIRM=4,
  DHCP6_RENEW=5,
  DHCP6_REBIND=6,
  DHCP6_REPLY=7,
  DHCP6_RELEASE=8,
  DHCP6_DECLINE=9,
  DHCP6_RECONFIGURE=10,
  DHCP6_INFOREQ=11,
  DHCP6_RELAYFWD=12,
  DHCP6_RELAYREPL=13
};

int dh6_parse_hdr(const uint8_t* pkt, size_t len, dh6_hdr_t* h, rd_t* body);
int dh6_write_hdr(wr_t* w, uint8_t msg_type, const uint8_t txid[3]);
