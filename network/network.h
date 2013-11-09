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

#ifndef _NETWORK_H
#define _NETWORK_H

#include <unistd.h>     /* ssize_t */
#include <sys/uio.h>    /* iovec */
#include <sys/socket.h>
#include <netinet/in.h>

ssize_t tcp_read_bytes(int sock, char *msg_buf, size_t bytes);
ssize_t tcp_send_bytes(int sock, const char *buf, size_t len);
ssize_t tcp_send_vector(int sock, struct iovec *vec, size_t len);
ssize_t tcp_sendf(int sock, size_t size, const char *fmt, ...);

ssize_t netstring_read(int sock, char **dst);
ssize_t netstring_send(int sock, size_t size, const char *msg);
ssize_t netstring_send_vector(int sock, struct iovec *vec, size_t len);
ssize_t netstring_sendf(int sock, size_t size, const char *fmt, ...);

int udp_send(const struct sockaddr *addr, size_t len, const char *msg);
int udp_sendf(const struct sockaddr *addr, size_t size, const char *fmt, ...);

#endif
