from archive import *
from conf import *
from connection import *
from message import *
from timeout import *

import os
import subprocess
import time

conn = Connection([server_ca, client_ca], client_cert, client_key)
conn.connect(server_hostname, server_port)

message = Message(MessageOp.WRITE, label=b"testfile", file=b"weeeeeeeee")
conn.write_bytes(message.to_bytes())

response = Message.from_conn(conn)

print("trying to sign then immediately cutting connection")
message = Message(MessageOp.SIGN)
conn.write_bytes(message.to_bytes())

# cat ate the cable
conn.close()

# fix the cable and try again
time.sleep(1)

print(f"checking connection condition")
conn = Connection([server_ca, client_ca], client_cert, client_key)
conn.connect(server_hostname, server_port)

message = Message(MessageOp.WRITE, label=b"testfile", file=b"weeeeeeeee")
conn.write_bytes(message.to_bytes())

to = Timeout(1)
response = Message.from_conn(conn, timeout=to)

if response.opcode() != MessageOp.ACK:
	print(f"received non-ack opcode {response.opcode()}")

print("all ok")
