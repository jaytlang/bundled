from archive import *
from conf import *
from connection import *
from message import *
from timeout import *

import os
import subprocess

conns = []

for _i in range(3):
	conn = Connection([server_ca, client_ca], client_cert, client_key)
	conn.connect(server_hostname, server_port)
	conns.append(conn)

testfiles = {}

testfiles[b"testfile"] = b"hello, this is a test!"
testfiles[b"testfile_copy"] = b"hello, this is a test as well!"
testfiles[b"testdir/testfile"] = b"asdfjkl"

for fname in testfiles:
	i = 1

	for conn in conns:
		realfname = bytes(f"{fname.decode('ascii')}_{i}", encoding="ascii")
		print(f"uploading {realfname}")

		message = Message(MessageOp.WRITE, label=realfname, file=testfiles[fname])
		conn.write_bytes(message.to_bytes())

		i += 1

	for conn in conns:
		to = Timeout(1)
		response = Message.from_conn(conn)
		to.cancel()
	
		if response == MESSAGE_INCOMPLETE:
			raise ValueError("received incomplete response")
		elif response.opcode() != MessageOp.ACK:
			raise ValueError(f"received non-ack opcode {response.opcode()}")


message = Message(MessageOp.SIGN)
for conn in conns:
	conn.write_bytes(message.to_bytes())

i = 1

for conn in conns:
	to = Timeout(1)
	response = Message.from_conn(conn)
	to.cancel()

	if response == MESSAGE_INCOMPLETE:
		raise ValueError("received incomplete response")
	elif response.opcode() != MessageOp.BUNDLE:
		raise ValueError(f"received non-bundle opcode {response.opcode()}")

	archive = Archive.from_bytes(response.file())
	filenames = archive.all_filenames()

	if len(filenames) != 3:
		raise ValueError(f"archive has {len(filenames)} != 3 files in it")

	for testfile in testfiles:
		filenamestr = f"{testfile.decode('ascii')}_{i}"
		file = archive.get_file_by_name(filenamestr)

		print(f"reviewing {filenamestr}")

		if file is None:
			raise ValueError(f"test file {filenamestr} didn't make it into archive")

		expected_content = testfiles[testfile]
		real_content = file.uncompressed_content()

		if expected_content == real_content: continue
		raise ValueError(f"{filenamstr}: wanted {expected_content}, got {real_content}")

	i += 1
