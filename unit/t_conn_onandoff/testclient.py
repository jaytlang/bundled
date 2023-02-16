import sys

sys.path.append("../../regress")

from connection import *

import ssl
import time

CA_LIST = ["/etc/ssl/authority/serverchain.pem", "/etc/ssl/authority/mitcca.pem"]
CLIENT_CERT = "/etc/ssl/jaytlang.pem"
CLIENT_KEY = "/etc/ssl/private/jaytlang.key"

def open_and_close():
	newconn = Connection(CA_LIST, CLIENT_CERT, CLIENT_KEY)
	try:
		newconn.connect("localhost", 443)
		raise AssertionError("remote connection did not close before handshake")
	except: 
		newconn.close()

for _i in range(5):
	open_and_close()
