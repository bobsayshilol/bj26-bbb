#!/usr/bin/env python
#
# Converter to convert and extract the palette from full screen images.
#

from io import TextIOWrapper

from PIL import Image, ImageFile
from pathlib import Path

# Each image gets 208 colours.
max_palette_size = 208

full_width = 256
full_height = 224

# Shrunk versions get a simpler palette.
simple_scale = 4 # quarter width and height
simple_palette = []
simple_pal_base = [0,16,24,28,31]
for r in simple_pal_base:
	for g in simple_pal_base:
		for b in simple_pal_base:
			simple_palette.append((r,g,b))
assert(len(simple_palette) == 125)
assert(len(simple_palette) < max_palette_size)

def _make_palette(img :ImageFile):
	pal_all = set()
	for y in range(img.height):
		for x in range(img.width):
			r,g,b,a = img.getpixel((x, y))
			if a == 0:
				raise RuntimeError(f"Image has transparency")
			# Quantize so we don't have duplicate colours.
			r = r * 32 // 256
			g = g * 32 // 256
			b = b * 32 // 256
			pal_all.add((r,g,b))

	if len(pal_all) > max_palette_size:
		# TODO: near-ness approach to reduce palette size
		# ^Note: intentionally not a map because of this
		raise RuntimeError(f"Too many colours in images: {len(pal_all)}")
	else:
		print(f"  Palette size: {len(pal_all)}")

	pal = [x for x in pal_all]
	return pal


def _get_idx(pal, px, approx):
	r, g, b, a = px
	r = r * 32 // 256
	g = g * 32 // 256
	b = b * 32 // 256
	if approx:
		idx = -1
		min_d = 1000000
		for i,(rr,gg,bb) in enumerate(pal):
			d = (rr-r)**2 + (gg-g)**2 + (bb-b)**2
			if d < min_d:
				min_d = d
				idx = i
		return idx
	else:
		for i,pax in enumerate(pal):
			if pax == (r,g,b):
				return i
		raise RuntimeError("BUG: missing colour in palette")


