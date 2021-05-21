/*
 * nsfwd
 * MIT License Copyright(c) 2021 Hiroshi Shimamoto
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
const int bufsz = BUFSZ;
static char buf[BUFSZ];

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

static void child_work(int s, struct sockaddr_in *dst)
{
	int r = socket(AF_INET, SOCK_STREAM, 0);
	if (r < 0)
		return;
	if (connect(r, (struct sockaddr *)dst, sizeof(*dst)) < 0)
		return;

	child_readwrite(s, r);
}

static void accept_and_run(int s, struct sockaddr_in *dst)
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

	child_work(cli, dst);
	_exit(0);
}

static int getaddr(const char *hostport, struct sockaddr_in *paddr)
{
	char *h = strdup(hostport);
	char *p = strchr(h, ':');
	if (p == NULL) {
		free(h);
		return -1;
	}
	*p++ = '\0';

	paddr->sin_family = AF_INET;
	paddr->sin_port = htons(atoi(p));
	paddr->sin_addr.s_addr = INADDR_ANY;
	if (h[0] != '\0')
		paddr->sin_addr.s_addr = inet_addr(h);

	free(h);
	return 0;
}

int main(int argc, char **argv)
{
	pid_t pid;
	int s;

	if (argc < 3) {
		fprintf(stderr, "nsfwd <listen> <dst>@<pid>\n");
		exit(1);
	}

	struct sockaddr_in listener, dst;

	if (getaddr(argv[1], &listener) == -1) {
		fprintf(stderr, "bad listener: %s\n", argv[1]);
		exit(1);
	}

	char *p = strchr(argv[2], '@');
	if (p == NULL) {
bad_dst:
		fprintf(stderr, "bad destination: %s\n", argv[2]);
		exit(1);
	}
	*p++ = '\0';
	pid = atoi(p);
	if (pid == 0)
		goto bad_dst;
	if (getaddr(argv[2], &dst) == -1)
		goto bad_dst;

	s = listensocket(&listener);
	if (s < 0)
		exit(1);

	/* don't care about child */
	signal(SIGCHLD, SIG_IGN);

	// now enter the new namespace
	if (nsenter(pid) == -1)
		exit(1);

	/* accept loop */
	for (;;)
		accept_and_run(s, &dst);

	return 0;
}
