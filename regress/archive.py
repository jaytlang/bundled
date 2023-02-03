import zlib

# archives (so far) are:
# -> uncompressed archive length out front (uint16_t)
# -> zlib compressed:
# 	-> numfiles (uint16_t)
#	-> for each file:
#		-> namelen (uint16_t)
#		-> name (char[])
#		-> filelen (uint16_t)
#		-> file

class File:
	def __init__(self, name, content):
		self._name = name
		self._body = body

	def name(self): return self._name
	def body(self): return self._body
	def size(self): return len(self._body)

	def serialized_size(self):
		return len(self._name) + len(self._body) + 4

	@classmethod
	def from_bytes(cls, bytes):
		if len(bytes) < 2:
			raise IndexError("not enough bytes for file length")

		namelength = int.from_bytes(bytes[0:2], "big")
		bodyoffset = 2 + namelength 
		if len(bytes) < bodyoffset:
			raise IndexError("not enough bytes for file name")

		bodylength = int.from_bytes(bytes[bodyoffset:bodyoffset + 2], "big")
		if len(bytes) < namelength + bodylength + 4:
			raise IndexError("not enough bytes for file body")

		name = bytes[2:bodyoffset].decode(encoding="ascii")
		body = bytes[bodyoffset+2:]
		
		return cls(name, body)

class Archive:
	def __init__(self, filelist):
		self._files = {}
		for file in filelist:
			self._files[file.name] = file

	def get_file_by_name(self, name):
		try: return self._files[name]
		except KeyError: raise KeyError(f"no such file {name} in archive")

	def num_files(self):
		return len(self._files)

	@classmethod
	def from_bytes(cls, bytes):
		if len(bytes) < 2:
			raise IndexError("not enough bytes for uncompressed length")

		length = int.from_bytes(bytes[0:2], "big")
		compressed = bytes[2:]
		decompressed = zlib.decompress(compressed, bufsize=length)

		return cls.from_decompressed_bytes(decompressed)

	@classmethod
	def from_decompressed_bytes(cls, bytes):
		if len(bytes) < 2:
			raise IndexError("not enough bytes for # of files")

		files = []
		numfiles = int.from_bytes(bytes[0:2], "big")	
		bytes = bytes[2:]

		for _i in range(numfiles):
			f = File.from_bytes(bytes)
			files.append(f)
			bytes = bytes[f.serialized_size():]
			
		if len(bytes) > 0:
			raise IndexError("garbage trailing data on end of archive")

		return cls(files)
