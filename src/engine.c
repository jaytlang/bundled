/* imaged true engine
 * (c) jay lang, 2023ish
 */

#include <sys/types.h>
#include <sys/time.h>

#include <errno.h>
#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imaged.h"

static void	engine_replytofrontend(int, uint32_t, char *);
static void	engine_requestsignature(uint32_t, char *);

static void	proc_getmsgfromfrontend(int, int, struct ipcmsg *);
static void	proc_getmsgfromparent(int, int, struct ipcmsg *);

static void
engine_replytofrontend(int type, uint32_t key, char *data)
{
	struct ipcmsg	*response;

	response = ipcmsg_new(key, data);
	if (response == NULL) log_fatal("engine_replytofrontend: ipcmsg_new");

	myproc_send(PROC_FRONTEND, type, -1, response);
	ipcmsg_teardown(response);
}

static void
engine_requestsignature(uint32_t key, char *path)
{
	struct ipcmsg	*request;

	request = ipcmsg_new(key, path);
	if (request == NULL) log_fatal("engine_requestsignature: ipcmsg_new");

	myproc_send(PROC_PARENT, IMSG_SIGNARCHIVE, -1, request);
	ipcmsg_teardown(request);
}

static void
proc_getmsgfromfrontend(int type, int fd, struct ipcmsg *msg)
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
			log_fatal("proc_getmsgfromfrontend: archive_fromkey");
	}

	switch (type) {
	case IMSG_NEWARCHIVE:
		if ((archive = archive_new(key)) == NULL)
			log_fatal("proc_getmsgfromfrontend: archive_new");

		engine_replytofrontend(IMSG_NEWARCHIVEACK, key, NULL);
		break;

	case IMSG_ADDFILE:
		/* XXX: races with netmsg_teardown in frontend on an unlink
		 * if netmsg_loadweakly fails to win the race, throw engine error
		 * and have the frontend intercept it silently -- activeconn in the
		 * frontend will be null at this point so should be okay
		 */
		weakmsg = netmsg_loadweakly(relmsgfile);
		if (weakmsg == NULL) {
			if (errno == ENOENT) {
				engine_replytofrontend(IMSG_ENGINEERROR, key, strerror(errno));
				break;
			}

			log_fatal("proc_getmsgfromfrontend: netmsg_loadweakly");
		}

		fname = netmsg_getlabel(weakmsg);	
		if (fname == NULL)
			log_fatalx("proc_getmsgfromfrontend: netmsg_getlabel: %s",
				netmsg_error(weakmsg));

		fdata = netmsg_getdata(weakmsg, &fdatasize);
		if (fdata == NULL)
			log_fatalx("proc_getmsgfromfrontend: netmsg_getdata: %s",
				netmsg_error(weakmsg));

		if (archive_addfile(archive, fname, fdata, (size_t)fdatasize) < 0) {
			char *error;

			error = archive_error(archive);
			engine_replytofrontend(IMSG_ENGINEERROR, key, error);
			free(error);

		} else engine_replytofrontend(IMSG_ADDFILEACK, key, NULL);
		

		free(fname);
		free(fdata);
		netmsg_teardown(weakmsg);

		break;

	case IMSG_WANTSIGN:
		fname = archive_getpath(archive);
		engine_requestsignature(key, fname);
		free(fname);

		break;

	case IMSG_GETBUNDLE:
		fname = archive_getpath(archive);
		engine_replytofrontend(IMSG_BUNDLE, key, fname);
		free(fname);

		break;

	case IMSG_KILLARCHIVE:
		archive_teardown(archive);
		break;

	default:
		log_fatalx("unknown message type received from frontend: %d", type);
	}

	free(msgfile);

	(void)fd;
}

static void
proc_getmsgfromparent(int type, int fd, struct ipcmsg *msg)
{
	struct archive	*archive;
	char		*signature;
	int	 	 key;

	log_writex(LOGTYPE_DEBUG, "signature received from parent");
	signature = ipcmsg_getmsg(msg);
	key = ipcmsg_getkey(msg);

	if (type != IMSG_SIGNATURE)
		log_fatalx("unknown message type received from parent: %d", type);

	/* XXX: race. see comment in imaged.c */
	archive = archive_fromkey(key);
	if (archive == NULL) goto end;

	archive_writesignature(archive, signature);
	engine_replytofrontend(IMSG_WANTSIGNACK, key, NULL);
end:	
	free(signature);

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

	myproc_listen(PROC_PARENT, proc_getmsgfromparent);
	myproc_listen(PROC_FRONTEND, proc_getmsgfromfrontend);

	event_dispatch();
	archive_teardownall();
}

__dead void
engine_signal(int signal, short event, void *arg)
{
	archive_teardownall();
	exit(0);

	(void)signal;
	(void)event;
	(void)arg;
}
