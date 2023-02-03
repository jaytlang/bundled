PROG=		imaged
NOMAN=		noman
BINDIR?=	/usr/sbin

SRCS=	ipcmsg.c log.c proc.c frontend.c engine.c

COPTS+= -Wall -Wextra -Werror -pedantic -I..
LDADD+= -lutil -levent -lz -ltls

DEBUG+=	-g

.ifdef DEBUG
LDFLAGS+= -Wl,-E
.endif

.include <bsd.prog.mk>