def _extract(filename :Path):
	with Image.open(filename) as img:
		if img.width != full_width:
			raise RuntimeError(f"Image height must be {full_width}: {img.width}")
		if img.height != full_height:
			raise RuntimeError(f"Image height must be {full_height}: {img.height}")

		# Build up a colour palette.
		pal = _make_palette(img)

		# Read the data.
		data_full :list[int] = []
		for y in range(img.height):
			for x in range(img.width):
				px = img.getpixel((x, y))
				idx = _get_idx(pal, px, approx=False)
				data_full.append(idx)

		# Make a smaller version too.
		data_simple :list[int] = []
		if True:
			for y in range(img.height // simple_scale):
				for x in range(img.width // simple_scale):
					px = img.getpixel((x * simple_scale, y * simple_scale))
					idx = _get_idx(simple_palette, px, approx=True)
					data_simple.append(idx)
		else:
			img = img.reduce(simple_scale)
			data_simple :list[int] = []
			for y in range(img.height):
				for x in range(img.width):
					px = img.getpixel((x, y))
					idx = _get_idx(simple_palette, px, approx=True)
					data_simple.append(idx)

	return data_full, data_simple, pal


def _write_pal(pal, file :TextIOWrapper):
	for (r,g,b) in pal:
		file.write(f"\tRGB555({r}, {g}, {b}),\n")


def _write_rle(suffix :str, data :list[int], file :TextIOWrapper):
	# RLE compress the data.
	rle = []
	last = None
	count = 0
	def emit():
		if last is not None:
			rle.append(last)
			rle.append(count)
	for val in data:
		if val != last or count == 0xFF:
			emit()
			last = val
			count = 0
		else:
			count += 1
	emit()

	# Write it out.
	file.write(f"alignas(uint16_t) static constexpr uint8_t compressed_{suffix}[] = {{\n")
	for i,r in enumerate(rle):
		file.write(f"{r}, ")
		if (i & 15) == 15:
			file.write("\n")
	file.write("};\n")

	file.write(f"static void decompress_{suffix}(uint8_t pal_offset, uint8_t * output)\n")
	file.write(f"{{ rle_decompress(compressed_{suffix}, engine::utils::size(compressed_{suffix}), pal_offset, output, {len(data)}); }}\n")
	file.write(f"static_assert({len(data)} == {suffix}_width * {suffix}_height);\n")


def convert(filenames :list[Path], out_cc :Path):
	out_h = out_cc.with_suffix(".h")
	symbols :list[str] = []

	with open(out_cc, "w") as output:
		output.write(f"#include \"{out_h.name}\"\n")
		output.write("#include \"memory.h\"\n")
		output.write("namespace gallery::stickers {\n")
		output.write("namespace {\n")

		# Simple palette.
		output.write(f"constexpr uint16_t simple_palette[] = {{\n")
		_write_pal(simple_palette, output)
		output.write("};\n")

		# Decompress functions.
		# TODO: more methods to check for better ones
		output.write("""
static void rle_decompress(
	const uint8_t * compressed, uint32_t count,
	uint8_t pal_offset,
	uint8_t * output, [[maybe_unused]] uint32_t expected
)
{
	[[maybe_unused]] auto * begin = output;

	ASSERT((count & 1) == 0);

	for (uint32_t i = 0; i < count; i += 2) {
		uint8_t value = pal_offset + compressed[i];

		const uint16_t length = compressed[i + 1] + 1;
		for (uint16_t j = 0; j < length; j++) {
			// TODO: could do 16bit stores here
			*output++ = value;
		}
	}

	ASSERT(output == begin + expected);
}
""")

		for filename in filenames:
			print(f"Adding {filename} to {out_cc}")
			data_full, data_preview, palette = _extract(filename)
			symbol = filename.stem[4:] # trim "art_" prefix
			symbols.append(symbol)
			output.write(f"struct {symbol} {{\n")

			# Write out both full and preview data.
			_write_rle("fullscreen", data_full, output)
			_write_rle("preview", data_preview, output)

			# Palette.
			output.write(f"static constexpr uint16_t palette[] = {{\n")
			_write_pal(palette, output)
			output.write("};\n")

			output.write("};\n\n\n")

		output.write("} // namespace\n")

		output.write("void decompress_fullscreen(Image img, uint8_t pal_offset, uint8_t * output) {\n")
		output.write("\tASSERT(pal_offset != 0); // 0 is transparent\n")
		output.write("\tswitch (img) {\n")
		for symbol in symbols:
			output.write(f"\tcase Image::{symbol}:\n")
			output.write(f"\t\t{symbol}::decompress_fullscreen(pal_offset, output);\n")
			output.write("\t\tbreak;\n")
		output.write("\tdefault: ASSERT(!\"Unknown image\"); break;\n")
		output.write("}}\n")

		output.write("void decompress_preview(Image img, uint8_t pal_offset, uint8_t * output) {\n")
		output.write("\tASSERT(pal_offset != 0); // 0 is transparent\n")
		output.write("\tswitch (img) {\n")
		for symbol in symbols:
			output.write(f"\tcase Image::{symbol}:\n")
			output.write(f"\t\t{symbol}::decompress_preview(pal_offset, output);\n")
			output.write("\t\tbreak;\n")
		output.write("\tdefault: ASSERT(!\"Unknown image\"); break;\n")
		output.write("}}\n")

		output.write("uint16_t load_palette(Image img, uint16_t * output) { switch (img) {\n")
		for symbol in symbols:
			output.write(f"\tcase Image::{symbol}: {{\n")
			output.write(f"\t\tconstexpr uint16_t size = engine::utils::size({symbol}::palette);\n")
			output.write(f"\t\tengine::utils::fast_memcpy(output, {symbol}::palette, size * 2);\n")
			output.write("\t\treturn size; }\n")
		output.write("\tdefault: ASSERT(\"Unknown image\"); return 0;\n")
		output.write("}}\n")

		output.write("uint16_t preview_palette(uint16_t * output) {\n")
		output.write("\tconstexpr uint16_t size = engine::utils::size(simple_palette);\n")
		output.write("\tengine::utils::fast_memcpy(output, simple_palette, size * 2);\n")
		output.write("\treturn size;\n")
		output.write("}\n")

		output.write("} // namespace gallery::stickers\n")

	# Write the header.
	with open(out_h, "w") as output:
		output.write("#pragma once\n")
		output.write("#include \"graphics.h\"\n")
		output.write("#include \"utils.h\"\n")
		output.write("namespace gallery::stickers {\n")
		output.write("enum class Image : uint8_t {\n")
		for symbol in symbols:
			output.write(f"\t{symbol},\n")
		output.write("};\n")
		output.write(f"inline constexpr uint8_t num_stickers = {len(symbols)};\n")
		output.write(f"inline constexpr uint8_t max_palette_size = {max_palette_size};\n")
		output.write(f"inline constexpr uint16_t fullscreen_width = {full_width};\n")
		output.write(f"inline constexpr uint16_t fullscreen_height = {full_height};\n")
		output.write(f"inline constexpr uint16_t preview_width = {full_width // simple_scale};\n")
		output.write(f"inline constexpr uint16_t preview_height = {full_height // simple_scale};\n")
		output.write("void decompress_fullscreen(Image img, uint8_t pal_offset, uint8_t * output);\n")
		output.write("void decompress_preview(Image img, uint8_t pal_offset, uint8_t * output);\n")
		output.write("uint16_t load_palette(Image img, uint16_t * output);\n")
		output.write("uint16_t preview_palette(uint16_t * output);\n")
		output.write("} // namespace gallery::stickers\n")


if __name__ == "__main__":
	import argparse
	parser = argparse.ArgumentParser()
	parser.add_argument("files", nargs="*")
	parser.add_argument("--output")

	args = parser.parse_args()
	files = [Path(f) for f in args.files]
	convert(files, Path(args.output))

