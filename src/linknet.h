/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_LINKNET_H
#define ZYPEROS_LINKNET_H

#include <stdbool.h>

/* The Link Cable's datagram socket, as link_ops's udp_* (src/link.h): bound
 * to `addr` (NULL for every address) and `port` (0 for any), never blocking;
 * a send; a receive (0 for nothing waiting, the sender's address into
 * `from`); and the close. */
int  linknet_open(const char *addr, int port);
bool linknet_send(int s, const char *addr, int port, const char *buf, unsigned len);
int  linknet_recv(int s, char *buf, unsigned n, char *from, unsigned fromn);
void linknet_close(int s);

#endif
