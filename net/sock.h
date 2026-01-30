#pragma once
#include <netinet/in.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
  int fd;
} dh6_sock_t;

int dh6_sock_open(dh6_sock_t* s, uint16_t port); // bind :: port
int dh6_sock_recv(dh6_sock_t* s, uint8_t* buf, size_t cap, size_t* out_len,
                  struct sockaddr_in6* peer, int* out_ifindex);
int dh6_sock_send(dh6_sock_t* s, const uint8_t* buf, size_t len,
                  const struct sockaddr_in6* peer, int ifindex);
