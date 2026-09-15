#!/usr/bin/env python

# MAME expects LE encoded ROMs, so byteswap it.

import sys
from pathlib import Path

rom = Path(sys.argv[1])
rom_le = rom.with_stem(rom.stem + "_mame")

with open(rom, "rb") as inp:
	with open(rom_le, "wb") as out:
		while True:
			b = inp.read(2)
			if len(b) == 0: break
			b = bytes([b[1],b[0]])
			out.write(b)
