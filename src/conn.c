/* imaged connection management
 * (c) jay lang, 2023
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/tree.h>

#include <arpa/inet.h>

#include <errno.h>
#include <event.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <tls.h>

#include "imaged.h"

#define CONN_CERT		"/etc/ssl/server.pem"
#define CONN_KEY		"/etc/ssl/private/server.key"
#define CONN_CA_PATH		"/etc/ssl/authority"

#define CONN_LISTENBACKLOG	128
#define CONN_MTU		1500

struct globalcontext {
	uint8_t			*tls_key;	
	size_t 			 tls_keysize;

	struct tls_config	*tls_globalcfg;
	struct tls		*tls_serverctx;

	int			 listen_fd;
	struct event		 listen_event;
};

static void	globalcontext_init(void);
static void	globalcontext_teardown(void);

static void	globalcontext_listen(void (*)(struct conn *));
static void	globalcontext_accept(int, short, void *);
static void	globalcontext_stoplistening(void);

static int	globalcontext_initialized = 0;

static struct globalcontext	globalcontext;

struct conn {
	int			  sockfd;
	struct sockaddr_in	  peer;

	struct tls		 *tls_context;
	struct event		  event_receive;
	struct event		  event_timeout;

	struct netmsg		 *incoming_message;
	struct netmsg		 *outgoing_message;

	void			(*cb_receive)(struct conn *, struct netmsg *);
	void			(*cb_timeout)(struct conn *);

	RB_ENTRY(conn)		  entries;
};

RB_HEAD(conntree, conn);

static struct conn		*conn_new(int, struct sockaddr_in *, struct tls *);
static int			 conn_compare(struct conn *, struct conn *);

static void			 conn_doreceive(int, short, void *);
static void			 conn_dosend(int, short, void *);
static void			 conn_catchtimeout(int, short, void *);

static struct conntree allcons = RB_INITIALIZER(&allcons);

RB_PROTOTYPE_STATIC(conntree, conn, entries, conn_compare);
RB_GENERATE_STATIC(conntree, conn, entries, conn_compare);

static void
globalcontext_init(void)
{
	struct tls_config	*globalcfg;
	struct tls		*serverctx;

	uint8_t			*key;
	size_t			 keysize;

	globalcfg = tls_config_new();
	if (out == NULL)
		log_fatalx("globalcontext_init: can't allocate tls globalcfg");

	if (tls_config_set_capath(out, CONN_CA_PATH) < 0) {
		tls_config_free(globalcfg);
		log_fatalx("globalcontext_init: can't set ca path to " CONN_CA_PATH);
	}

	if (tls_config_set_cert_file(out, CONN_CERT) < 0) {
		tls_config_free(globalcfg);
		log_fatalx("globalcontext_init: can't set cert file to " CONN_CERT);
	}

	if ((key = tls_load_file(CONN_KEY, &keysize, NULL)) == NULL) {
		tls_config_free(out);
		log_fatalx("globalcontext_init: can't load keyfile " CONN_KEY);
	}

	if (tls_config_set_key_mem(globalcfg, key, keysize) < 0) {
		tls_unload_file(key, keysize);
		tls_config_free(out);
		log_fatalx("globalcontext_init: can't set key memory");
	}

	tls_config_verify_client(globalcfg);

	serverctx = tls_server();
	if (serverctx == NULL)
		log_fatalx("globalcontext_init: can't allocate tls serverctx");

	if (tls_configure(serverctx, globalcfg) < 0)
		log_fatalx("globalcontext_init: can't configure server: %s",
			tls_error(serverctx));

	globalcontext.tls_key = key;
	globalcontext.tls_keysize = keysize;

	globalcontext.tls_globalcfg = globalcfg;
	globalcontext.tls_serverctx = serverctx;

	globalcontext.listen_fd = -1;
	globalcontext_initialized = 1;

	bzero(&globalcontext.listen_event, sizeof(struct event));
}

static void
globalcontext_teardown(void)
{
	if (globalcontext.listen_fd > 0)
		log_fatalx("globalcontext_teardown: prematurely tore down listener");

	close(globalcontext.listen_fd);
	globalcontext.listen_fd = -1;

	tls_free(globalcontext.tls_serverctx);
	tls_config_free(globalcontext.tls_globalcfg);
	tls_unload_file(globalcontext.tls_key, globalcontext.tls_keysize);

	bzero(&globalcontext, sizeof(struct globalcontext));	
	globalcontext_initialized = 0;
}

static void
globalcontext_listen(void (*cb)(struct conn *));
{
	struct sockaddr_in	sa;
	int			lfd;

	lfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (lfd < 0) log_fatal("globalcontext_listen: socket");

	bzero(&sa, sizeof(struct sockaddr_in));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(CONN_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(lfd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) < 0)
		log_fatal("globalcontext_listen: bind");

	if (listen(lfd, CONN_LISTENBACKLOG) < 0)
		log_fatal("globalcontext_listen: listen");

	event_set(&globalcontext.listen_event, lfd, EV_READ | EV_PERSIST, cb, NULL);

	if (event_add(&globalcontext.listen_event, NULL) < 0) {
		bzero(&globalcontext.listen_event, sizeof(struct event));
		log_fatal("globalcontext_listen: event_add");
	}

	globalcontext.listen_fd = lfd;
}

static void
globalcontext_accept(int fd, short event, void *arg)
{
	struct sockaddr_in	 *peer;
	struct tls		 *connctx;
	struct conn		 *newconn;

	socklen_t	  	  addrlen;
	int			  newfd;	

	void			(*cb)(struct conn *) = (void (*)(struct conn *))arg;

	/* XXX: will this always work? i.e. could we get a RST immediately
	 * after an incoming SYN that breaks everything and causes an error here
	 */
	newfd = accept4(fd, (struct sockaddr *)peer, &addrlen,
		SOCK_NONBLOCK | SOCK_CLOEXEC);

	if (tls_accept_socket(globalconfig.tls_serverctx, &connctx, fd) < 0)
		log_fatalx("tls_accept_socket: %s", tls_error(globalconfig.tls_serverctx);

	if (newfd < 0) log_fatal("globalcontext_accept: accept");

	newconn = conn_new(newfd, peer, connctx);
	cb(newconn);

	(void)event;
}

