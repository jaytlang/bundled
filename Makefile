SUBDIR+=	src
SUDO?=		doas

# TODO: move a lot of this to a configuration file
# for now, using #defines in the project for these variables

RCDAEMON=	bundled
USER=		_bundled
GECOS=		"Build Image Creation Daemon"

SPUB=		bundled.pub
SSEC=		bundled.sec

CHROOT=		${DESTDIR}/var/bundled
ARCHIVES=	${CHROOT}/archives
SIGNATURES=	${CHROOT}/signatures
MESSAGES=	${CHROOT}/messages

CONFIG=		bundled.conf

SERVERSEC=	/etc/ssl/private/server.key
SERVERCA=	/etc/ssl/authority/serverchain.pem
SERVERPUB=	/etc/ssl/server.pem

# Build configuration ends here

REHASH=		scripts/rehash.sh
UGOUID=		's/.*\(....\)/\1/'

checkcerts:
	[ -f "${SERVERPUB}" ] || { 					\
		cat << EOF;						\
Makefile configuration has SERVERPUB='${SERVERPUB}', but the file	\
${SERVERPUB} does not exist; install cannot continue.			\
EOF									\
		exit 1;							\
	}
	[ -f "${SERVERCA}" ] || {					\
		cat << EOF;						\
Makefile configuration has SERVERCA='${SERVERCA}', but the		\
file ${SERVERCA} does not exist; install cannot continue.		\
EOF									\
		exit 1;							\
	}
	doas [ -f "${SERVERSEC}" ] || {					\
		cat << EOF;						\
Makefile configuration has SERVERSEC='${SERVERSEC}', but the		\
file ${SERVERSEC} does not exist; install cannot continue.		\
EOF									\
		exit 1;							\
	}
	if [ `stat -f "%p%u" ${SERVERPUB} | sed ${UGOUID}` != "4440" ];	\
	then								\
		echo "${SERVERPUB} has incorrect permissions";		\
		exit 1;							\
	fi
	if [ `stat -f "%p%u" ${SERVERCA} | sed ${UGOUID}` != "4440" ];	\
	then								\
		echo "${SERVERCA} has incorrect permissions";		\
		exit 1;							\
	fi
	if [ `stat -f "%p%u" ${SERVERSEC} | sed ${UGOUID}` != "4000" ];	\
	then								\
		echo "${SERVERSEC} has incorrect permissions";		\
		exit 1;							\
	fi



checkroot:
	[ `whoami` = "root" ] || {					\
		echo "install should be run as root";			\
		exit 1;							\
	}

beforeinstall: checkroot checkcerts
	if [ -d "${CHROOT}" ]; then					\
		echo "install already ran";				\
		exit 1;							\
	fi

afterinstall:
	useradd -c ${GECOS} -k /var/empty -s /sbin/nologin -d ${CHROOT}		\
		-m ${USER} 2>/dev/null
	${INSTALL} -o root -g daemon -m 755 -d ${CHROOT}
	${INSTALL} -o root -g ${USER} -m 775 -d ${ARCHIVES} ${SIGNATURES} 	\
		${MESSAGES}
	cd etc;									\
	${INSTALL} -o root -g wheel -m 555 ${RCDAEMON} 				\
		${DESTDIR}/etc/rc.d;						\
	${INSTALL} -o root -g _bundled -m 644 ${SPUB} ${DESTDIR}/etc/signify;	\
	${INSTALL} -o root -g _bundled -m 640 ${SSEC} ${DESTDIR}/etc/signify;	\
	${INSTALL} -o root -g wheel -m 644 ${CONFIG} ${DESTDIR}/etc
	sh scripts/rehash.sh /etc/ssl/authority

.PHONY: uninstall reinstall
uninstall: checkroot
	${SUDO} ./scripts/uninstall.sh
reinstall: uninstall install

.include <bsd.subdir.mk>
