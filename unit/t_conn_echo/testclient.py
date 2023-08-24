import sys

sys.path.append("../../regress/")

from connection import *
from message import *
from timeout import *

import ssl
import time

CA_LIST = None
CLIENT_CERT = None
CLIENT_KEY = None

active = []

for _i in range(5):
	nc = Connection(CA_LIST, CLIENT_CERT, CLIENT_KEY)
	nc.connect("fpga3.mit.edu", 443)	

	label = b"hello"
	file = b"world" * 50000

	testmsg = Message(MessageOp.WRITE, label=label, file=file)
	nc.write_bytes(testmsg.to_bytes())

	timeout = Timeout(1)
	message = Message.from_conn(nc, timeout=timeout)

	assert(message.opcode() == MessageOp.WRITE)
	assert(message.label() == "hello")
	assert(message.file() == file)

	active.append(nc)

print("all done, closing connections")
for conn in active: conn.close()
