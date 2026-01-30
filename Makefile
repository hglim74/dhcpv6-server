CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11 -I./src
LDFLAGS=

SRCS=$(shell find src -name "*.c")
OBJS=$(SRCS:.c=.o)

dhcpv6d: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

clean:
	rm -f $(OBJS) dhcpv6d
