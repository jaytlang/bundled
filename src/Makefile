PROG=		bundled
NOMAN=		noman
BINDIR?=	/usr/sbin

SRCS=	archive.c	\
	buffer.c	\
	conn.c		\
	crypto.c	\
	engine.c	\
	frontend.c	\
	bundled.c	\
	ipcmsg.c	\
	log.c		\
	msgqueue.c	\
	netmsg.c	\
	parse.y		\
	proc.c		\
	scan.l

COPTS+= -Wall -Wextra -Werror -pedantic -I..
COPTS+= -Wno-unused-function -Wno-unneeded-internal-declaration 
LDADD+= -lutil -levent -lz -ltls

DEBUG+=	-g

.ifdef DEBUG
LDFLAGS+= -Wl,-E
.endif

.include <bsd.prog.mk>
