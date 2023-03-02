#!/bin/sh
# because why not
# this is not intended for
# serious parameterizable use

set -x

doas rcctl stop bundled
doas rcctl disable bundled
doas rm -f /etc/rc.d/bundled

yes | doas rmuser _bundled
doas rm -rf /var/bundled
doas rm -f /usr/sbin/bundled

doas rm -f /etc/ssl/authority/mitcca.pem
doas rm -f /etc/ssl/jaytlang.pem
doas rm -f /etc/ssl/private/jaytlang.key
doas rm -f /etc/signify/bundled.*

doas sh scripts/rehash.sh /etc/ssl/authority
