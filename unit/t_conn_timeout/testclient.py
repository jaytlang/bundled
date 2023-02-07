from connection import *
from message import *
from timeout import *

import ssl
import time

CA_LIST = ["/etc/ssl/authority/serverchain.pem", "/etc/ssl/authority/mitcca.pem"]
CLIENT_CERT = "/etc/ssl/jaytlang.pem"
CLIENT_KEY = "/etc/ssl/private/jaytlang.key"

nc = Connection(CA_LIST, CLIENT_CERT, CLIENT_KEY)
nc.connect("eecs-digital-53.mit.edu", 443)	

time.sleep(2)
