/*
 * nsenter
 * MIT License Copyright(c) 2019 Hiroshi Shimamoto
 */
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

int nsenter(pid_t pid)
{
	char userns[32], netns[32];

	snprintf(userns, 32, "/proc/%u/ns/user", pid);
	snprintf(netns, 32, "/proc/%u/ns/net", pid);

	int user = open(userns, O_RDONLY);
	if (user == -1) {
		perror(userns);
		return -1;
	}
	int net = open(netns, O_RDONLY);
	if (net == -1) {
		perror(netns);
		return -1;
	}
	if (setns(user, CLONE_NEWUSER) == -1) {
		perror("newuser");
		return -1;
	}
	if (setns(net, CLONE_NEWNET) == -1) {
		perror("newnet");
		return -1;
	}
	close(user);
	close(net);

	return 0;
}
