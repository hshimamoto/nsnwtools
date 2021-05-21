CC = gcc
CFLAGS = -g -O2 -Wall
LDFLAGS = -g -O2 -Wall

bins = nsnw nsexec nstap nsproxy nsfwd

nsnw-objs = nsnw.o
nsexec-objs = nsexec.o nsenter.o
nstap-objs = nstap.o nsenter.o
nsproxy-objs = nsproxy.o nsenter.o
nsfwd-objs = nsfwd.o nsenter.o
objs = $(nsnw-objs) $(nsexec-objs) $(nstap-objs)

all: $(bins)

nstap: $(nstap-objs)
	$(CC) $(LDFLAGS) -o $@ $(nstap-objs)

nsnw: $(nsnw-objs)
	$(CC) $(LDFLAGS) -o $@ $(nsnw-objs)

nsexec: $(nsexec-objs)
	$(CC) $(LDFLAGS) -o $@ $(nsexec-objs)

nsproxy: $(nsproxy-objs)
	$(CC) $(LDFLAGS) -o $@ $(nsproxy-objs)

nsfwd: $(nsfwd-objs)
	$(CC) $(LDFLAGS) -o $@ $(nsfwd-objs)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(objs) $(bins)
