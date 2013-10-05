/* Copyright 2013 Drew Thoreson */

/*
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "ipv6.h"
#include "network.h"

ssize_t tcp_send_bytes(int sock, const char *buf, size_t len)
{
	size_t bsent;
	ssize_t rv;

	bsent = 0;
	while (bsent < len) {
		rv = send(sock, buf + bsent, len - bsent, MSG_NOSIGNAL);
		if (rv == -1)
			return -errno;
		bsent += rv;
	}
	return bsent;
}

ssize_t tcp_sendf(int sock, size_t size, const char *fmt, ...)
{
	char buf[size];
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(buf, size, fmt, ap);
	va_end(ap);

	return tcp_send_bytes(sock, buf, len);
}

ssize_t tcp_read_bytes(int sock, char *msg_buf, size_t bytes)
{
	ssize_t rv;
	size_t bread = 0;

	while (bread < bytes) {
		rv = recv(sock, msg_buf + bread, bytes - bread, 0);
		if (rv == -1)
			return -errno;
		if (rv == 0)
			break;
		bread += rv;
	}
	return bread;
}

ssize_t tcp_read_msg(int sock, char *buf, size_t len)
{
	size_t i;

	/* delimiter-matching state */
	struct {
		const char * const str;
		const size_t len;
		size_t pos;
	} delim = { "\r\n\r\n", 4, 0 };

	for (i = 0; i < len && delim.pos < delim.len; i++) {
		signed char c;
		ssize_t rv;

		rv = recv(sock, &c, 1, 0);
		if (rv == -1)
			return -errno;
		if (rv == 0)
			return 0;

		if (c == delim.str[delim.pos])
			delim.pos++;
		else
			delim.pos = 0;

		buf[i] = c;
	}

	if (i < len)
		buf[i] = '\0';
	return i;
}

int udp_send(const struct sockaddr *addr, size_t len, const char *msg)
{
	int sock, rc;
	socklen_t sin_size = get_sockaddr_size(addr);

	sock = socket(addr->sa_family, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == -1)
		return -errno;

	rc = sendto(sock, msg, len, 0, addr, sin_size);
	if (rc == -1)
		rc = -errno;

	close(sock);
	return rc;
}

int udp_sendf(const struct sockaddr *addr, size_t size, const char *fmt, ...)
{
	char msg[size];
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(msg, size, fmt, ap);
	va_end(ap);

	return udp_send(addr, len, msg);
}
