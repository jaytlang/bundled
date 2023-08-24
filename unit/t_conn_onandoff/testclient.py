import sys

sys.path.append("../../regress")

from connection import *

import ssl
import time

CA_LIST = None
CLIENT_CERT = None
CLIENT_KEY = None

def open_and_close():
	newconn = Connection(CA_LIST, CLIENT_CERT, CLIENT_KEY)
	try:
		newconn.connect("fpga3.mit.edu", 443)
		raise AssertionError("remote connection did not close before handshake")
	except: 
		newconn.close()

for _i in range(5):
	open_and_close()
