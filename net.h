/*
 * net.h
 * MIT License Copyright(c) 2021 Hiroshi Shimamoto
 */

#ifndef NET_H
#define NET_H

#define _GNU_SOURCE

static int listensocket(struct sockaddr_in *paddr)
{
	int s, one = 1;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return -1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0)
		goto bad;
	if (bind(s, (struct sockaddr *)paddr, sizeof(*paddr)) < 0)
		goto bad;
	if (listen(s, 5) < 0)
		goto bad;

	return s;
bad:
	close(s);
	return -1;
}

static void iorelay(int s, int r)
{
	fd_set fds;
	int max;
	int ret;

	const int bufsz = 65536;
	char *buf = malloc(bufsz);

	max = ((r > s) ? r : s) + 1;

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
			int rest = ret;
			while (rest > 0) {
				ret = write(r, buf, ret);
				if (ret <= 0)
					return;
				rest -= ret;
			}
		}
		if (FD_ISSET(r, &fds)) {
			ret = read(r, buf, bufsz);
			if (ret <= 0)
				return;
			int rest = ret;
			while (rest > 0) {
				ret = write(s, buf, ret);
				if (ret <= 0)
					return;
				rest -= ret;
			}
		}
	}
}

#endif // NET_H
