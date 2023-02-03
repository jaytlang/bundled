#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <err.h>
#include <event.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imaged.h"

static void	 catchsigint(int, short, void *);
static void	 firesigint(int, int, struct ipcmsg *);
static void	 ackchild(int, int, struct ipcmsg *);

int		 debug = 1, verbose = 1;

static void
catchsigint(int signal, short event, void *arg)
{
	struct ipcmsg	*response;

	warnx("caught sigint!");

	if (signal != SIGINT)
		errx(1, "caught unexpected non-sigint signal %d", signal);

	response = ipcmsg_new(0, NULL);
	if (response == NULL) err(1, "ipcmsg_new");

	myproc_send(PROC_ROOT, IMSG_HELLO, -1, response);

	ipcmsg_teardown(response);

	(void)event;
	(void)arg;
}

static void
firesigint(int type, int fd, struct ipcmsg *data)
{
	if (kill(getpid(), SIGINT) < 0)	
		err(1, "self kill");

	(void)type;
	(void)fd;
	(void)data;
}

static void
ackchild(int type, int fd, struct ipcmsg *data)
{
	exit(0);

	(void)type;
	(void)fd;
	(void)data;
}

void
frontend_launch(void)
{
	myproc_listen(PROC_ROOT, firesigint);
	myproc_listen(PROC_ENGINE, nothing);

	event_dispatch();
}

void
engine_launch(void)
{
	myproc_listen(PROC_ROOT, nothing);
	myproc_listen(PROC_FRONTEND, nothing);

	event_dispatch();
}

int
main()
{
	struct proc	*p, *f, *e;	
	struct ipcmsg	*ping;

	p = proc_new(PROC_ROOT);
	if (p == NULL) err(1, "proc_new -> parent process");

	f = proc_new(PROC_FRONTEND);
	if (f == NULL) err(1, "proc_new -> frontend process");

	proc_handlesigev(f, SIGEV_INT, catchsigint);

	e = proc_new(PROC_ENGINE);
	if (e == NULL) err(1, "proc_new -> engine process");

	proc_startall(p, f, e);

	ping = ipcmsg_new(555, "hi");
	if (ping == NULL) err(1, "ipcmsg_new");

	myproc_listen(PROC_FRONTEND, ackchild);
	myproc_listen(PROC_ENGINE, nothing);

	myproc_send(PROC_FRONTEND, IMSG_HELLO, -1, ping);

	ipcmsg_teardown(ping);
	event_dispatch();
}
