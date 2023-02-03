SUBDIR+=	src
SUDO?=		doas

# TODO: move a lot of this to a configuration file
# for now, using #defines in the project for these variables

RCDAEMON=	imaged
USER=		_imaged
GECOS=		"Build Image Creation Daemon"

CA=		mitcca.pem
CERT=		jaytlang.pem
KEY=		jaytlang.key

SPUB=		imaged.pub
SSEC=		imaged.sec

CHROOT=		${DESTDIR}/var/imaged
ARCHIVES=	${CHROOT}/archives
SIGNATURES=	${CHROOT}/signatures
MESSAGES=	${CHROOT}/messages

REHASH=		scripts/rehash.sh

checkroot:
	[ `whoami` = "root" ] || {					\
		echo "install should be run as root";			\
		exit 1;							\
	}

beforeinstall: checkroot
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
	${INSTALL} -o root -g wheel -m 444 ${CA} ${DESTDIR}/etc/ssl/authority;	\
	${INSTALL} -o root -g wheel -m 444 ${CERT} ${DESTDIR}/etc/ssl;		\
	${INSTALL} -o root -g wheel -m 400 ${KEY} ${DESTDIR}/etc/ssl/private;	\
	${INSTALL} -o root -g wheel -m 644 ${SPUB} ${DESTDIR}/etc/signify;	\
	${INSTALL} -o root -g wheel -m 600 ${SSEC} ${DESTDIR}/etc/signify
	sh scripts/rehash.sh /etc/ssl/authority

.PHONY: uninstall reinstall
uninstall: checkroot
	${SUDO} ./scripts/uninstall.sh
reinstall: uninstall install

.include <bsd.subdir.mk>
