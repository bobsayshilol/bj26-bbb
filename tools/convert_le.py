#!/usr/bin/env python

# MAME expects LE encoded ROMs, so byteswap it.

with open("rom.bin", "rb") as inp:
	with open("rom_le.bin", "wb") as out:
		while True:
			b = inp.read(2)
			if len(b) == 0: break
			b = bytes([b[1],b[0]])
			out.write(b)
