/* imaged true frontend
 * (c) jay lang, 2022
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/tree.h>

#include <arpa/inet.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imaged.h"

#define FRONTEND_TIMEOUT	1
#define FRONTEND_ADDRESSSIZE	16

struct activeconn {
	struct conn		*c;
	uint32_t	 	 backendkey;

	int			 shouldheartbeat;
	char			 peer[FRONTEND_ADDRESSSIZE];

	SLIST_ENTRY(activeconn)	 freelist_entries;
	RB_ENTRY(activeconn)	 bykey_entries;
	RB_ENTRY(activeconn)	 byptr_entries;
};

SLIST_HEAD(freelist, activeconn);
RB_HEAD(activekeytree, activeconn);
RB_HEAD(activeptrtree, activeconn);

static struct activeconn	*activeconn_new(struct conn *);
static void			 activeconn_handleteardown(struct conn *);

static struct activeconn	*activeconn_bykey(uint32_t);
static struct activeconn	*activeconn_byptr(struct conn *);

static int			 activeconn_comparekeys(struct activeconn *, struct activeconn *);
static int			 activeconn_compareptrs(struct activeconn *, struct activeconn *);

static void			 activeconn_errortoclient(struct activeconn *, const char *, ...);
static void			 activeconn_requesttoengine(struct activeconn *, int, char *);

/* misc. helpers
 * XXX: move this guy? generalize? the engine
 * does a similar operation except it loads weakly
 */
static struct netmsg		*netmsg_withfilefromipcmsg(uint8_t, char *, struct ipcmsg *);


static void	conn_accept(struct conn *);
static void	conn_timeout(struct conn *);
static void	conn_getmsg(struct conn *, struct netmsg *);
static void	proc_getmsg(int, int, struct ipcmsg *);

static uint32_t			maxkey = 0;

static struct freelist		freeconns = SLIST_HEAD_INITIALIZER(freeconns);
static struct activekeytree	connsbykey = RB_INITIALIZER(&connsbykey);
static struct activeptrtree	connsbyptr = RB_INITIALIZER(&connsbyptr);


RB_PROTOTYPE_STATIC(activekeytree, activeconn, bykey_entries, activeconn_comparekeys)
RB_PROTOTYPE_STATIC(activeptrtree, activeconn, byptr_entries, activeconn_compareptrs)

RB_GENERATE_STATIC(activekeytree, activeconn, bykey_entries, activeconn_comparekeys)
RB_GENERATE_STATIC(activeptrtree, activeconn, byptr_entries, activeconn_compareptrs)


static struct activeconn *
activeconn_new(struct conn *c)
{
	struct activeconn	*out = NULL;
	struct sockaddr_in	*peer;

	if (!SLIST_EMPTY(&freeconns)) {
		out = SLIST_FIRST(&freeconns);
		SLIST_REMOVE_HEAD(&freeconns, freelist_entries);

	} else {
		if (maxkey == UINT32_MAX) {
			errno = EAGAIN;
			goto end;
		}

		/* explicitly zero data structure fields */
		out = calloc(1, sizeof(struct activeconn));
		if (out == NULL)
			log_fatal("activeconn_new: malloc");

		out->backendkey = ++maxkey;
	}

	peer = conn_getsockpeer(c);

	if (peer == NULL)
		log_fatal("activeconn_new: conn_getsockpeer");
	else if (inet_ntop(AF_INET, peer, out->peer, FRONTEND_ADDRESSSIZE) == NULL)
		log_fatal("activeconn_new: inet_ntop");

	free(peer);

	out->c = c;
	RB_INSERT(activekeytree, &connsbykey, out);
	RB_INSERT(activeptrtree, &connsbyptr, out);
end:
	return out;
}

static void
activeconn_handleteardown(struct conn *c)
{
	struct activeconn	*ac;

	ac = activeconn_byptr(c);

	activeconn_requesttoengine(ac, IMSG_KILLARCHIVE, NULL);

	RB_REMOVE(activekeytree, &connsbykey, ac);
	RB_REMOVE(activeptrtree, &connsbyptr, ac);

	ac->c = NULL;

	SLIST_INSERT_HEAD(&freeconns, ac, freelist_entries);
}

static struct activeconn *
activeconn_bykey(uint32_t key)
{
	struct activeconn	*out, dummy;

	dummy.backendkey = key;
	out = RB_FIND(activekeytree, &connsbykey, &dummy);

	if (out == NULL)
		log_fatal("activeconn_bykey: no such key %u", key);

	return out;
}

