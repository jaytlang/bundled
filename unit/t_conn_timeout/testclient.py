import sys

sys.path.append("../../regress")

from connection import *
from message import *
from timeout import *

import ssl
import time

CA_LIST = None
CLIENT_CERT = None
CLIENT_KEY = None

nc = Connection(CA_LIST, CLIENT_CERT, CLIENT_KEY)
nc.connect("bundle.jtlang.dev", 443)	

time.sleep(2)
