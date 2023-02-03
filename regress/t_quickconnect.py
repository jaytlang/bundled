from conf import *
from connection import *
from message import *

conn = Connection([server_ca, client_ca], client_cert, client_key)

# make three rapid-fire connections, and make sure
# each of them work okay
for _i in range(3):
	conn.connect(server_hostname, server_port)
	conn.close()

