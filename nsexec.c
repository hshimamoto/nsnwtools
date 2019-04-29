/*
 * nsexec
 * MIT License Copyright(c) 2019 Hiroshi Shimamoto
 */
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

extern int nsenter(pid_t pid);

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "nsenter pid command\n");
		exit(1);
	}

	if (nsenter(atoi(argv[1])) == -1)
		exit(1);

	return execvp(argv[2], &argv[2]);
}
