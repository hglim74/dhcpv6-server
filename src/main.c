#include "net/sock.h"
#include "util/log.h"
#include "util/time.h"
#include "util/hash.h"

#include "dhcp/handlers.h"
#include "store/mem_store.h"
#include "config/config.h"
#include "cli/cli.h"

#include <string.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>

static void make_server_duid(duid_t* d, uint64_t seed){
  static const uint8_t raw[] = {
    0x00,0x02, /* DUID-EN */
    0x12,0x34,0x56,0x78,
    0xaa,0xbb,0xcc,0xdd
  };
  memcpy(d->bytes, raw, sizeof(raw));
  d->len = sizeof(raw);
  d->h = hash64_bytes(d->bytes, d->len, seed);
}

int main(){
  log_set_level(LOG_INFO);

  /* socket */
  dh6_sock_t sock;
  if(dh6_sock_open(&sock, 547) < 0) return 1;

  /* store */
  lease_store_t st;
  if(mem_store_init(&st, 4096) < 0) return 1;

  /* server context */
  server_ctx_t s;
  memset(&s, 0, sizeof(s));
  s.store = &st;
  s.duid_seed = 0xA5A5A5A5ULL;

  make_server_duid(&s.server_duid, s.duid_seed);

  /* defaults (can be overridden by config) */
  s.offer_ttl = 30;
  s.decline_ttl = 600;
  s.preferred_lft = 43200;
  s.valid_lft = 86400;

  /* load config */
  config_load("/etc/dhcpv6d.conf", &s);
  config_dump(&s);

  /* CLI */
  cli_init(&s, "/run/dhcpv6d.sock");

  log_printf(LOG_INFO, "dhcpv6d started");

  while(1){
    fd_set rfds;
    FD_ZERO(&rfds);

    int maxfd = 0;

    FD_SET(sock.fd, &rfds);
    if(sock.fd > maxfd) maxfd = sock.fd;

    int cli_fd = cli_get_fd();
    if(cli_fd >= 0){
      FD_SET(cli_fd, &rfds);
      if(cli_fd > maxfd) maxfd = cli_fd;
    }

    int rc = select(maxfd + 1, &rfds, NULL, NULL, NULL);
    if(rc < 0) continue;

    /* DHCPv6 packet */
    if(FD_ISSET(sock.fd, &rfds)){
      uint8_t inbuf[2048], outbuf[2048];
      size_t inlen=0, outlen=0;
      struct sockaddr_in6 peer, out_peer;
      int ifindex=0, out_ifindex=0;

      int r = dh6_sock_recv(&sock, inbuf, sizeof(inbuf), &inlen, &peer, &ifindex);
      if(r > 0){
        int h = dh6_handle_packet(&s, inbuf, inlen,
                                  &peer, ifindex,
                                  outbuf, sizeof(outbuf), &outlen,
                                  &out_peer, &out_ifindex);
        if(h == 1){
          dh6_sock_send(&sock, outbuf, outlen, &out_peer, out_ifindex);
        }
      }
    }

    /* CLI */
    if(cli_fd >= 0 && FD_ISSET(cli_fd, &rfds)){
      cli_handle();
    }
  }

  return 0;
}
