from conf import *
from connection import *
from message import *
from timeout import *

import os
import subprocess

conn = Connection([server_ca, client_ca], client_cert, client_key)
conn.connect(server_hostname, server_port)

testfiles = {}

testfiles[b"testfile"] = b"hello, this is a test!"
testfiles[b"testfile_copy"] = b"hello, this is a test as well!"
testfiles[b"testdir/testfile"] = b"asdfjkl"

for fname in testfiles:
	print(f"uploading {fname}")

	message = Message(MessageOp.WRITE, label=fname, file=testfiles[fname])
	conn.write_bytes(message.to_bytes())

	to = Timeout(1)
	response = Message.from_conn(conn)
	to.cancel()

	if response == MESSAGE_INCOMPLETE:
		raise ValueError("received incomplete response")
	elif response.opcode() != MessageOp.ACK:
		raise ValueError(f"received non-ack opcode {response.opcode()}")

message = Message(MessageOp.SIGN)
conn.write_bytes(message.to_bytes())

to = Timeout(1)
response = Message.from_conn(conn)
to.cancel()

if response == MESSAGE_INCOMPLETE:
	raise ValueError("received incomplete response")
elif response.opcode() != MessageOp.BUNDLE:
	raise ValueError(f"received non-bundle opcode {response.opcode()}")

f = open("1.bundle", 'wb')
f.write(response.file())
f.close()

subprocess.run(["hexdump", "-C", "1.bundle"])

os.unlink("1.bundle")
