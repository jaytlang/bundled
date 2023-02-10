/* imaged.h
 * (c) jay lang, 2023
 * global state, prototypes, definitions
 */

#ifndef IMAGED_H
#define IMAGED_H

#include <sys/types.h>

#define USER		"_imaged"
#define CHROOT		"/var/imaged"

/* changing around for unit tests */
#ifndef MESSAGES
#define MESSAGES	"/messages"
#endif /* MESSAGES */

#ifndef ARCHIVES
#define ARCHIVES	"/archives"
#endif /* ARCHIVES */

#ifndef SIGNATURES
#define SIGNATURES	"/signatures"
#endif /* SIGNATURES */

#define	ERRSTRSIZE	500
#define MAXNAMESIZE	1024
#define MAXFILESIZE	1048576
#define MAXSIGSIZE	512

extern char *__progname;
extern int debug, verbose;

struct event;
struct imsgbuf;
struct sockaddr_in;
struct timeval;

/* log.c */

#define LOGTYPE_MSG	0
#define LOGTYPE_DEBUG	1
#define LOGTYPE_WARN	2
#define LOGTYPE_MAX	3

void		log_init(void);
void		log_write(int, const char *, ...);
void		log_writex(int, const char *, ...);
__dead void	log_fatal(const char *, ...);
__dead void	log_fatalx(const char *, ...);

/* ipcmsg.c */

struct ipcmsg;

struct ipcmsg	*ipcmsg_new(uint32_t, char *);
void		 ipcmsg_teardown(struct ipcmsg *);

uint32_t	 ipcmsg_getkey(struct ipcmsg *);
char		*ipcmsg_getmsg(struct ipcmsg *);

char		*ipcmsg_marshal(struct ipcmsg *, uint16_t *);
struct ipcmsg	*ipcmsg_unmarshal(char *, uint16_t);

/* buffer.c */

int		 buffer_open(void);
int		 buffer_close(int);
int		 buffer_truncate(int, off_t);
ssize_t	 	 buffer_read(int, void *, size_t);
ssize_t	 	 buffer_write(int, const void *, size_t);
off_t	  	 buffer_seek(int, off_t, int);

/* netmsg.c */

struct netmsg;

#define NETOP_UNUSED	0
#define NETOP_SIGN	1
#define NETOP_WRITE	2
#define NETOP_BUNDLE	3
#define NETOP_HEARTBEAT	4
#define NETOP_ACK	5
#define NETOP_ERROR	6
#define NETOP_MAX	7

struct netmsg	*netmsg_new(uint8_t);
struct netmsg	*netmsg_loadweakly(char *);

void		 netmsg_teardown(struct netmsg *);

const char	*netmsg_error(struct netmsg *);
void		 netmsg_clearerror(struct netmsg *);

ssize_t		 netmsg_write(struct netmsg *, void *, size_t);
ssize_t		 netmsg_read(struct netmsg *, void *, size_t);
ssize_t		 netmsg_seek(struct netmsg *, ssize_t, int);
int		 netmsg_truncate(struct netmsg *, ssize_t);

uint8_t		 netmsg_gettype(struct netmsg *);

char		*netmsg_getlabel(struct netmsg *);
int		 netmsg_setlabel(struct netmsg *, char *);

char		*netmsg_getdata(struct netmsg *, uint64_t *);
int		 netmsg_setdata(struct netmsg *, char *, uint64_t);

int		 netmsg_isvalid(struct netmsg *, int *);

/* proc.c */

#define PROC_ROOT       0
#define PROC_FRONTEND   1
#define PROC_ENGINE     2
#define PROC_MAX        3

#define SIGEV_HUP       0
#define SIGEV_INT       1
#define SIGEV_TERM      2
#define SIGEV_CHLD      3
#define SIGEV_MAX       4

#define IMSG_HELLO              0
#define IMSG_INITFD             1
#define IMSG_MAX                2

struct proc;

struct proc	*proc_new(int);

void		 proc_handlesigev(struct proc *, int, void (*)(int, short, void *));
void		 proc_setchroot(struct proc *, char *);
void		 proc_setuser(struct proc *, char *);

void		 proc_startall(struct proc *, struct proc *, struct proc *);

void    	 myproc_send(int, int, int, struct ipcmsg *);
void    	 myproc_listen(int, void (*cb)(int, int, struct ipcmsg *));
void    	 myproc_stoplisten(int);

void		 frontend_launch(void);
void		 engine_launch(void);

/* conn.c */

#define CONN_PORT	443

struct conn;

void			 conn_listen(void (*)(struct conn *));
void			 conn_teardown(struct conn *);
void			 conn_teardownall(void);

void			 conn_receive(struct conn *, void (*)(struct conn *, struct netmsg *));
void			 conn_stopreceiving(struct conn *);

void			 conn_settimeout(struct conn *, struct timeval *, void (*)(struct conn *));
void			 conn_canceltimeout(struct conn *);

void		 	 conn_send(struct conn *, struct netmsg *);

int		 	 conn_getfd(struct conn *);
struct sockaddr_in	*conn_getsockpeer(struct conn *);

/* msgqueue.c */

struct msgqueue;

struct msgqueue	*msgqueue_new(struct conn *, void (*)(struct msgqueue *, struct conn *));
void		 msgqueue_teardown(struct msgqueue *);

void		 msgqueue_append(struct msgqueue *, struct netmsg *);
void		 msgqueue_deletehead(struct msgqueue *);

struct netmsg	*msgqueue_gethead(struct msgqueue *);
size_t		 msgqueue_getcachedoffset(struct msgqueue *);
int		 msgqueue_setcachedoffset(struct msgqueue *, size_t);

/* general use */
__attribute__((unused)) static void
nothing(int a, int b, struct ipcmsg *c)
{
	(void)a;
	(void)b;
	(void)c;
}

/* archive.c */

struct archive;

struct archive	*archive_new(uint32_t);
struct archive	*archive_fromfile(uint32_t, char *);
struct archive	*archive_fromkey(uint32_t);

void		 archive_teardown(struct archive *);

int		 archive_addfile(struct archive *, char *, char *, size_t);
int		 archive_hasfile(struct archive *, char *);
char		*archive_loadfile(struct archive *, char *, size_t *);

int		 archive_setsignature(struct archive *, char *);
int		 archive_verifysignature(struct archive *, char *);

int		 archive_isvalid(struct archive *, int);

#endif /* IMAGED_H */
