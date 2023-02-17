from conf import *
from connection import *
from message import *
from timeout import *

conn = Connection([server_ca, client_ca], client_cert, client_key)
conn.connect(server_hostname, server_port)

fname = "t" * 2000 + ".txt"
file = "h" * 3000

message = Message(MessageOp.ERROR, b"disaster hath struck")
conn.write_bytes(message.to_bytes())

to = Timeout(5)
response = bytes()
while True:
	response += conn.read_bytes()
	message = Message.from_bytes(response)

	if message == MESSAGE_INCOMPLETE: continue

	assert message.opcode() == MessageOp.ERROR
	assert message.file() == None
	break
