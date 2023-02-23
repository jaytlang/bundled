from archive import *
from conf import *
from connection import *
from message import *
from timeout import *

import os
import subprocess

conn = Connection([server_ca, client_ca], client_cert, client_key)
conn.connect(server_hostname, server_port)

for i in range(max_archive_files + 3):
	message = Message(MessageOp.WRITE, label=bytes(f"{i}", encoding="ascii"), file=b"wow")
	conn.write_bytes(message.to_bytes())

	to = Timeout(1)
	response = Message.from_conn(conn)
	to.cancel()

	if response == MESSAGE_INCOMPLETE:
		raise ValueError("received incomplete response")

	if i >= max_archive_files and response.opcode() != MessageOp.ERROR:
		raise ValueError(f"{i}: received non-error opcode {resopnse.opcode()}")
	elif i < max_archive_files and response.opcode() != MessageOp.ACK:
		raise ValueError(f"{i}: received non-ack opcode {response.opcode()}")

message = Message(MessageOp.SIGN)
conn.write_bytes(message.to_bytes())

to = Timeout(1)
response = Message.from_conn(conn)
to.cancel()

if response == MESSAGE_INCOMPLETE:
	raise ValueError("received incomplete response")
elif response.opcode() != MessageOp.BUNDLE:
	raise ValueError(f"received non-bundle opcode {response.opcode()}")

archive = Archive.from_bytes(response.file())
filenames = archive.all_filenames()

if len(filenames) != max_archive_files:
	raise ValueError(f"archive has {len(filenames)} != {max_archive_files} files in it")
