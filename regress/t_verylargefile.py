from archive import *
from conf import *
from connection import *
from message import *
from timeout import *

import os
import subprocess

conn = Connection([server_ca, client_ca], client_cert, client_key)
conn.connect(server_hostname, server_port)

print(f"uploading testfile")
message = Message(MessageOp.WRITE, label=b"testfile", file=b"f" * (max_file_size + 1))
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

archive = Archive.from_bytes(response.file())
filenames = archive.all_filenames()
print(filenames)

