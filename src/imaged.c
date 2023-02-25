/* imaged
 * (c) jay lang, 2022
 */

#include <sys/types.h>
#include <sys/time.h>

#include <dirent.h>
#include <err.h>
#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imaged.h"

__dead static void	parent_signal(int, short, void *);
__dead static void	usage(void);

static void		empty_directory(char *);

static void		parent_replytoengine(int, uint32_t, char *);
static void		proc_getmsg(int, int, struct ipcmsg *);


int	debug = 0;
int	verbose = 0;

__dead static void
parent_signal(int signal, short event, void *arg)
{
	log_writex(LOGTYPE_WARN, "clean shutdown (signal %d)", signal);
	exit(0);

	(void)event;
	(void)arg;
}

__dead static void
usage(void)
{
	fprintf(stderr, "usage: %s [-dhv]\n", __progname);
	exit(1);
}

static void
empty_directory(char *dir)
{
	struct dirent	*dp;
	DIR		*dirp;	

	dirp = opendir(dir);
	if (dirp == NULL)
		log_fatal("empty_directory: open directory %s", dir);

	while ((dp = readdir(dirp)) != NULL) {
		char *fpath;

		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;

		if (asprintf(&fpath, "%s/%s", dir, dp->d_name) < 0)
			log_fatal("empty_directory: asprintf");

		if (unlink(fpath) < 0)
			log_fatal("empty_directory: unlink %s", fpath);

		free(fpath);
	}

	closedir(dirp);
}

static void
parent_replytoengine(int type, uint32_t key, char *data)
{
	struct ipcmsg	*response;

	response = ipcmsg_new(key, data);
	if (response == NULL) log_fatal("parent_replytoengine: ipcmsg_new");

	myproc_send(PROC_ENGINE, type, -1, response);
	ipcmsg_teardown(response);
}

static void
proc_getmsg(int type, int fd, struct ipcmsg *msg)
{
	struct archive	*tosign;

	char		*msgfile, *absmsgfile, *signature;
	uint32_t	 key;

	msgfile = ipcmsg_getmsg(msg);
	key = ipcmsg_getkey(msg);
	log_writex(LOGTYPE_DEBUG, "signature request received for %s", msgfile);

	if (type != IMSG_SIGNARCHIVE)
		log_fatalx("proc_getmsg: unknown message type received from frontend: %d", type);

	if (asprintf(&absmsgfile, "%s%s", CHROOT, msgfile) < 0)
		log_fatal("proc_getmsg: asprintf");

	/* XXX: race. if the client disconnects, the engine could delete an archive out
	 * from under us. if this happens, we will fail silently. if the archive is deleted
	 * after the below line, but before the engine replies to the frontend, we should wind
	 * up receiving a signature for a non-existent key, and can silently discard it
	 */
	if ((tosign = archive_fromfile(key, absmsgfile)) == NULL) goto end;

	signature = crypto_takesignature(tosign);
	parent_replytoengine(IMSG_SIGNATURE, key, signature);
	log_writex(LOGTYPE_DEBUG, "signature reply sent");

	archive_teardown(tosign);
	free(signature);

end:
	free(absmsgfile);
	free(msgfile);

	(void)fd;
}

int
main(int argc, char *argv[])
{
	struct proc	*parent, *frontend, *engine;
	int		 ch;	

	while ((ch = getopt(argc, argv, "dhv")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 0) usage();
	else if (geteuid() != 0) errx(1, "need root privileges");

	empty_directory(CHROOT ARCHIVES);
	empty_directory(CHROOT SIGNATURES);
	empty_directory(CHROOT MESSAGES);

	parent = proc_new(PROC_PARENT);
	if (parent == NULL) err(1, "proc_new -> parent process");

	proc_handlesigev(parent, SIGEV_INT, parent_signal);
	proc_handlesigev(parent, SIGEV_TERM, parent_signal);
	proc_setuser(parent, USER);

	/* XXX: defer frontend privilege drop until after it launches,
	 * because we have to load privileged data e.g. tls context
	 */
	frontend = proc_new(PROC_FRONTEND);
	if (frontend == NULL) err(1, "proc_new -> frontend process");

	proc_handlesigev(frontend, SIGEV_INT, frontend_signal);
	proc_handlesigev(frontend, SIGEV_TERM, frontend_signal);

	engine = proc_new(PROC_ENGINE);
	if (engine == NULL) err(1, "proc_new -> engine process");

	proc_handlesigev(engine, SIGEV_INT, engine_signal);
	proc_handlesigev(engine, SIGEV_TERM, engine_signal);
	proc_setchroot(engine, CHROOT);
	proc_setuser(engine, USER);

	log_init();
	log_writex(LOGTYPE_DEBUG, "verbose logging enabled");

	/* drop the solid rocket boosters... */
	if (!debug && daemon(0, 0) < 0) err(1, "daemonize");

	/* and fire the main engines */
	proc_startall(parent, frontend, engine);

	log_writex(LOGTYPE_MSG, "startup");

	if (unveil(CHROOT SIGNATURES, "rwc") < 0)
		log_fatal("unveil " CHROOT SIGNATURES);

	if (unveil(CHROOT ARCHIVES, "r") < 0)
		log_fatal("unveil " CHROOT ARCHIVES);

	if (unveil(CRYPTO_SIGNIFY, "x") < 0)
		log_fatal("unveil " CRYPTO_SIGNIFY);

	if (unveil("/usr/libexec/ld.so", "r") < 0)
		log_fatal("unveil ld.so");

	if (pledge("stdio rpath wpath cpath proc exec", NULL) < 0)
		log_fatal("pledge");

	myproc_listen(PROC_FRONTEND, nothing);
	myproc_listen(PROC_ENGINE, proc_getmsg);

	event_dispatch();

	/* never hit */
	return 0;
}
