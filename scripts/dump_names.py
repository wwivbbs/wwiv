import struct

with open('names2.lst', 'rb') as f:
	while True:
		b = f.read(31 + 2)
		if not b:
			break
		s = struct.unpack('<31sh', b)
		name = s[0].decode('cp437').split('\0')[0]
		print(str(s[1]) + ':' + name);