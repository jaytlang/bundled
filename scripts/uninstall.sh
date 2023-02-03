#!/bin/sh
# because why not
# this is not intended for
# serious parameterizable use

set -x

doas rcctl stop imaged
doas rcctl disable imaged
doas rm -f /etc/rc.d/imaged

yes | doas rmuser _imaged
doas rm -rf /var/imaged
doas rm -f /usr/sbin/imaged

doas rm -f /etc/ssl/authority/mitcca.pem
doas rm -f /etc/ssl/jaytlang.pem
doas rm -f /etc/ssl/private/jaytlang.key
doas rm -f /etc/signify/imaged.*

doas sh scripts/rehash.sh /etc/ssl/authority
