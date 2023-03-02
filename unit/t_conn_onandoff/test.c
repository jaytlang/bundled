#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <event.h>
#include <stdlib.h>
#include <unistd.h>

#include "bundled.h"

#define	PYTHON3	"/usr/local/bin/python3"	

static void	conn_accept(struct conn *);
static void	fork_client(void);

static int	conn_counter = 0;
int		debug = 1, verbose = 1;

static void
conn_accept(struct conn *c)
{
	warnx("connection accepted!!");
	conn_teardown(c);

	if (++conn_counter == 5) {
		conn_teardownall();
		exit(0);
	}
}

static void
fork_client(void)
{
	pid_t 	pid;

	pid = fork();

	if (pid < 0) log_fatal("fork");
	else if (pid == 0) {
		execl(PYTHON3, PYTHON3, "testclient.py", NULL);
		err(1, "exec");
	}
}

int
main()
{
	config_parse(NULL);
	event_init();
	conn_listen(conn_accept);	

	fork_client();
	event_dispatch();
}
