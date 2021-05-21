/*
 * nsproxy
 * MIT License Copyright(c) 2020,2021 Hiroshi Shimamoto
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "net.h"

extern int nsenter(pid_t pid);

#define BUFSZ	(65536)
const int defport = 8888;
const int bufsz = BUFSZ;
static char buf[BUFSZ];

static int child_connect(int s, const char *host, const char *port)
{
	struct addrinfo hints, *res;
	int r;

	r = socket(AF_INET, SOCK_STREAM, 0);
	if (r < 0)
		return -1;

	printf("connecting to %s %s\n", host, port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(host, port, &hints, &res) != 0)
		return -1;
	if (connect(r, res->ai_addr, res->ai_addrlen) < 0)
		return -1; /* TODO: freeaddrinfo(res) */
	freeaddrinfo(res);

	return r;
}

static void child_readwrite(int s, int r)
{
	fd_set fds;
	int max;
	int ret;

	max = (r > s) ? r : s;
	max++;

	for (;;) {
		struct timeval tv;
		FD_ZERO(&fds);
		FD_SET(s, &fds);
		FD_SET(r, &fds);
		tv.tv_sec = 3600;
		tv.tv_usec = 0;
		ret = select(max, &fds, NULL, NULL, &tv);
		if (ret < 0)
			return;
		// timeout happens
		if (ret == 0) {
			printf("nothing happen in hour, disconnect\n");
			return;
		}
		if (FD_ISSET(s, &fds)) {
			ret = read(s, buf, bufsz);
			if (ret <= 0)
				return;
			write(r, buf, ret);
		}
		if (FD_ISSET(r, &fds)) {
			ret = read(r, buf, bufsz);
			if (ret <= 0)
				return;
			write(s, buf, ret);
		}
	}
}

static void child_work(int s)
{
	char *proto, *crlf2;
	char *host, *port;
	int ret;
	int r;

	ret = read(s, buf, bufsz);
	if (ret < 0)
		return;
	if (strncmp(buf, "CONNECT ", 8))
		return;
	proto = strstr(buf + 8, " HTTP/1");
	if (!proto)
		return;
	crlf2 = strstr(proto, "\r\n\r\n");
	if (!crlf2)
		return;
	/* get host:port */
	host = buf + 8;
	port = host;
	while (port < proto) {
		if (*port == ':')
			goto hostok;
		port++;
	}
	return;
hostok:
	*port++ = '\0'; /* clear ':' */
	*proto = '\0';
	r = child_connect(s, host, port);
	if (r < 0)
		return;
	/* connect ok 56789 123456789 123 */
	write(s, "HTTP/1.0 200 CONNECT OK\r\n\r\n", 27);
	/* write rest */
	crlf2 += 4;
	ret -= crlf2 - &buf[0];
	if (ret > 0)
		write(r, crlf2, ret);

	child_readwrite(s, r);
}

static void accept_and_run(int s)
{
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	int cli;
	pid_t pid;

	cli = accept(s, (struct sockaddr *)&addr, &len);
	if (cli == -1) {
		if (errno == EINTR)
			return;
		exit(1);
	}

	/* ok, fork it */
	pid = fork();
	if (pid) {
		/* no need client side socket */
		close(cli);
		return;
	}

	/* no need accept socket */
	close(s);

	child_work(cli);
	_exit(0);
}

int main(int argc, char **argv)
{
	struct sockaddr_in addr;
	pid_t pid;
	int s;

	if (argc < 2) {
		fprintf(stderr, "nsproxy pid [listen addr:port]\n");
		exit(1);
	}

	pid = atoi(argv[1]);

	// init addr
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(defport);

	if (argc > 2) {
		char *opt = argv[2];

		if (opt[0] == ':') {
			// port only
			int port = atoi(opt + 1);
			addr.sin_port = htons(port);
		} else {
			char *del = strchr(opt, ':');
			if (del) {
				*del = '\0';
				++del;
			}
			// currently only inet_addr support
			inet_aton(opt, &addr.sin_addr);
		}
	}

	/* don't care about child */
	signal(SIGCHLD, SIG_IGN);

	s = listensocket(&addr);
	if (s < 0)
		exit(1);

	// now enter the new namespace
	if (nsenter(pid) == -1)
		exit(1);

	/* accept loop */
	for (;;)
		accept_and_run(s);

	return 0;
}
