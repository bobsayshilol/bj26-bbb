#!/usr/bin/env python
#
# Converter for MIDI files to the MIDI format used by the Loopy.
#
# MIDI format docs:
#   https://www.ccarh.org/courses/253/handout/smf/
#   https://ccrma.stanford.edu/~craig/articles/linuxmidi/misc/essenmidi.html
#
# Loopy format is same as MIDI, but with a reduced header and chunk prefix,
# ie [dt, size, [MIDI:size]].
#
# Only tested with: https://jakeonaut.itch.io/neocomposer and canyon.mid
#
# TODO: check https://yurisizov.itch.io/boscaceoil-blue
#

from io import BufferedReader
from dataclasses import dataclass


# Types of events.
@dataclass
class Event:
	dt :int
@dataclass
class NoteOffEvent(Event):
	channel :int
	key :int
	velocity :int
	def write(self, a: bytearray): a += bytearray([0x90 | self.channel, self.key, 0])
@dataclass
class NoteOnEvent(Event):
	channel :int
	key :int
	velocity :int
	def write(self, a: bytearray): a += bytearray([0x90 | self.channel, self.key, self.velocity])
@dataclass
class ProgramChangeEvent(Event):
	channel :int
	voice :int
	def write(self, a: bytearray): a += bytearray([0xC0 | self.channel, self.voice])
@dataclass
class ResetEvent(Event):
	def write(self, a: bytearray): pass

# Max event size when written out.
max_event_size = 3


