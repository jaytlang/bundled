PROG=		imaged
NOMAN=		noman
BINDIR?=	/usr/sbin

SRCS=	archive.c	\
	buffer.c	\
	conn.c		\
	crypto.c	\
	engine.c	\
	frontend.c	\
	imaged.c	\
	ipcmsg.c	\
	log.c		\
	msgqueue.c	\
	netmsg.c	\
	proc.c

COPTS+= -Wall -Wextra -Werror -pedantic -I..
LDADD+= -lutil -levent -lz -ltls

DEBUG+=	-g

.ifdef DEBUG
LDFLAGS+= -Wl,-E
.endif

.include <bsd.prog.mk>
