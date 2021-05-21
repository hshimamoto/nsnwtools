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

#endif // NET_H
