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

/* network.c
 *
 * This file contains some convenient functions for TCP/UDP communication which
 * avoid the short read/short write problem, as well as a function to
 * packetize incoming TCP streams.
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

#define NETSTRING_MAX_DIGITS 100

static void shift_msghdr(struct msghdr *hdr, size_t amnt)
{
	size_t i, off;
	int tmp = amnt;

	for (i = 0; tmp > 0 && i < hdr->msg_iovlen; i++)
		tmp -= hdr->msg_iov[i].iov_len;

	/* exactly on iov boundary */
	if (tmp == 0) {
		hdr->msg_iov = &hdr->msg_iov[i];
		hdr->msg_iovlen -= i;
		return;
	}

	hdr->msg_iov = &hdr->msg_iov[i-1];
	hdr->msg_iovlen -= i-1;

	off = hdr->msg_iov->iov_len + tmp;
	hdr->msg_iov->iov_base += off;
	hdr->msg_iov->iov_len -= off;
}

ssize_t tcp_send_vector(int sock, struct iovec *vec, size_t len)
{
	ssize_t rv;
	size_t bsent, total = 0;
	struct msghdr hdr = { .msg_iov = vec, .msg_iovlen = len };

	for (size_t i = 0; i < len; i++)
		total += vec[i].iov_len;

	for (;;) {
		rv = sendmsg(sock, &hdr, MSG_NOSIGNAL);
		if (rv == -1)
			return -errno;
		bsent += rv;
		if (bsent < total && rv > 0)
			shift_msghdr(&hdr, rv);
		else
			break;
	}
	return bsent;
}

ssize_t netstring_send_vector(int sock, struct iovec *vec, size_t len)
{
	struct iovec msg_iov[len+2];
	char digits[NETSTRING_MAX_DIGITS];
	int digits_len;
	size_t total = 0;

	for (size_t i = 0; i < len; i++)
		total += vec[i].iov_len;

	digits_len = snprintf(digits, NETSTRING_MAX_DIGITS, "%lu:", total);

	msg_iov[0].iov_base = digits;
	msg_iov[0].iov_len  = digits_len;

	for (size_t i = 1; i < len + 1; i++)
		msg_iov[i] = vec[i-1];

	msg_iov[len+1].iov_base = ",";
	msg_iov[len+1].iov_len  = 1;

	return tcp_send_vector(sock, msg_iov, len+2);
}

ssize_t netstring_send(int sock, size_t size, const char *msg)
{
	int digits_len;
	char digits[NETSTRING_MAX_DIGITS];
	struct iovec msg_iov[] = {
		[1] = { .iov_base = (void*) msg, .iov_len = size },
		[2] = { .iov_base = (void*) ",", .iov_len = 1 }
	};

	digits_len = snprintf(digits, NETSTRING_MAX_DIGITS, "%lu:", size);

	msg_iov[0].iov_base = digits;
	msg_iov[0].iov_len  = digits_len;

	return tcp_send_vector(sock, msg_iov, 3);
}

ssize_t netstring_sendf(int sock, size_t size, const char *fmt, ...)
{
	size_t buf_len = size + NETSTRING_MAX_DIGITS + 2;
	char buf[buf_len];
	char *start;
	va_list ap;
	int len, tmp, digits = 1;

	va_start(ap, fmt);
	len = vsnprintf(buf + NETSTRING_MAX_DIGITS + 1, buf_len, fmt, ap);
	va_end(ap);

	buf[NETSTRING_MAX_DIGITS + len + 1] = ',';

	tmp = len;
	while ((tmp /= 10) != 0)
		digits++;

	start = buf + (NETSTRING_MAX_DIGITS - digits - 1);
	len += sprintf(start, "%d:", len);

	return tcp_send_bytes(sock, start, len+2);
}

ssize_t netstring_read(int sock, char **dst)
{
	char *data;
	size_t size = 0;

	for (int i = 0; i < NETSTRING_MAX_DIGITS; i++) {
		signed char c;
		ssize_t rv;

		rv = recv(sock, &c, 1, 0);
		if (rv == -1)
			return -errno;
		if (rv == 0)
			return 0;

		if (i == 0 && c == '0')
			return 0;

		if (c == ':')
			break;

		if (c < '0' || c > '9')
			return -1;

		size *= 10;
		size += c - '0';
	}
	if (size == 0)
		return 0;

	data = malloc(size + 1);
	tcp_read_bytes(sock, data, size + 1);

	if (data[size] != ',') {
		free(data);
		return -1;
	}

	data[size] = '\0';
	*dst = data;
	return size;
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
