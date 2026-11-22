/* SPDX-License-Identifier: GPL-3.0-only */
/* The Link Cable's datagrams (src/link.h's udp_* ops): one socket that never
 * blocks. POSIX alone, no SDL, so tools/link-check.c sends through it on the
 * loopback. */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "linknet.h"

int linknet_open(const char *addr, int port)
{
	struct sockaddr_in a;
	int s = socket(AF_INET, SOCK_DGRAM, 0), one = 1;

	if (s < 0) return -1;
	/* Not into a program the launcher starts, and never waiting */
	fcntl(s, F_SETFD, FD_CLOEXEC);
	fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons((unsigned short)port);
	a.sin_addr.s_addr = htonl(INADDR_ANY);
	if (addr && inet_pton(AF_INET, addr, &a.sin_addr) != 1) {
		close(s);
		return -1;
	}
	if (bind(s, (struct sockaddr *)&a, sizeof a) < 0) {
		fprintf(stderr, "link: bind %s:%d: %s\n", addr ? addr : "*", port, strerror(errno));
		close(s);
		return -1;
	}
	return s;
}

bool linknet_send(int s, const char *addr, int port, const char *buf, unsigned len)
{
	struct sockaddr_in a;

	if (s < 0 || !addr) return false;
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons((unsigned short)port);
	if (inet_pton(AF_INET, addr, &a.sin_addr) != 1) return false;
	return sendto(s, buf, len, 0, (struct sockaddr *)&a, sizeof a) == (ssize_t)len;
}

int linknet_recv(int s, char *buf, unsigned n, char *from, unsigned fromn)
{
	struct sockaddr_in a;
	socklen_t al = sizeof a;
	ssize_t got;

	if (s < 0) return 0;
	got = recvfrom(s, buf, n, 0, (struct sockaddr *)&a, &al);
	if (got <= 0) return 0;
	if (from && fromn && !inet_ntop(AF_INET, &a.sin_addr, from, fromn)) from[0] = '\0';
	return (int)got;
}

void linknet_close(int s)
{
	if (s >= 0) close(s);
}
