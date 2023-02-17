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
	response_bytes = conn.read_bytes()
	to.cancel()

	response = Message.from_bytes(response_bytes)
	if message == MESSAGE_INCOMPLETE:
		raise ValueError("received incomplete response")
	elif message.opcode() != MessageOp.ACK:
		raise ValueError(f"received non-ack opcode {message.opcode()}")


message = Message(MessageOp.SIGN)
conn.write_bytes(message.to_bytes())

to = Timeout(1)
response_bytes = conn.read_bytes()
to.cancel()

response = Message.from_bytes(response_bytes)
if message == MESSAGE_INCOMPLETE:
	raise ValueError("received incomplete response")
elif message.opcode() != MessageOp.BUNDLE:
	raise ValueError(f"received non-bundle opcode {message.opcode()}")

with open(message.label(), 'wb') as f:
	f.write(message.file())

subprocess.run(["hexdump", "-C", message.label()])

os.unlink(message.label())
