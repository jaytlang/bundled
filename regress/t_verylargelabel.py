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
message = Message(MessageOp.WRITE, label=b"t" * (max_name_size * 2), file=b"weeeeeeeee")
conn.write_bytes(message.to_bytes())

to = Timeout(1)
response = Message.from_conn(conn)
to.cancel()

if response == MESSAGE_INCOMPLETE:
	raise ValueError("received incomplete response")
elif response.opcode() != MessageOp.ERROR:
	raise ValueError(f"received non-error opcode {response.opcode()}")

print(f"error = {response.label()}")

