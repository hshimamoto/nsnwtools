/*
 * nsnw
 * MIT License Copyright(c) 2019 Hiroshi Shimamoto
 */
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	char *name = NULL;
	int foreground = 0;

	// nsnw [name]
	//  or
	// nsnw -d
	//  foreground mode
	if (argc > 1) {
		if (strcmp(argv[1], "-d") == 0)
			foreground = 1;
		else
			name = argv[1];
	}

	if (!foreground) {
		if (fork())
			return 0;
		// child
		// setup env
		pid_t pid = getpid();
		char pid_s[256];

		sprintf(pid_s, "%u", pid);

		if (!name) {
			name = malloc(256);
			sprintf(name, "net-%u", pid);
		}

		setenv("NSNW_PID", pid_s, 1);
		setenv("NSNW_NAME", name, 1);

		FILE *fp = fopen("nsnw.pid", "w");
		fprintf(fp, "%u", pid);
		fclose(fp);

		return execlp(argv[0], argv[0], "-d", NULL);
	}

	if (jail() == -1)
		exit(1);

	for (;;)
		sleep(600);

	return 0;
}