static struct activeconn *
activeconn_byptr(struct conn *c)
{
	struct activeconn	*out, dummy;

	dummy.c = c;
	out = RB_FIND(activeptrtree, &connsbyptr, &dummy);

	if (out == NULL)
		log_fatal("activeconn_byptr: no such conn %p", c);

	return out;
}

static int
activeconn_comparekeys(struct activeconn *a, struct activeconn *b)
{
	int	result = 0;	

	if (a->backendkey > b->backendkey) result = 1;
	if (a->backendkey < b->backendkey) result = -1;

	return result;
}

static int
activeconn_compareptrs(struct activeconn *a, struct activeconn *b)
{
	int	result = 0;

	if ((uintptr_t)a->c > (uintptr_t)b->c) result = 1;
	if ((uintptr_t)a->c < (uintptr_t)b->c) result = -1;

	return result;
}

static void
activeconn_errortoclient(struct activeconn *ac, const char *fmt, ...)
{
	struct netmsg	*response;
	char		*label;
	va_list		 ap;

	va_start(ap, fmt);

	if (vasprintf(&label, fmt, ap) < 0)
		log_fatal("activeconn_errortoclient: asprintf");
		
	response = netmsg_new(NETOP_ERROR);
	if (response == NULL)
		log_fatal("activeconn_errortoclient: netmsg_new");

	if (netmsg_setlabel(response, label) < 0)
		log_fatalx("activeconn_errortoclient: netmsg_setlabel: %s", netmsg_error(response));

	conn_send(ac->c, response);

	free(label);
	va_end(ap);
}

static void
activeconn_requesttoengine(struct activeconn *ac, int request, char *label)
{
	struct ipcmsg	*imsg;

	imsg = ipcmsg_new(ac->backendkey, label);
	if (imsg == NULL) log_fatal("activeconn_requesttoengine: ipcmsg_new");

	myproc_send(PROC_ENGINE, request, -1, imsg);
	ipcmsg_teardown(imsg);

	conn_stopreceiving(ac->c);
}

static struct netmsg *
netmsg_withfilefromipcmsg(uint8_t type, char *label, struct ipcmsg *m)
{
	struct netmsg	*out;
	char		*file;

	char		*buf;
	uint64_t	 bebufsize;
	ssize_t		 bufsize;
	int		 fd, unrecoverable;

	out = netmsg_new(type);
	if (out == NULL) log_fatal("netmsg_withfilefromipcmsg: netmsg_new");

	if (netmsg_setlabel(out, label) < 0)
		log_fatalx("netmsg_withfilefromipcmsg: netmsg_setlabel: %s", netmsg_error(out));

	file = ipcmsg_getmsg(m);	

	fd = open(file, O_RDONLY);
	if (fd < 0) log_fatal("netmsg_withfilefromipcmsg: open");

	if ((bufsize = lseek(fd, 0, SEEK_END)) < 0 || lseek(fd, 0, SEEK_SET) < 0)
		log_fatal("netmsg_withfilefromipc: lseek");

	bebufsize = htobe64((uint16_t)bufsize);

	buf = reallocarray(NULL, (size_t)bufsize, sizeof(char));
	if (buf == NULL) log_fatal("netmsg_withfilefromipc: reallocarray");

	if (netmsg_seek(out, sizeof(uint64_t), SEEK_END) < 0)
		log_fatal("netmsg_withfilefromipcmsg: netmsg_seek");

	if (netmsg_write(out, &bebufsize, sizeof(uint64_t)) < 0)
		log_fatal("netmsg_withfilefromipcmsg: netmsg_write message size");

	if (netmsg_write(out, buf, (size_t)bufsize) < 0)
		log_fatal("netmsg_withfilefromipcmsg: netmsg_write message");

	free(buf);
	close(fd);
	free(file);

	/* XXX: sanity check */
	if (debug && !netmsg_isvalid(out, &unrecoverable))
		log_fatalx("netmsg_withfilefromipcmsg: netmsg not valid: %s",
			netmsg_error(out));

	return out;
}


static void
conn_accept(struct conn *c)
{
	struct activeconn	*ac;
	struct timeval		 tv;

	ac = activeconn_new(c);
	if (ac == NULL) {
		log_write(LOGTYPE_WARN, "conn_accept: activeconn_new");
		conn_teardown(c);
		return;
	}

	tv.tv_sec = FRONTEND_TIMEOUT;
	tv.tv_usec = 0;

	conn_settimeout(ac->c, &tv, conn_timeout);
	conn_setteardowncb(ac->c, activeconn_handleteardown);

	activeconn_requesttoengine(ac, IMSG_NEWARCHIVE, NULL);
}

