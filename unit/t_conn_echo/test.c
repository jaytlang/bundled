#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <event.h>
#include <stdlib.h>
#include <unistd.h>

#include "imaged.h"

#define	PYTHON3	"/usr/local/bin/python3"	

static void	conn_accept(struct conn *);
static void	conn_echomsg(struct conn *, struct netmsg *);
static void	fork_client(void);

static int	message_counter = 0;
int		debug = 1, verbose = 1;

static void
conn_accept(struct conn *c)
{
	warnx("connection accepted");
	conn_receive(c, conn_echomsg);
}

static void
conn_echomsg(struct conn *c, struct netmsg *msg)
{
	struct netmsg	*outgoing;
	char		*msglabel;
	char		*msgdata;
	uint64_t	 msgdatasize;
	uint8_t		 msgtype;

	msgtype = netmsg_gettype(msg);

	msglabel = netmsg_getlabel(msg);
	if (msglabel == NULL)
		err(1, "conn_echomsg: netmsg_getlabel");

	msgdata = netmsg_getdata(msg, &msgdatasize);
	if (msgdata == NULL)
		err(1, "conn_echomsg: netmsg_getdata");

	warnx("received netmsg: type %u, label %s, data size %llu",
		msgtype, msglabel, msgdatasize);

	outgoing = netmsg_new(msgtype);
	if (outgoing == NULL)
		err(1, "conn_echomsg: netmsg_new");

	if (netmsg_setlabel(outgoing, msglabel) < 0)
		err(1, "conn_echomsg: netmsg_setlabel");

	if (netmsg_setdata(outgoing, msgdata, msgdatasize) < 0)
		err(1, "conn_echomsg: netmsg_setdata");

	conn_send(c, outgoing);

	free(msglabel);
	free(msgdata);

	if (++message_counter == 5) {
		conn_teardownall();
		exit(0);
	}
}

static void
fork_client(void)
{
	pid_t 	pid;

	pid = fork();

	if (pid < 0) log_fatal("fork");
	else if (pid == 0) {
		execl(PYTHON3, PYTHON3, "testclient.py", NULL);
		err(1, "exec");
	}
}

int
main()
{
	event_init();
	conn_listen(conn_accept);	

	fork_client();
	event_dispatch();
}
