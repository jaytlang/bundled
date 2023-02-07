from connection import *
from message import *
from timeout import *

import ssl
import time

# not working yet!

CA_LIST = ["/etc/ssl/authority/serverchain.pem", "/etc/ssl/authority/mitcca.pem"]
CLIENT_CERT = "/etc/ssl/jaytlang.pem"
CLIENT_KEY = "/etc/ssl/private/jaytlang.key"

active = []

for _i in range(5):
	nc = Connection(CA_LIST, CLIENT_CERT, CLIENT_KEY)
	nc.connect("localhost", 443)	

	testmsg = Message(MessageOp.WRITE, label=b"hello", data=b"world")
	nc.write_bytes(testmsg.to_bytes())

	timeout = Timeout(1)
	message = Message.from_conn(nc, timeout=timeout)

	assert(message.opcode() == MessageOp.WRITE)
	assert(message.label() == "hello")
	assert(message.file() == b"world")

	active.append(nc)

for conn in active: conn.close()
