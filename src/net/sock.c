#define _GNU_SOURCE
#include "net/sock.h"
#include "util/log.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>

int dh6_sock_open(dh6_sock_t* s, uint16_t port){
  memset(s, 0, sizeof(*s));
  int fd = socket(AF_INET6, SOCK_DGRAM, 0);
  if(fd < 0){
    log_printf(LOG_ERR, "socket(AF_INET6) failed: %s", strerror(errno));
    return -1;
  }

  int on = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));

  struct sockaddr_in6 addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_addr = in6addr_any;
  addr.sin6_port = htons(port);

  if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
    log_printf(LOG_ERR, "bind(:::%u) failed: %s", port, strerror(errno));
    close(fd);
    return -1;
  }

  s->fd = fd;
  return 0;
}

int dh6_sock_recv(dh6_sock_t* s, uint8_t* buf, size_t cap, size_t* out_len,
                  struct sockaddr_in6* peer, int* out_ifindex)
{
  struct iovec iov = { .iov_base = buf, .iov_len = cap };
  uint8_t cmsgbuf[CMSG_SPACE(sizeof(struct in6_pktinfo))];

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_name = peer;
  msg.msg_namelen = sizeof(*peer);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  ssize_t n = recvmsg(s->fd, &msg, 0);
  if(n < 0){
    if(errno == EINTR) return 0;
    log_printf(LOG_ERR, "recvmsg failed: %s", strerror(errno));
    return -1;
  }

  *out_len = (size_t)n;
  *out_ifindex = 0;

  for(struct cmsghdr* c = CMSG_FIRSTHDR(&msg);
      c != NULL;
      c = CMSG_NXTHDR(&msg, c)){
    if(c->cmsg_level == IPPROTO_IPV6 &&
       c->cmsg_type == IPV6_PKTINFO){
      struct in6_pktinfo* pi =
        (struct in6_pktinfo*)CMSG_DATA(c);
      *out_ifindex = (int)pi->ipi6_ifindex;
      break;
    }
  }

  return 1;
}

int dh6_sock_send(dh6_sock_t* s, const uint8_t* buf, size_t len,
                  const struct sockaddr_in6* peer, int ifindex)
{
  struct iovec iov = { .iov_base = (void*)buf, .iov_len = len };
  uint8_t cmsgbuf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
  memset(cmsgbuf, 0, sizeof(cmsgbuf));

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_name = (void*)peer;
  msg.msg_namelen = sizeof(*peer);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = IPPROTO_IPV6;
  cmsg->cmsg_type = IPV6_PKTINFO;
  cmsg->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));

  struct in6_pktinfo* pi =
    (struct in6_pktinfo*)CMSG_DATA(cmsg);
  pi->ipi6_ifindex = (unsigned)ifindex;

  ssize_t n = sendmsg(s->fd, &msg, 0);
  if(n < 0){
    log_printf(LOG_WARN, "sendmsg failed: %s", strerror(errno));
    return -1;
  }
  return 0;
}
