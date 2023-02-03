from connection import *
from timeout import *

from enum import IntEnum

# these are in frontend.c
class MessageOp(IntEnum):
	SIGN = 1
	WRITE = 2
	BUNDLE = 3
	HEARTBEAT = 4
	ACK = 5
	ERROR = 6

# f it
MESSAGE_INCOMPLETE = -69

class MessageField:
	def __init__(self, content):
		self._bytes = content

	@classmethod
	def from_bytes(cls, bytes):
		if len(bytes) < 2:
			raise IndexError("not enough bytes for message field length")

		length = int.from_bytes(bytes[0:2], "big")
		if len(bytes) < 2 + length:
			raise IndexError(f"not enough bytes for message field body")

		return cls(bytes[2:2 + length])

	def content(self): return self._bytes

	def to_ascii(self):
		return self._bytes.decode(encoding="ascii")

	def length(self):
		return len(self._bytes) + 2

	def to_bytes(self): 
		length_bytes = len(self._bytes).to_bytes(2, "big")
		return length_bytes + self._bytes

class Message:
	def _check_fname(self):
		if self._fname is None:
			e = f"message op {self._opcode} expected fname field, got none"
			raise ValueError(e)

	def _check_file(self):
		if self._file is None:
			e = f"message op {self._opcode} expected file field, got none"
			raise ValueError(e)

	def __init__(self, opcode, fname=None, file=None):
		self._opcode = opcode
		self._fname = None
		self._file = None

		if fname is not None: self._fname = MessageField(fname)
		if file is not None: self._file = MessageField(file)

	@classmethod
	def from_conn(cls, conn, timeout=None):
		response = bytes()
		while True:
			response += conn.read_bytes()
			message = cls.from_bytes(response)

			if message == MESSAGE_INCOMPLETE: continue
			else:
				if timeout is not None: timeout.cancel()
				return message

	@classmethod
	def from_bytes(cls, bytes):
		full_length = len(bytes)
		if full_length < 1: return MESSAGE_INCOMPLETE

		try: opcode = MessageOp(bytes[0])
		except ValueError:
			raise ValueError(f"invalid opcode {opcode} received")

		print(f"from_bytes opcode = {opcode}")

		if opcode in [MessageOp.SIGN, MessageOp.HEARTBEAT, MessageOp.ACK]:
			return cls(opcode)

		try: fname = MessageField.from_bytes(bytes[1:])
		except IndexError: return MESSAGE_INCOMPLETE

		if opcode == MessageOp.ERROR:
			return cls(opcode, fname=fname.to_ascii())

		try:
			file = MessageField.from_bytes(bytes[1 + fname.length():])
			return cls(opcode, fname=fname.to_ascii(), file=file)
		except IndexError: return MESSAGE_INCOMPLETE

	def to_bytes(self):
		msg = int(self._opcode).to_bytes(1, "big")
		if self._fname is not None: msg += self._fname.to_bytes()
		if self._file is not None: msg += self._file.to_bytes()
		return msg

	def opcode(self): return self._opcode

	def fname(self):
		if self._fname is None: return None
		else: return self._fname.to_ascii()

	def file(self):
		if self._file is None: return None
		else: return self._file.content()
