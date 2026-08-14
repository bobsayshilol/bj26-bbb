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
# Note: canyon.mid no longer works because of the 2 voice limit.
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

# Some programs.
program_piano = 0x00
program_guitar = 0x03
program_beep = 0x0A
program_xylophone = 0x1E
program_drums = 0x27


class MIDIConverter:
	def _reset(self):
		self._midi :BufferedReader = None
		self._bpm = 0
		self._voices = {}


	def _error(self, msg):
		pos = self._midi.tell()
		raise RuntimeError(f"Error at file position 0x{pos:x}: {msg}")


	def _set_loopy_voice(self, trk_chan :int, name :str):
		if name == "000-piano":
			self._voices[trk_chan] = program_piano
		elif name == "030-dist":
			self._voices[trk_chan] = program_guitar
		elif name == "054-synvoice":
			self._voices[trk_chan] = program_xylophone # TODO: better choice
		elif name == "128-Drums":
			self._voices[trk_chan] = program_drums
		else:
			self._error(f"Unknown voice mapping for {name}")


	def _to_loopy_voice(self, trk_chan :int, voice :int):
		# neocomposer seems to have a bug where the first track doesn't get
		# the right voice, so use the instrument name instead.
		if trk_chan in self._voices:
			return self._voices[trk_chan]

		voice_map = {
			1 : program_piano, # piano
			# ??? : program_guitar, # guitar
			# ??? : program_beep, # beep
			90 : program_xylophone, # xylophone
			64 : program_drums, # drums
		}
		if voice not in voice_map:
			self._error(f"Voice {voice} didn't have a mapping set")
		return voice_map[voice]


	def _get_channel(self, evt :int):
		chan = evt & 0xF
		if chan not in [0, 1]:
			self._error(f"Too many channels in track: {chan}")
		return chan


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


	def _parse_event(self, cumdt :int, events :list[Event], trk_chan :int) -> int:
		dt = self._parse_vlen()
		if dt < 0:
			self._error(f"-ve dt: {dt}")

		# Add on any existing dts for events that we might have skipped.
		dt += cumdt

		evt = int.from_bytes(self._midi.read(1))
		event = None

		if self._fmt == 1 and 0x80 <= evt and evt <= 0xEF:
			if (evt & 0xF) != 0:
				# We can't support multi-tracks that also have multiple channels per track.
				self._error(f"Multi track file has multiple channels per track: 0x{evt:x}")
			# Add the current track number as the channel.
			evt = evt | trk_chan

		if 0x00 <= evt and evt <= 0x7F: # ???
			print(f"Unknown: 0x{evt:x}")
			unk = int.from_bytes(self._midi.read(1))

		elif 0x80 <= evt and evt <= 0x8F: # note off
			chan = self._get_channel(evt)
			key = int.from_bytes(self._midi.read(1))
			vel = int.from_bytes(self._midi.read(1))
			event = NoteOffEvent(dt, chan, key, vel)

		elif 0x90 <= evt and evt <= 0x9F: # note on
			chan = self._get_channel(evt)
			key = int.from_bytes(self._midi.read(1))
			vel = int.from_bytes(self._midi.read(1))
			event = NoteOnEvent(dt, chan, key, vel)

		elif 0xA0 <= evt and evt <= 0xAF: # aftertouch
			print(f"Ignored: 0x{evt:x}")
			chan = self._get_channel(evt)
			key = int.from_bytes(self._midi.read(1))
			tch = int.from_bytes(self._midi.read(1))

		elif 0xB0 <= evt and evt <= 0xBF: # controller change
			print(f"Ignored: 0x{evt:x}")
			chan = self._get_channel(evt)
			cnt = int.from_bytes(self._midi.read(1))
			val = int.from_bytes(self._midi.read(1))

		elif 0xC0 <= evt and evt <= 0xCF: # program change
			chan = self._get_channel(evt)
			voice = int.from_bytes(self._midi.read(1))
			loopy_voice = self._to_loopy_voice(trk_chan, voice)
			event = ProgramChangeEvent(dt, chan, loopy_voice)

		elif 0xD0 <= evt and evt <= 0xDF: # pressure change
			print(f"Ignored: 0x{evt:x}")
			chan = self._get_channel(evt)
			prs = int.from_bytes(self._midi.read(1))

		elif 0xE0 <= evt and evt <= 0xEF: # pitch bend
			print(f"Ignored: 0x{evt:x}")
			chan = self._get_channel(evt)
			lsb = int.from_bytes(self._midi.read(1))
			msb = int.from_bytes(self._midi.read(1))

		elif evt == 0xFF: # system reset
			typ = int.from_bytes(self._midi.read(1))
			size = int.from_bytes(self._midi.read(1)) # size
			val = self._midi.read(size)
			if typ == 0x04: # voice mappings
				name = val.decode()
				self._set_loopy_voice(trk_chan, name)
			else:
				event = ResetEvent(dt)

		else:
			self._error(f"Unhandled event: 0x{evt:x}")


		if event:
			events.append(event)
			return 0 # event was consumed
		else:
			return dt # event was skipped, but dt must be preserved


	def _parse_track(self, trk_chan :int):
		# Check that this is a track chunk.
		if self._midi.read(4).decode() != "MTrk":
			self._error("Bad chunk in MIDI file")
		length = int.from_bytes(self._midi.read(4))
		end = self._midi.tell() + length

		# Read the events.
		trk_events :list[Event] = []
		cumdt = 0
		while self._midi.tell() < end:
			cumdt = self._parse_event(cumdt, trk_events, trk_chan)
		if self._midi.tell() != end:
			self._error("Read past the end of a track chunk")

		return trk_events


	def _combine_tracks(self, tracks :list[list[Event]]):
		if self._fmt == 0:
			return tracks[0]

		events :list[Event] = []

		while any([len(t) > 0 for t in tracks]):
			# Look for the next event.
			next_track :list[Event] = None
			next_dt = 100000 # assuming no huge jumps
			for track in tracks:
				if len(track) > 0 and track[0].dt < next_dt:
					next_dt = track[0].dt
					next_track = track

			# Add the event.
			events.append(next_track.pop(0))

			# Reduce the timeout of the rest of the next events.
			for track in tracks:
				if track != next_track and len(track) > 0:
					track[0].dt -= next_dt

		return events


	def _parse_file(self, filename):
		with open(filename, "rb") as midi:
			self._midi = midi
			fmt, num_chunks, bpm = self._read_header()

			# Sanity check the header.
			if fmt not in [0, 1]: # 0 = single track, 1 = multi track
				self._error(f"Unhandled MIDI format: {fmt}")
			if fmt == 0 and num_chunks > 1:
				self._error("Too many track chunks in single track file")
			if fmt == 1 and num_chunks > 2: # we only support 2 because LoopyMSE does
				self._error("Too many track chunks in multi track file")
			self._bpm = bpm
			self._fmt = fmt

			# Read each track and combine them.
			tracks = []
			for trk_chan in range(num_chunks):
				tracks.append(self._parse_track(trk_chan))
			return self._combine_tracks(tracks)


	def _write_file(self, sym :str, filename, events :list[Event]):
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
		last_dt = 0
		for event in events:
			# Emit the current block if it's at a new timestamp, or it's gotten too big.
			max_block_size = 0xF0 # arbitrary, might need a proper cap
			if event.dt != 0 or len(current_block) + max_event_size > max_block_size:
				emit(last_dt, current_block, output)
				last_dt = event.dt

			# Add the new event.
			event.write(current_block)

		# Emit final one.
		emit(last_dt, current_block, output)

		# Dump it to a file.
		with open(filename, "w") as out:
			out.write("#include <stdint.h>\n")
			out.write("#include \"music.h\"\n")
			out.write("namespace game::music {\n")
			out.write(f"constexpr uint8_t {sym}[] = {{\n")
			i = 0
			for b in output:
				out.write(f"0x{b:x},")
				i += 1
				if i == 15:
					out.write("\n")
					i = 0
			out.write(f"\n{sym}_end_evt\n") # C declares the repeat mode
			out.write("};\n")
			out.write("} // namespace game::music")


	# Load MIDI |infile|, spit out .c file at |outfile|, with symbol named |sym|.
	def convert_file(self, sym, infile, outfile):
		escaped_name = "".join(x if x.isalnum() else "_" for x in sym)
		print(f"Converting {infile} to {outfile}, symbol name '{escaped_name}'")
		self._reset()
		events = self._parse_file(infile)
		self._write_file(escaped_name, outfile, events)



if __name__ == "__main__":
	import argparse
	parser = argparse.ArgumentParser()
	parser.add_argument("input")
	parser.add_argument("output")
	parser.add_argument("symbol")

	args = parser.parse_args()

	midi = MIDIConverter()
	midi.convert_file(args.symbol, args.input, args.output)

