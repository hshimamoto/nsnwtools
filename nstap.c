/*
 * nstap
 * MIT License Copyright(c) 2019 Hiroshi Shimamoto
 */
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <sys/ioctl.h>
#include <linux/if_tun.h>
#include <net/if.h>

extern int nsenter(pid_t pid);

int open_tap(const char *name)
{
	int fd = open("/dev/net/tun", O_RDWR);
	if (fd == -1) {
		perror("open /dev/net/tun");
		return -1;
	}

	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name) - 1);

	if (ioctl(fd, TUNSETIFF, (void *)&ifr) == -1) {
		perror("ioctl(TUNSETIFF)");
		close(fd);
		return -1;
	}

	int one = 1;

	if (ioctl(fd, TUNSETPERSIST, (void *)&one) == -1) {
		perror("ioctl(TUNSETPERSIST)");
		close(fd);
		return -1;
	}

	return fd;
}

int open_nsnwtap(int pid, const char *tapname)
{
	if (nsenter(pid) == -1)
		return -1;

	return open_tap(tapname);
}

int sendfd(int sock, int fd)
{
	char dummy;
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	char cms[CMSG_SPACE(sizeof(int))];

	memset(&msg, 0, sizeof(msg));
	iov.iov_base = &dummy;
	iov.iov_len = 1;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cms;
	msg.msg_controllen = sizeof(cms);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
	memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));
	msg.msg_controllen = cmsg->cmsg_len;

	return sendmsg(sock, &msg, 0);
}

int recvfd(int sock)
{
	int fd;
	char dummy;
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	char cms[CMSG_SPACE(sizeof(int))];

	memset(&msg, 0, sizeof(msg));
	iov.iov_base = &dummy;
	iov.iov_len = 1;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cms;
	msg.msg_controllen = sizeof(cms);

	int err = recvmsg(sock, &msg, 0);

	if (err == -1) {
		perror("recvmsg");
		return -1;
	} else if (err == 0) {
		fprintf(stderr, "no message\n");
		return -1;
	}

	cmsg = CMSG_FIRSTHDR(&msg);
	if (cmsg == NULL || cmsg->cmsg_type != SCM_RIGHTS) {
		fprintf(stderr, "message doesn't contain fd\n");
		return -1;
	}

	memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));

	return fd;
}

int child(int sock, pid_t nspid, const char *tapname)
{
	int tapfd = open_nsnwtap(nspid, tapname);

	if (tapfd == -1)
		exit(1);

	return sendfd(sock, tapfd);
}

int parent(int sock, const char *tapname, int argc, char **argv)
{
	int fd = recvfd(sock);

	wait(NULL);
	close(sock);

	printf("tap %s fd=%d\n", tapname, fd);

	for (int i = 0; i < argc; i++)
		printf("argv[%d] %s\n", i, argv[i]);

	char key[256];
	char val[8];

	sprintf(key, "NSTAPFD_%s", tapname);
	sprintf(val, "%u", fd);

	setenv(key, val, 1);

	execvp(argv[0], argv);

	perror("execvp");

	for (;;) {
		unsigned char buf[4096];
		int ret;

		ret = read(fd, &buf, 4096);
		printf("recv %d bytes\n", ret);

		for (int i = 0; (i < 16 || i < ret); i++) {
			printf(" %02x", buf[i]);
		}
		printf("\n");
	}

	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "nstap pid tapname command...\n");
		exit(1);
	}

	pid_t pid_target = atoi(argv[1]);
	const char *tapname = argv[2];

	int pair[2];

	if (socketpair(PF_UNIX, SOCK_DGRAM, 0, pair) == -1) {
		perror("socketpair");
		exit(1);
	}

	pid_t pid = fork();

	if (pid == -1) {
		perror("fork");
		exit(1);
	}

	if (pid == 0) {
		close(pair[1]);
		return child(pair[0], pid_target, tapname);
	}

	close(pair[0]);
	return parent(pair[1], tapname, argc - 3, &argv[3]);
}
