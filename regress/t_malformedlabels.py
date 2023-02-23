from archive import *
from conf import *
from connection import *
from message import *
from timeout import *

import os
import subprocess

conn = Connection([server_ca, client_ca], client_cert, client_key)
conn.connect(server_hostname, server_port)

print(f"uploading testfile with short label")
message = Message(MessageOp.WRITE, label=b"testfile", file=b"wee")
message.set_label_length_override(1)
conn.write_bytes(message.to_bytes())

to = Timeout(1)
response = Message.from_conn(conn)
to.cancel()

if response == MESSAGE_INCOMPLETE:
	raise ValueError("received incomplete response")
elif response.opcode() != MessageOp.ERROR:
	raise ValueError(f"received non-error opcode {response.opcode()}")

print(f"error = {response.label()}")

# this should block, since we aren't sending enough bytes to break anything yet
print(f"uploading testfile with long label")
message.set_label_length_override(500)
conn.write_bytes(message.to_bytes())

to = Timeout(3)
response = Message.from_conn(conn)
to.cancel()

if response == MESSAGE_INCOMPLETE:
	raise ValueError("received incomplete response")
elif response.opcode() != MessageOp.HEARTBEAT:
	raise ValueError(f"received non-heartbeat opcode {response.opcode()}")

print("got heartbeat admitting waiting for more data, killing halfway")
