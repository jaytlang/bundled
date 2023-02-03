from archive import *
from conf import *
from connection import *
from message import *
from timeout import *

conn = Connection([server_ca, client_ca], client_cert, client_key)
conn.connect(server_hostname, server_port)

# literally nothing in there
message = Message(MessageOp.SIGN, None, None)
conn.write_bytes(message.to_bytes())

to = Timeout(5)
response = Message.from_conn(conn, timeout=to)

print(f"opcode: {response.opcode()}")
if response.opcode() == MessageOp.ERROR:
	print(f"got error {response.fname()}")
assert response.opcode() == MessageOp.BUNDLE

compressed = response.file()
archive = Archive.from_bytes(compressed)
assert archive.num_files() == 0
