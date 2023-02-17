/* imaged true engine
 * (c) jay lang, 2023ish
 */

#include <sys/types.h>
#include <sys/time.h>

#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imaged.h"

static void	proc_getmsg(int, int, struct ipcmsg *);
static void	engine_replytofrontend(int, uint32_t, char *);

static void
engine_replytofrontend(int type, uint32_t key, char *data)
{
	struct ipcmsg	*response;

	response = ipcmsg_new(key, data);
	if (response == NULL) log_fatal("engine_replytofrontend: ipcmsg_new");

	myproc_send(PROC_FRONTEND, type, -1, response);
	ipcmsg_teardown(response);
}

void
proc_getmsg(int type, int fd, struct ipcmsg *msg)
{
	struct netmsg	*weakmsg;
	struct archive	*archive;

	char		*fdata, *fname, *msgfile, *relmsgfile;
	uint64_t	 fdatasize;
	uint32_t	 key;

	msgfile = ipcmsg_getmsg(msg);
	key = ipcmsg_getkey(msg);

	relmsgfile = msgfile + strlen(CHROOT);

	if (type != IMSG_NEWARCHIVE) {
		archive = archive_fromkey(key);
		if (archive == NULL)
			log_fatal("proc_getmsg: archive_fromkey");
	}

	switch (type) {
	case IMSG_NEWARCHIVE:
		if ((archive = archive_new(key)) == NULL)
			log_fatal("proc_getmsg: archive_new");

		engine_replytofrontend(IMSG_NEWARCHIVEACK, key, NULL);
		break;

	case IMSG_ADDFILE:
		log_writex(LOGTYPE_DEBUG, "will try to load %s", relmsgfile);
		weakmsg = netmsg_takeownership(relmsgfile);
		if (weakmsg == NULL) log_fatal("proc_getmsg: netmsg_loadweakly");

		fname = netmsg_getlabel(weakmsg);	
		if (fname == NULL)
			log_fatal("proc_getmsg: netmsg_getlabel: %s",
				netmsg_error(weakmsg));

		fdata = netmsg_getdata(weakmsg, &fdatasize);
		if (fdata == NULL)
			log_fatal("proc_getmsg: netmsg_getdata: %s",
				netmsg_error(weakmsg));

		if (archive_addfile(archive, fname, fdata, (size_t)fdatasize) < 0)
			log_fatal("proc_getmsg: archive_addfile: %s", archive_error(archive));

		free(fname);
		free(fdata);
		log_writex(LOGTYPE_DEBUG, "weak teardown");
		netmsg_teardown(weakmsg);

		engine_replytofrontend(IMSG_ADDFILEACK, key, NULL);
		break;

	case IMSG_PLEASESIGN:
		fname = archive_getpath(archive);
		engine_replytofrontend(IMSG_SIGNEDBUNDLE, key, fname);

		free(fname);
		break;

	case IMSG_KILLARCHIVE:
		archive_teardown(archive);
		break;

	default:
		log_fatal("unknown message type received from frontend: %d", type);
	}

	free(msgfile);

	(void)fd;
}

void
engine_launch(void)
{
	if (unveil(ARCHIVES, "rwc") < 0)
		log_fatal("unveil %s", ARCHIVES);
	else if (unveil(MESSAGES, "r") < 0)
		log_fatal("unveil %s", MESSAGES);

	if (pledge("stdio rpath wpath cpath", "") < 0)
		log_fatal("pledge");

	myproc_listen(PROC_ROOT, nothing);
	myproc_listen(PROC_FRONTEND, proc_getmsg);

	event_dispatch();
}
