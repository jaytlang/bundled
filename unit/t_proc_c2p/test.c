#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <event.h>
#include <stdlib.h>
#include <string.h>

#include "imaged.h"

static void	 child_msgreply(int, int, struct ipcmsg *);
static void	 parent_readreply(int, int, struct ipcmsg *);

static uint32_t	 testkey = 5536;
static char 	*testmsg = "hello, world";
static int	 parentresponses = 0;

int		 debug = 1, verbose = 1;

static void
child_msgreply(int type, int fd, struct ipcmsg *data)
{
	char	*message;

	if (type != IMSG_HELLO)
		errx(1, "child got unexpected message type %d", type);
	else if (fd != -1)
		errx(1, "child got unexpected fd %d", fd);

	if (ipcmsg_getkey(data) != testkey)
		errx(1, "child got incorrect key %u", ipcmsg_getkey(data));

	message = ipcmsg_getmsg(data);
	if (message == NULL)
		err(1, "ipcmsg_getmsg");

	if (strcmp(message, testmsg) != 0)
		errx(1, "child got incorrect message %s", message);

	free(message);
	myproc_send(PROC_ROOT, IMSG_HELLO, -1, data);
}

static void
parent_readreply(int type, int fd, struct ipcmsg *data)
{
	char	*message;

	if (type != IMSG_HELLO)
		errx(1, "parent got unexpected message type %d", type);
	else if (fd != -1)
		errx(1, "parent got unexpected fd %d", fd);

	if (ipcmsg_getkey(data) != testkey)
		errx(1, "parent got incorrect key %u", ipcmsg_getkey(data));

	message = ipcmsg_getmsg(data);
	if (message == NULL)
		err(1, "ipcmsg_getmsg");

	if (strcmp(message, testmsg) != 0)
		errx(1, "parent got incorrect message %s", message);

	free(message);

	if (++parentresponses == 2) exit(0);
}

void
frontend_launch(void)
{
	myproc_listen(PROC_ROOT, child_msgreply);
	myproc_listen(PROC_ENGINE, nothing);

	event_dispatch();
}

void
engine_launch(void)
{
	myproc_listen(PROC_ROOT, child_msgreply);
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

	e = proc_new(PROC_ENGINE);
	if (e == NULL) err(1, "proc_new -> engine process");

	proc_startall(p, f, e);

	ping = ipcmsg_new(testkey, testmsg);
	if (ping == NULL) err(1, "ipcmsg_new");

	myproc_listen(PROC_FRONTEND, parent_readreply);
	myproc_listen(PROC_ENGINE, parent_readreply);

	myproc_send(PROC_FRONTEND, IMSG_HELLO, -1, ping);
	myproc_send(PROC_ENGINE, IMSG_HELLO, -1, ping);

	ipcmsg_teardown(ping);
	event_dispatch();
}
