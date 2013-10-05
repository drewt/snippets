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

#ifndef _IPV6_H
#define _IPV6_H

#include <string.h> /* memcmp */
#include <sys/socket.h>
#include <netinet/in.h>

static inline socklen_t get_sockaddr_size (const struct sockaddr *sa)
{
	return sa->sa_family == AF_INET ? sizeof (struct sockaddr_in) :
		sizeof (struct sockaddr_in6);
}

static inline void *get_in_addr (const struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static inline in_port_t get_in_port (const struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return ((struct sockaddr_in*)sa)->sin_port;
	return ((struct sockaddr_in6*)sa)->sin6_port;
}

static inline void set_in_port (struct sockaddr *sa, in_port_t val)
{
	if (sa->sa_family == AF_INET)
		((struct sockaddr_in*)sa)->sin_port = val;
	else
		((struct sockaddr_in6*)sa)->sin6_port = val;
}

static inline size_t get_addr_strlen(const struct sockaddr *sa)
{
	return sa->sa_family == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN;
}

static inline int sin_equals (const struct sockaddr_in *a,
        const struct sockaddr_in *b)
{
    return a->sin_addr.s_addr == b->sin_addr.s_addr &&
        a->sin_port == b->sin_port;
}

static inline int sin6_equals (const struct sockaddr_in6 *a,
        const struct sockaddr_in6 *b)
{
    return !memcmp (a->sin6_addr.s6_addr, b->sin6_addr.s6_addr, 16) &&
        a->sin6_port == b->sin6_port;
}

static inline int sockaddr_equals (const struct sockaddr *a,
        const struct sockaddr *b)
{
    if (a->sa_family != b->sa_family)
        return 0;
    if (a->sa_family == AF_INET)
        return sin_equals ((struct sockaddr_in*)a, (struct sockaddr_in*)b);
    else if (a->sa_family == AF_INET6)
        return sin6_equals ((struct sockaddr_in6*)a, (struct sockaddr_in6*)b);
    return 0;
}

static inline int ip_addr_equals (const struct sockaddr *a,
        const struct sockaddr *b)
{
    if (a->sa_family != b->sa_family)
        return 0;
    if (a->sa_family == AF_INET) {
        return ((struct sockaddr_in*)a)->sin_addr.s_addr
            != ((struct sockaddr_in*)b)->sin_addr.s_addr;
    } else {
        return !memcmp (((struct sockaddr_in6*)a)->sin6_addr.s6_addr,
                ((struct sockaddr_in6*)b)->sin6_addr.s6_addr, 16);
    }
}

#endif
