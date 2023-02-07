SRCS+=	${SRCDIR}/conn.c	\
	${SRCDIR}/msgqueue.c	\
	${SRCDIR}/netmsg.c	\
	${SRCDIR}/buffer.c	\
	${SRCDIR}/log.c		\
	test.c

TESTUSER=	root
COPTS+=		-DMESSAGES=\"/var/imaged/messages\"

.include <bsd.prog.mk>