static void
globalcontext_stoplistening(void)
{
	if (event_del(&globalcontext.listen_event) < 0)
		log_fatal("globalcontext_stoplistening: event_del");

	globalcontext.listen_fd = -1;
	bzero(&globalcontext.listen_event, sizeof(struct event));
}

static struct conn *
conn_new(int fd, struct sockaddr_in *peer, struct tls *connctx)
{
	struct conn	*out;	

	out = calloc(1, sizeof(struct conn));
	if (out == NULL) log_fatal("conn_new: calloc struct conn");

	out->sockfd = fd;
	out->tls_context = connctx;

	memcpy(&out->peer, peer, sizeof(struct sockaddr_in));

	RB_INSERT(conntree, &allcons, out);
	return out;
}

static int
conn_compare(struct conn *a, struct conn *b)
{
	int	result = 0;

	if (a->sockfd > b->sockfd) result = 1;
	if (a->sockfd < b->sockfd) result = -1;

	return result;
}

static void
conn_doreceive(int fd, short event, void *arg)
{
	struct conn	*c = (struct conn *)arg;
	char		*receivebuf = NULL;

	ssize_t		 receivesize = 0;
	int		 unrecoverable, willteardown;

	for (;;) {
		ssize_t		 thispacketsize;
		char		*newreceivebuf;

		newreceivebuf = reallocarray(receivebuf, receivesize + CONN_MTU, sizeof(char));
		if (newreceivebuf == NULL)
			log_fatal("conn_doreceive: reallocarray");

		receivebuf = newreceivebuf;

		thispacketsize = tls_read(c->tls_context, receivebuf + receivesize, CONN_MTU);
		if (thispacketsize == -1 || thispacketsize == 0) {
			willteardown = 1;
			break;	
		} else if (thispacketsize == TLS_WANT_POLLIN || thispacketsize == TLS_WANT_POLLOUT) 
			break;

		receivesize += thispacketsize;
	}

	if (receivesize > 0) {

		if (c->incoming_message == NULL) {
			uint8_t	opcode;
	
			opcode = *(uint8_t *)receivebuf;
			c->incoming_message = netmsg_new(opcode);
	
			/* invalid argument -> bad opcode */
			if (c->incoming_message == NULL) {
				if (errno == EINVAL) c->cb_receive(c, NULL);
				else log_fatal("conn_doreceive: netmsg_new");
			}
		}
	
		if (netmsg_write(c->incoming_message, receivebuf, receivesize) != receivesize)
			log_fatalx("conn_doreceive: netmsg_write: %s",
				netmsg_error(c->incoming_message));
		
		if (!netmsg_isvalid(c->incoming_message, &unrecoverable)) {

			if (unrecoverable) {
				/* deliver as is, caller checks for validity and
				 * can discover errstr + work with it as desired
				 */
				c->cb_receive(c, c->incoming_message);

				netmsg_teardown(c->incoming_message);
				c->incoming_message = NULL;
			}

		} else {
			netmsg_clearerror(c->incoming_message);
			c->cb_receive(c, c->incoming_message);

			netmsg_teardown(c->incoming_message);
			c->incoming_message = NULL;
		}
	}
	
	free(receivebuf);

	if (willteardown)
		conn_teardown(c);

	(void)event;
}

static void
conn_dosend(int fd, short event, void *arg)
{
	/* TODO: return here after message'queue' is done */	
}


void
conn_listen(void (*cb)(struct conn *))
{
	if (!globalcontext_initialized) globalcontext_init();

	if (globalcontext.listening)
		log_fatalx("conn_listen: tried to listen twice in a row");

	globalcontext_listen(cb);
}

void
conn_teardown(struct conn *c)
{
	RB_REMOVE(conntree, &allcons, c);

	conn_stopreceiving(c);
	conn_canceltimeout(c);

	shutdown(c->sockfd, SHUT_RDWR);
	close(c->sockfd);

	tls_free(c->tls_context);
	free(c);
}

void
conn_teardownall(void)
{
	globalcontext_stoplistening();

	while (!RB_EMPTY(&allcons)) {
		struct conn	*toremove;

		toremove = RB_MIN(conntree, &allcons);
		conn_teardown(toremove);
	}
	
	globalcontext_teardown();
}

void
conn_receive
