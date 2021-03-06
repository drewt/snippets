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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <syslog.h>
#include <pthread.h>

#include "network.h"
#include "server.h"

/* server.c
 *
 * This file implements threaded TCP/UDP servers.
 */

#define BACKLOG 10

static int num_threads;
static pthread_mutex_t num_threads_lock;

int tcp_server_init(char *port)
{
	struct addrinfo hints, *servinfo, *p;
	const int yes = 1;
	int sockfd;
	int rc;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = AI_PASSIVE;

	if ((rc = getaddrinfo(NULL, port, &hints, &servinfo))) {
		syslog(LOG_EMERG, "getaddrinfo: %s\n", gai_strerror(rc));
		exit(EXIT_FAILURE);
	}

	/* create a socket to listen for incoming connections */
	for (p = servinfo; p; p = p->ai_next) {
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd == -1) {
			syslog(LOG_ERR, "socket: %s\n", strerror(errno));
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			syslog(LOG_EMERG, "setsockopt: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			syslog(LOG_ERR, "bind: %s\n", strerror(errno));
			continue;
		}

		break;
	}

	if (!p) {
		fprintf(stderr, "tcpserver: failed to bind\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(servinfo);

	if (listen(sockfd, BACKLOG) == -1) {
		syslog(LOG_EMERG, "failed to bind\n");
		exit(EXIT_FAILURE);
	}

	return sockfd;
}

_Noreturn void tcp_server_main(int sock, int max_threads, void*(*cb)(void*))
{
	socklen_t sin_size;
	struct msg_info *targ;
	pthread_t tid;

	struct timeval tv = { .tv_sec = 30, .tv_usec = 0 };

	for (;;) {

		targ = malloc(sizeof(struct msg_info));
		targ->socktype = SOCK_TCP;

		/* wait for a connection */
		sin_size = sizeof(targ->addr);
		targ->sock = accept(sock, (struct sockaddr*) &targ->addr, &sin_size);
		if (targ->sock == -1) {
			syslog(LOG_ERR, "accept: %s\n", strerror(errno));
			free(targ);
			continue;
		}

		/* close connection if thread limit reached */
		pthread_mutex_lock(&num_threads_lock);
		if (num_threads >= max_threads) {
			pthread_mutex_unlock(&num_threads_lock);
			syslog(LOG_WARNING, "thread limit reached\n");
			close(targ->sock);
			free(targ);
			continue;
		}

		num_threads++;
		pthread_mutex_unlock(&num_threads_lock);

		setsockopt(targ->sock, SOL_SOCKET, SO_RCVTIMEO, (char*) &tv,
				sizeof(tv));

#ifdef VERBOSE_LOG
		inet_ntop(targ->addr.ss_family,
				get_in_addr((struct sockaddr*) &targ->addr),
				targ->paddr, sizeof targ->paddr);
		syslog(LOG_INFO, "connection from %s\n", targ->paddr);
#endif
		/* create a new thread to service the connection */
		if (pthread_create(&tid, NULL, cb, targ))
			syslog(LOG_ERR, "pthread_create\n");
		else
			pthread_detach(tid);
	}
}

int udp_server_init(char *port)
{
	struct addrinfo hints, *servinfo, *p;
	const int yes = 1;
	int sockfd;
	int rc;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags    = AI_PASSIVE;

	if ((rc = getaddrinfo(NULL, port, &hints, &servinfo))) {
		syslog(LOG_EMERG, "getaddrinfo: %s\n", gai_strerror(rc));
		exit(EXIT_FAILURE);
	}

	for (p = servinfo; p; p = p->ai_next) {
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd == -1) {
			syslog(LOG_ERR, "socket: %s\n", strerror(errno));
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			syslog(LOG_EMERG, "setsockopt: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			syslog(LOG_ERR, "bind: %s\n", strerror(errno));
			continue;
		}

		break;
	}

	if (!p) {
		syslog(LOG_EMERG, "failed to bind\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(servinfo);

	num_threads = 0;
	pthread_mutex_init(&num_threads_lock, NULL);

	return sockfd;
}

_Noreturn void udp_server_main(int sock, int max_threads, void *(*cb)(void*))
{
	struct msg_info *msg;
	socklen_t sin_size;
	ssize_t rc;
	pthread_t tid;

	for(;;) {
		sin_size = sizeof(struct sockaddr_in);
		msg = malloc(sizeof(struct msg_info));
		msg->msg = malloc(MSG_MAX);
		msg->socktype = SOCK_UDP;

		rc = recvfrom(sock, msg->msg, MSG_MAX-1, 0,
				(struct sockaddr*) &msg->addr, &sin_size);
		if (rc == -1) {
			syslog(LOG_ERR, "recvfrom: %s\n", strerror(errno));
			free(msg->msg);
			free(msg);
			continue;
		}
		msg->msg[rc] = '\0';
		msg->len = rc;

		pthread_mutex_lock(&num_threads_lock);
		if (num_threads >= max_threads) {
			pthread_mutex_unlock(&num_threads_lock);
			syslog(LOG_WARNING, "thread limit reached\n");
			free(msg->msg);
			free(msg);
			continue;
		}

		num_threads++;
		pthread_mutex_unlock(&num_threads_lock);

#ifdef VERBOSE_LOG
		inet_ntop(msg->addr.ss_family,
				get_in_addr((struct sockaddr*) &msg->addr),
				msg->paddr, sizeof msg->paddr);
		syslog(LOG_INFO, "message from %s\n", targ->paddr);
#endif

		if (pthread_create(&tid, NULL, cb, msg))
			syslog(LOG_ERR, "pthread_create\n");
		else
			pthread_detach(tid);
	}
	close(sock);
}

_Noreturn void service_exit(struct msg_info *msg)
{
	close(msg->sock);
#ifdef VERBOSE_LOG
	syslog(LOG_INFO, "connection from %s closed\n", fdsa->paddr);
#endif
	if (msg->socktype == SOCK_UDP)
		free(msg->msg);
	free(msg);

	pthread_mutex_lock(&num_threads_lock);
	num_threads--;
	pthread_mutex_unlock(&num_threads_lock);
	pthread_exit(NULL);
}
