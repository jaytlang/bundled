from conf import *
from connection import *
from message import *
from timeout import *

import time

conns = []
for i in range(3):
	nc = Connection([server_ca, client_ca], client_cert, client_key)
	nc.connect(server_hostname, server_port)

	print(f"connection {i + 1}/3")
	conns.append(nc)
	time.sleep(0.5)
	
heartbeat_counter = 0
wire_bytes = bytes()

while heartbeat_counter < 15:
	for conn in conns:
		to = Timeout(3)
		wire_bytes += conn.read_bytes()
		to.cancel()

		message = Message.from_bytes(wire_bytes)

		if message == MESSAGE_INCOMPLETE: continue
		elif message.opcode() != MessageOp.HEARTBEAT:
			raise ValueError(f"received non-heartbeat opcode {message.opcode()}")

		heartbeat_counter += 1
		print(f"message {heartbeat_counter}/15...")

		response = Message(MessageOp.HEARTBEAT)
		conn.write_bytes(response.to_bytes())

for conn in conns: conn.close()
