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

#ifndef _SERVER_H
#define _SERVER_H

#include <netinet/in.h>
#include <pthread.h>

#define MSG_MAX 512

enum {
	SOCK_TCP,
	SOCK_UDP
};

/*
 * A UDP or TCP message from a client.
 */
struct msg_info {
	int sock;
	int socktype;
	char *msg;
	size_t len;
	struct sockaddr_storage addr;
	char paddr[INET6_ADDRSTRLEN];
};

int tcp_server_init(char *port);

_Noreturn void tcp_server_main(int sock, int max_threads, void*(*cb)(void*));

int udp_server_init(char *port);

_Noreturn void udp_server_main(int sock, int max_threads, void *(*cb)(void*));

_Noreturn void service_exit(struct msg_info *msg);

#endif