class MIDIConverter:
	def _reset(self):
		self._midi :BufferedReader = None
		self._events :list[Event] = []
		self._bpm = 0


	def _error(self, msg):
		pos = self._midi.tell()
		raise RuntimeError(f"Error at file position 0x{pos:x}: {msg}")


	def _read_header(self):
		# Check that this is a MIDI file.
		if self._midi.read(4).decode() != "MThd":
			self._error("Not a MIDI file")
		hdr_len = int.from_bytes(self._midi.read(4))
		if hdr_len != 6:
			self._error(f"Unknown header length (expected 6, got {hdr_len})")

		# Read off the header fields.
		fmt = int.from_bytes(self._midi.read(2))
		num_chunks = int.from_bytes(self._midi.read(2))
		bpm = int.from_bytes(self._midi.read(2))
		return fmt, num_chunks, bpm


	def _parse_vlen(self):
		v = 0
		while True:
			b = int.from_bytes(self._midi.read(1))
			if b <= 127:
				return v + b
			v += b & 127
			v *= 127


	def _parse_event(self, cumdt):
		dt = self._parse_vlen()
		if dt < 0:
			self._error(f"-ve dt: {dt}")

		# Add on any existing dts for events that we might have skipped.
		dt += cumdt

		evt = int.from_bytes(self._midi.read(1))
		event = None

		if 0x00 <= evt and evt <= 0x7F: # ???
			print(f"Unknown: 0x{evt:x}")
			unk = int.from_bytes(self._midi.read(1))

		elif 0x80 <= evt and evt <= 0x8F: # note off
			chan = evt & 0xF
			key = int.from_bytes(self._midi.read(1))
			vel = int.from_bytes(self._midi.read(1))
			event = NoteOffEvent(dt, chan, key, vel)

		elif 0x90 <= evt and evt <= 0x9F: # note on
			chan = evt & 0xF
			key = int.from_bytes(self._midi.read(1))
			vel = int.from_bytes(self._midi.read(1))
			event = NoteOnEvent(dt, chan, key, vel)

		elif 0xA0 <= evt and evt <= 0xAF: # aftertouch
			print(f"Ignored: 0x{evt:x}")
			chan = evt & 0xF
			key = int.from_bytes(self._midi.read(1))
			tch = int.from_bytes(self._midi.read(1))

		elif 0xB0 <= evt and evt <= 0xBF: # controller change
			print(f"Ignored: 0x{evt:x}")
			chan = evt & 0xF
			cnt = int.from_bytes(self._midi.read(1))
			val = int.from_bytes(self._midi.read(1))

		elif 0xC0 <= evt and evt <= 0xCF: # program change
			chan = evt & 0xF
			voice = int.from_bytes(self._midi.read(1))
			event = ProgramChangeEvent(dt, chan, voice)

		elif 0xE0 <= evt and evt <= 0xEF: # pitch bend
			print(f"Ignored: 0x{evt:x}")
			chan = evt & 0xF
			lsb = int.from_bytes(self._midi.read(1))
			msb = int.from_bytes(self._midi.read(1))

		elif evt == 0xFF: # system reset
			self._midi.read(1) # ignore
			size = int.from_bytes(self._midi.read(1)) # size
			self._midi.read(size) # ignore
			event = ResetEvent(dt)

		else:
			self._error(f"Unhandled event: 0x{evt:x}")


		if event:
			self._events.append(event)
			return 0 # event was consumed
		else:
			return dt # event was skipped, but dt must be preserved


	def _parse_chunk(self):
		# Check that this is a chunk.
		if self._midi.read(4).decode() != "MTrk":
			self._error("Bad chunk in MIDI file")
		length = int.from_bytes(self._midi.read(4))
		end = self._midi.tell() + length

		# Read the events.
		cumdt = 0
		while self._midi.tell() < end:
			cumdt = self._parse_event(cumdt)
		if self._midi.tell() != end:
			self._error("Read past the end of the chunk")


	def _parse_file(self, filename):
		with open(filename, "rb") as midi:
			self._midi = midi
			fmt, num_chunks, bpm = self._read_header()
			self._bpm = bpm
			if fmt != 0: # 0 = single track
				self._error(f"Unhandled MIDI format: {fmt}")
			for _ in range(num_chunks):
				self._parse_chunk()


	def _write_file(self, sym, filename):
		output = bytearray()

		# Write out the header.
		output += (3333 // self._bpm).to_bytes(length=1) # magic number, might need tweaking
		output += bytearray([0xA2, 0x08, 0xA0]) # ???

		# TODO: merge the off-on notes together to shrink the file.

		# Write out into blocks as we go.
		def emit(dt, blk, out):
			blk_len = len(blk)
			if blk_len > 0:
				if dt > 255:
					print(f"Warning: event delta too big: {dt}. Capping to 255")
					dt = 255
				out += bytearray([dt, blk_len])
				out += blk
				blk.resize(0)

		current_block = bytearray()
		for event in self._events:
			# Emit the current block if it's at a new timestamp, or it's gotten too big.
			max_block_size = 0xF0 # arbitrary, might need a proper cap
			if event.dt != 0 or len(current_block) + max_event_size > max_block_size:
				emit(event.dt, current_block, output)

			# Add the new event.
			event.write(current_block)

		# Emit final one.
		emit(0, current_block, output)

		# Write footer.
		output += bytearray([0x00, 0xFE, 0xFF, 0xFF])

		# Dump it to a file.
		escaped_name = "".join(x if x.isalnum() else "_" for x in sym)
		with open(filename, "w") as out:
			out.write("#include <stdint.h>\n")
			out.write(f"const uint8_t {escaped_name}[] = {{\n")
			i = 0
			for b in output:
				out.write(f"0x{b:x},")
				i += 1
				if i == 15:
					out.write("\n")
					i = 0
			out.write("\n};")


	# Load MIDI |infile|, spit out .c file at |outfile|, with symbol named |sym|.
	def convert_file(self, sym, infile, outfile):
		self._reset()
		self._parse_file(infile)
		self._write_file(sym, outfile)



if __name__ == "__main__":
	import argparse
	parser = argparse.ArgumentParser()
	parser.add_argument("input")
	parser.add_argument("output")
	parser.add_argument("symbol")

	args = parser.parse_args()

	midi = MIDIConverter()
	midi.convert_file(args.symbol, args.input, args.output)