static void
conn_timeout(struct conn *c)
{
	struct activeconn	*ac;
	struct netmsg		*heartbeat;

	ac = activeconn_byptr(c);
	if (ac->shouldheartbeat) conn_teardown(ac->c);
	else {
		ac->shouldheartbeat = 1;

		heartbeat = netmsg_new(NETOP_HEARTBEAT);
		if (heartbeat == NULL) log_fatal("conn_timeout: netmsg_new");

		conn_send(ac->c, heartbeat);

		/* cute little reboot */
		conn_stopreceiving(ac->c);
		conn_receive(ac->c, conn_getmsg);
	}
}

static void
conn_getmsg(struct conn *c, struct netmsg *m)
{
	struct activeconn	*ac;
	char			*msgpath;

	ac = activeconn_byptr(c);

	if (m == NULL || strlen(netmsg_error(m)) > 0) {

		if (m == NULL)
			log_writex(LOGTYPE_WARN,
				   "conn_getmsg: peer %s sent unintelligble message",
				   ac->peer);
		else
			log_writex(LOGTYPE_WARN,
			   	   "conn_getmsg: peer %s sent bad message: %s",
			   	   ac->peer,
			   	   netmsg_error(m));
		
		activeconn_errortoclient(ac, "received bad message: %s",
			(m == NULL) ? "unintelligble" : netmsg_error(m));
	}

	switch (netmsg_gettype(m)) {

	case NETOP_SIGN:
		activeconn_requesttoengine(ac, IMSG_PLEASESIGN, NULL);
		break;

	case NETOP_WRITE:
		msgpath = netmsg_getpath(m);
		log_writex(LOGTYPE_DEBUG, "have a netmsg at path %s", msgpath);

		netmsg_persistfile(m);
		activeconn_requesttoengine(ac, IMSG_ADDFILE, msgpath);

		free(msgpath);
		break;

	case NETOP_HEARTBEAT:
		ac->shouldheartbeat = 0;
		break;

	default:
		log_writex(LOGTYPE_WARN,
			   "conn_getmsg: peer %s sent bad message type %u",
			   ac->peer, 
			   netmsg_gettype(m));

		activeconn_errortoclient(ac, "received bad message type %u", netmsg_gettype(m));
	}
}

static void
proc_getmsg(int type, int fd, struct ipcmsg *msg)
{
	struct netmsg		*response;	
	struct activeconn	*ac;
	char			*archivepath;

	ac = activeconn_bykey(ipcmsg_getkey(msg));

	log_writex(LOGTYPE_DEBUG, "parent response hours (message type = %d)", type);

	switch (type) {

	case IMSG_NEWARCHIVEACK:
		conn_receive(ac->c, conn_getmsg);
		break;

	case IMSG_ADDFILEACK:
		response = netmsg_new(NETOP_ACK);
		if (response == NULL) log_fatal("proc_getmsg: netmsg_new");

		conn_send(ac->c, response);
		conn_receive(ac->c, conn_getmsg);
		break;

	case IMSG_SIGNEDBUNDLE:
		archivepath = ipcmsg_getmsg(msg);

		response = netmsg_withfilefromipcmsg(NETOP_BUNDLE,
			archivepath, msg);

		conn_send(ac->c, response);
		conn_receive(ac->c, conn_getmsg);

		netmsg_teardown(response);
		free(archivepath);
		break;

	default:
		log_fatalx("proc_getmsg: unexpected message type %d from engine", type);
	}

	(void)fd;
}

void
frontend_launch(void)
{
	struct passwd *user;

	conn_listen(conn_accept);

	if ((user = getpwnam(USER)) == NULL)
		log_fatalx("no such user %s", USER);

	if (unveil(CONN_CA_PATH, "r") < 0)
		log_fatal("unveil %s", CONN_CA_PATH);

	if (unveil(CONN_CERT, "r") < 0)
		log_fatal("unveil %s", CONN_CERT);

	if (unveil(CHROOT MESSAGES, "rwc") < 0)
		log_fatal("unveil %s", CHROOT MESSAGES);

	if (unveil(CHROOT ARCHIVES, "r") < 0)
		log_fatal("unveil %s", CHROOT ARCHIVES);

	if (setresgid(user->pw_gid, user->pw_gid, user->pw_gid) < 0)
		log_fatal("setresgid");
	else if (setresuid(user->pw_uid, user->pw_uid, user->pw_uid) < 0)
		log_fatal("setresuid");

	if (pledge("stdio rpath wpath cpath inet", "") < 0)
		log_fatal("pledge");

	myproc_listen(PROC_ROOT, nothing);
	myproc_listen(PROC_ENGINE, proc_getmsg);

	event_dispatch();
	conn_teardownall();
}
