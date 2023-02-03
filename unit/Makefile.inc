PROG=	test
NOMAN=	noman
SUDO=	doas

TESTUSER?=	_imaged
SRCDIR?=	../../src

COPTS+= -O0 -Wall -Wextra -Werror -pedantic -I${SRCDIR}
LDADD+= -lutil -levent -lz -ltls

DEBUG+= -g
LDFLAGS += -Wl,-E

${PROG}: ${LIBCRT0} ${OBJS} ${LIBC} ${CRTBEGIN} ${CRTEND} ${DPADD}
	${CC} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} ${OBJS:S/..\/..\/src\///g} ${LDADD}

.PHONY: run clean
run:
	${SUDO} -u ${TESTUSER} ./${PROG}
clean:
	rm -f a.out [Ee]rrs mklog *.core y.tab.h a.out *.[od] ${PROG}
