from conf import *
from connection import *
from message import *
from timeout import *

conn = Connection([server_ca, client_ca], client_cert, client_key)
conn.connect(server_hostname, server_port)

heartbeat_counter = 0
wire_bytes = bytes()

while heartbeat_counter < 5:
	to = Timeout(2)
	wire_bytes += conn.read_bytes()
	to.cancel()

	message = Message.from_bytes(wire_bytes)

	if message == MESSAGE_INCOMPLETE: continue
	elif message.opcode() != MessageOp.HEARTBEAT:
		raise ValueError(f"received non-heartbeat opcode {message.opcode()}")

	heartbeat_counter += 1
	print(f"message {heartbeat_counter}/5...")

	response = Message(MessageOp.HEARTBEAT)
	conn.write_bytes(response.to_bytes())


conn.close()
