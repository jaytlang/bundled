/* bundled.h
 * (c) jay lang, 2023
 * global state, prototypes, definitions
 */

#ifndef IMAGED_H
#define IMAGED_H

#include <sys/types.h>

/* parse.y */

#define CONFIG_DEFAULTPATH	"/etc/bundled.conf"

struct config {
	char	*chroot;		/* chroot "/var/bundled" */
	char	*user;			/* user _bundled */

	char	*archive_path;		/* archiver workdir "/var/bundled/archive" */
	char	*message_path;		/* server workdir  "/var/bundled/messages" */
	char	*signature_path;	/* notary workdir "/var/bundled/signatures" */

	uint16_t	 server_port;		/* server listen on 443 */
	uint32_t	 server_timeout;	/* server timeout 1 */
	char		*server_ca_path;	/* server ca "/etc/ssl/authority/serverchain.pem" */
	char		*server_cert_path;	/* server certificate "/etc/ssl/server.pem" */
	char		*server_key_path;	/* server private key "/etc/ssl/private/server.key" */

	char		*signature_privkey;	/* notary private key "/etc/signify/bundled.sec */
	char		*signature_pubkey;	/* notary public key "/etc/signify/bundled/pub */

	uint64_t	 archive_maxnamesize;		/* archive max name size 1024 */
	uint64_t	 archive_maxfilesize;		/* archive max file size 1048576 */
	uint64_t	 archive_maxsignaturesize;	/* archive max signature size 177 */
	uint64_t	 archive_maxfiles;		/* archive max files 100 */
};

void			config_parse(char *fpath);
extern struct config	config;

#define USER		(config.user)
#define CHROOT		(config.chroot)

#define MESSAGES	(config.message_path)
#define ARCHIVES	(config.archive_path)
#define SIGNATURES	(config.signature_path)

/* these settings are shared across both frontend and engine,
 * so i figured i'd place them here
 */
#define MAXNAMESIZE	(config.archive_maxnamesize)
#define MAXFILESIZE	(config.archive_maxfilesize)
#define MAXSIGSIZE	(config.archive_maxsignaturesize)

#define	ERRSTRSIZE	2048
#define BLOCKSIZE	1048576

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
#define NETOP_GETBUNDLE	3

#define NETOP_HEARTBEAT	4

#define NETOP_BUNDLE	5
#define NETOP_ACK	6
#define NETOP_ERROR	7

#define NETOP_MAX	8

struct netmsg	*netmsg_new(uint8_t);
struct netmsg	*netmsg_loadweakly(char *);

void		 netmsg_retain(struct netmsg *);
void		 netmsg_teardown(struct netmsg *);

const char	*netmsg_error(struct netmsg *);
void		 netmsg_clearerror(struct netmsg *);

ssize_t		 netmsg_write(struct netmsg *, void *, size_t);
ssize_t		 netmsg_read(struct netmsg *, void *, size_t);
ssize_t		 netmsg_seek(struct netmsg *, ssize_t, int);
int		 netmsg_truncate(struct netmsg *, ssize_t);

uint8_t		 netmsg_gettype(struct netmsg *);
char		*netmsg_getpath(struct netmsg *);

char		*netmsg_getlabel(struct netmsg *);
int		 netmsg_setlabel(struct netmsg *, char *);

char		*netmsg_getdata(struct netmsg *, uint64_t *);
int		 netmsg_setdata(struct netmsg *, char *, uint64_t);

int		 netmsg_isvalid(struct netmsg *, int *);

/* proc.c */

#define PROC_PARENT	0
#define PROC_FRONTEND  	1
#define PROC_ENGINE   	2
#define PROC_MAX      	3

#define SIGEV_HUP       0
#define SIGEV_INT       1
#define SIGEV_TERM      2
#define SIGEV_MAX       3

#define IMSG_HELLO              0
#define IMSG_INITFD             1

#define IMSG_NEWARCHIVE		2
#define IMSG_ADDFILE		3
#define IMSG_WANTSIGN		4
#define IMSG_GETBUNDLE		5
#define IMSG_KILLARCHIVE	6

#define IMSG_NEWARCHIVEACK	7
#define IMSG_ADDFILEACK		8
#define IMSG_WANTSIGNACK	9
#define IMSG_BUNDLE		10
#define IMSG_ENGINEERROR	11	

#define IMSG_SIGNARCHIVE	12
#define IMSG_SIGNATURE		13

#define IMSG_MAX                14

#define FRONTEND_TIMEOUT	(config.server_timeout)

struct proc;

struct proc	*proc_new(int);

void		 proc_handlesigev(struct proc *, int, void (*)(int, short, void *));
void		 proc_setchroot(struct proc *, char *);
void		 proc_setuser(struct proc *, char *);

void		 proc_startall(struct proc *, struct proc *, struct proc *);

void    	 myproc_send(int, int, int, struct ipcmsg *);
void    	 myproc_listen(int, void (*cb)(int, int, struct ipcmsg *));
void    	 myproc_stoplisten(int);
int		 myproc_ischrooted(void);


void		 frontend_launch(void);
__dead void	 frontend_signal(int, short, void *);

void		 engine_launch(void);
__dead void	 engine_signal(int, short, void *);

/* conn.c */

#define CONN_PORT	(config.server_port)

#define CONN_CA_PATH	(config.server_ca_path)
#define CONN_CERT	(config.server_cert_path)
#define CONN_KEY	(config.server_key_path)

struct conn;

void			 conn_listen(void (*)(struct conn *));
void			 conn_teardown(struct conn *);
void			 conn_teardownall(void);

void			 conn_receive(struct conn *, void (*)(struct conn *, struct netmsg *));
void			 conn_stopreceiving(struct conn *);

void			 conn_setteardowncb(struct conn *, void (*)(struct conn *));

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

#define ARCHIVE_MAXFILES	(config.archive_maxfiles)

struct archive;

struct archive	*archive_new(uint32_t);
struct archive	*archive_fromfile(uint32_t, char *);
struct archive	*archive_fromkey(uint32_t);

void		 archive_teardown(struct archive *);
void		 archive_teardownall(void);

int		 archive_addfile(struct archive *, char *, char *, size_t);
int		 archive_hasfile(struct archive *, char *);
char		*archive_loadfile(struct archive *, char *, size_t *);

uint32_t	 archive_getcrc32(struct archive *);
char		*archive_getsignature(struct archive *);
void		 archive_writesignature(struct archive *, char *);

char		*archive_error(struct archive *);
int		 archive_isvalid(struct archive *);

char		*archive_getpath(struct archive *);

/* crypto.c */

#define CRYPTO_SIGNIFY		"/usr/bin/signify"

#define CRYPTO_PUBKEY		(config.signature_pubkey)
#define CRYPTO_SECKEY		(config.signature_privkey)

char		*crypto_takesignature(struct archive *);
int		 crypto_verifysignature(struct archive *);

#endif /* IMAGED_H */
