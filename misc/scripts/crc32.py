#!/usr/bin/env python

import sys
import binascii

def c(filename):
	print(filename)
	buf = open(filename,'rb').read()
	buf = (binascii.crc32(buf) & 0xFFFFFFFF)
	print("CRC: %08X" % buf)

c(sys.argv[1])