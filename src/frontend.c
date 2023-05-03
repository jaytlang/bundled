/* bundled true frontend
 * (c) jay lang, 2022
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/tree.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bundled.h"

#define FRONTEND_ADDRESSSIZE	16

struct activeconn {
	struct conn		*c;
	uint32_t	 	 backendkey;

	int			 shouldheartbeat;
	char			 peer[FRONTEND_ADDRESSSIZE];

	struct netmsg		*pendingmsg;

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

/* misc. helper
 * XXX: move this guy? generalize?
 */
static struct netmsg		*conn_makemsgfromarchive(char *);


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
	else if (inet_ntop(AF_INET, &peer->sin_addr, out->peer, FRONTEND_ADDRESSSIZE) == NULL)
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
	ac->shouldheartbeat = 0;

	if (ac->pendingmsg != NULL) {
		log_writex(LOGTYPE_DEBUG, "tearing down pending message for peer %s", ac->peer);
		netmsg_teardown(ac->pendingmsg);
		ac->pendingmsg = NULL;
	}

	SLIST_INSERT_HEAD(&freeconns, ac, freelist_entries);
}

static struct activeconn *
activeconn_bykey(uint32_t key)
{
	struct activeconn	*out, dummy;

	dummy.backendkey = key;
	out = RB_FIND(activekeytree, &connsbykey, &dummy);

	if (out == NULL) errno = EINVAL;
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
	log_writex(LOGTYPE_DEBUG, "sent error to cilent");

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
conn_makemsgfromarchive(char *label)
{
	struct netmsg	*out;
	int		 fd, unrecoverable;

	char		*reallabel;
	size_t		 reallabelsize;

	char		*buf;
	uint64_t	 bebufsize;
	ssize_t		 bufsize;

	out = netmsg_new(NETOP_BUNDLE);
	if (out == NULL) log_fatal("conn_makemsgfromarchive: netmsg_new");

	reallabelsize = strlen(label) + strlen(CHROOT) + 1;

	reallabel = reallocarray(NULL, reallabelsize, sizeof(char));
	if (reallabel == NULL) log_fatal("conn_makemsgfromarchive: reallocarray");

	strlcpy(reallabel, CHROOT, reallabelsize);
	strlcat(reallabel, label, reallabelsize);

	netmsg_setlabel(out, reallabel);

	fd = open(reallabel, O_RDONLY);
	if (fd < 0) log_fatal("conn_makemsgfromarchive: open %s", reallabel);

	if ((bufsize = lseek(fd, 0, SEEK_END)) < 0 || lseek(fd, 0, SEEK_SET) < 0)
		log_fatal("netmsg_withfilefromipc: lseek");

	bebufsize = htobe64((uint64_t)bufsize);

	buf = reallocarray(NULL, (size_t)bufsize, sizeof(char));
	if (buf == NULL) log_fatal("netmsg_withfilefromipc: reallocarray");

	if (read(fd, buf, bufsize) < 0)
		log_fatal("conn_makemsgfromarchive: read");

	if (netmsg_seek(out, 0, SEEK_END) < 0)
		log_fatal("conn_makemsgfromarchive: netmsg_seek");

	if (netmsg_write(out, &bebufsize, sizeof(uint64_t)) < 0)
		log_fatal("conn_makemsgfromarchive: netmsg_write message size");

	if (netmsg_write(out, buf, (size_t)bufsize) < 0)
		log_fatal("conn_makemsgfromarchive: netmsg_write message");

	free(buf);
	free(reallabel);

	close(fd);

	/* XXX: sanity check */
	if (debug && !netmsg_isvalid(out, &unrecoverable))
		log_fatalx("conn_makemsgfromarchive: netmsg not valid: %s",
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
	if (ac->shouldheartbeat) {
		log_writex(LOGTYPE_DEBUG, "heartbeat timeout");
		conn_teardown(ac->c);
	} else {
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
	ac->shouldheartbeat = 0;

	if (m == NULL || strlen(netmsg_error(m)) > 0) {

		if (m == NULL)
			log_writex(LOGTYPE_DEBUG,
				   "conn_getmsg: peer %s sent unintelligble message",
				   ac->peer);
		else
			log_writex(LOGTYPE_DEBUG,
			   	   "conn_getmsg: peer %s sent bad message: %s",
			   	   ac->peer,
			   	   netmsg_error(m));
		
		activeconn_errortoclient(ac, "received bad message: %s",
			(m == NULL) ? "unintelligble" : netmsg_error(m));

		return;
	}

	switch (netmsg_gettype(m)) {

	case NETOP_SIGN:
		activeconn_requesttoengine(ac, IMSG_WANTSIGN, NULL);
		break;

	case NETOP_WRITE:
		msgpath = netmsg_getpath(m);
		netmsg_retain(m);
		ac->pendingmsg = m;

		activeconn_requesttoengine(ac, IMSG_ADDFILE, msgpath);

		free(msgpath);
		break;

	case NETOP_GETBUNDLE:
		activeconn_requesttoengine(ac, IMSG_GETBUNDLE, NULL);
		break;

	case NETOP_HEARTBEAT:
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
	char			*archivepath, *error;

	ac = activeconn_bykey(ipcmsg_getkey(msg));

	if (ac == NULL) {
		if (type == IMSG_ENGINEERROR) {
			log_writex(LOGTYPE_DEBUG, "teardown race observed");
			return;
		}

		log_fatalx("proc_getmsg: received engine reply for null connection");
	}

	switch (type) {

	case IMSG_NEWARCHIVEACK:
		break;

	case IMSG_ADDFILEACK:
		response = netmsg_new(NETOP_ACK);
		if (response == NULL) log_fatal("proc_getmsg: netmsg_new");

		netmsg_teardown(ac->pendingmsg);
		ac->pendingmsg = NULL;

		conn_send(ac->c, response);
		break;

	case IMSG_WANTSIGNACK:
		response = netmsg_new(NETOP_ACK);
		if (response == NULL) log_fatal("proc_getmsg: netmsg_new");

		conn_send(ac->c, response);
		break;

	case IMSG_BUNDLE:
		archivepath = ipcmsg_getmsg(msg);

		response = conn_makemsgfromarchive(archivepath);
		conn_send(ac->c, response);

		free(archivepath);
		break;

	case IMSG_ENGINEERROR:
		error = ipcmsg_getmsg(msg);	

		activeconn_errortoclient(ac, "%s", error);
		free(error);

		if (ac->pendingmsg != NULL) {
			netmsg_teardown(ac->pendingmsg);
			ac->pendingmsg = NULL;
		}

		break;

	default:
		log_fatalx("proc_getmsg: unexpected message type %d from engine", type);
	}

	conn_receive(ac->c, conn_getmsg);

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

	if (unveil(MESSAGES, "rwc") < 0)
		log_fatal("unveil %s", MESSAGES);

	if (unveil(ARCHIVES, "r") < 0)
		log_fatal("unveil %s", ARCHIVES);

	if (setresgid(user->pw_gid, user->pw_gid, user->pw_gid) < 0)
		log_fatal("setresgid");
	else if (setresuid(user->pw_uid, user->pw_uid, user->pw_uid) < 0)
		log_fatal("setresuid");

	if (pledge("stdio rpath wpath cpath inet", "") < 0)
		log_fatal("pledge");

	myproc_listen(PROC_PARENT, nothing);
	myproc_listen(PROC_ENGINE, proc_getmsg);

	event_dispatch();
	conn_teardownall();
}

__dead void
frontend_signal(int signal, short event, void *arg)
{
	conn_teardownall();
	exit(0);

	(void)signal;
	(void)event;
	(void)arg;
}
