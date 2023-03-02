#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bundled.h"

#define IMSG_P2C	0
#define IMSG_C2P	1

#ifndef EXPECTED_UID
#define EXPECTED_UID	1002	
#endif /* EXPECTED_UID */

static void	 checksecurity(int, int, struct ipcmsg *);
static void	 parent_ackchild(int, int, struct ipcmsg *);

int		 debug = 1, verbose = 1;

static void
checksecurity(int type, int fd, struct ipcmsg *data)
{
	struct stat	 sb;	
	struct ipcmsg	*response;
	uid_t		 ruid, euid, suid;
	gid_t		 rgid, egid, sgid;

	if (stat("/tmp", &sb) >= 0 || errno != ENOENT)
		err(1, "unexpected result from stat - are we chrooted?");

	if (getresuid(&ruid, &euid, &suid) < 0) err(1, "getresuid");

	if (ruid != EXPECTED_UID || euid != EXPECTED_UID || suid != EXPECTED_UID)
		errx(1, "got unexpected resuid %d %d %d", ruid, euid, suid);

	if (getresgid(&rgid, &egid, &sgid) < 0) err(1, "getresgid");

	if (rgid != EXPECTED_UID || egid != EXPECTED_UID || sgid != EXPECTED_UID)
		errx(1, "got unexpected resgid %d %d %d", rgid, egid, sgid);

	if (type == IMSG_P2C) {
		response = ipcmsg_new(0, NULL);
		if (response == NULL) err(1, "ipcmsg_new");

		myproc_send(PROC_PARENT, IMSG_C2P, fd, response);
		ipcmsg_teardown(response);
	}

	(void)fd;
	(void)data;
}

static void
parent_ackchild(int type, int fd, struct ipcmsg *data)
{
	checksecurity(type, fd, data);
	exit(0);
}

void
frontend_launch(void)
{
	myproc_listen(PROC_PARENT, checksecurity);
	myproc_listen(PROC_ENGINE, nothing);

	event_dispatch();
}

void
engine_launch(void)
{
	myproc_listen(PROC_PARENT, nothing);
	myproc_listen(PROC_FRONTEND, nothing);

	event_dispatch();
}

int
main()
{
	struct proc	*p, *f, *e;	
	struct ipcmsg	*ping;

	p = proc_new(PROC_PARENT);
	if (p == NULL) err(1, "proc_new -> parent process");

	proc_setchroot(p, "/var/empty");
	proc_setuser(p, "_bundled");

	f = proc_new(PROC_FRONTEND);
	if (f == NULL) err(1, "proc_new -> frontend process");

	proc_setchroot(f, "/var/empty");
	proc_setuser(f, "_bundled");

	e = proc_new(PROC_ENGINE);
	if (e == NULL) err(1, "proc_new -> engine process");

	proc_startall(p, f, e);

	ping = ipcmsg_new(555, "hi");
	if (ping == NULL) err(1, "ipcmsg_new");

	myproc_listen(PROC_FRONTEND, parent_ackchild);
	myproc_listen(PROC_ENGINE, nothing);

	myproc_send(PROC_FRONTEND, IMSG_HELLO, -1, ping);

	ipcmsg_teardown(ping);
	event_dispatch();
}
