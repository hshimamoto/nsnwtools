/*
 * nsnw
 * MIT License Copyright(c) 2019 Hiroshi Shimamoto
 */
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int jail(void)
{
	int uid = getuid();
	int gid = getgid();

	if (unshare(CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWUSER) == -1) {
		perror("unshare");
		return -1;
	}

	FILE *fp;

	fp = fopen("/proc/self/setgroups", "w");
	fprintf(fp, "deny\n");
	fclose(fp);
	fp = fopen("/proc/self/uid_map", "w");
	fprintf(fp, "0 %u 1\n", uid);
	fclose(fp);
	fp = fopen("/proc/self/gid_map", "w");
	fprintf(fp, "0 %u 1\n", gid);
	fclose(fp);

	// check root?
	if (getuid() != 0 || getgid() != 0) {
		fprintf(stderr, "no root\n");
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	if (jail() == -1)
		exit(1);

	if (daemon(1, 1)) {
		perror("daemon");
		exit(1);
	}

	FILE *fp = fopen("nsnw.pid", "w");
	fprintf(fp, "%u", getpid());
	fclose(fp);

	for (;;)
		sleep(600);

	return 0;
}
