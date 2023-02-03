from conf import *
from connection import *
from message import *

conn = Connection([server_ca], client_cert, client_key)
try:
	conn.connect(server_hostname, server_port)
	raise ValueError("somehow got a connection")
	exit(1)
except: pass
