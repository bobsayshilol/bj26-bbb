#!/usr/bin/env python
#
# Converter to convert and extract the palette from a set of tiles.
#
# Order of tiles is x then y, where each tile is 8x8, ie 24x24 would become:
#   | 1 | 2 | 3 |
#   | 4 | 5 | 6 |
#   | 7 | 8 | 9 |
#

from io import TextIOWrapper

from PIL import Image, ImageFile
from pathlib import Path

# Give each image 16 colours.
palette_size = 16

tile_size = 8

pal_transparent = -1


def _make_palette(img :ImageFile):
	pal_all = set()
	for y in range(img.height):
		for x in range(img.width):
			r,g,b,a = img.getpixel((x, y))
			if a != 0:
				# Quantize so we don't have duplicate colours.
				r = r * 32 // 256
				g = g * 32 // 256
				b = b * 32 // 256
				pal_all.add((r,g,b))

	if len(pal_all) > palette_size:
		# TODO: near-ness approach to reduce palette size
		# ^Note: intentionally not a map because of this
		raise RuntimeError(f"Too many colours in tiles: {len(pal_all)}")
	else:
		print(f"  Palette size: {len(pal_all)}")

	pal = [x for x in pal_all]
	while len(pal) < palette_size:
		pal.append((0,0,0))
	return pal


def _get_idx(pal, px):
	r, g, b, a = px
	if a == 0:
		return pal_transparent
	r = r * 32 // 256
	g = g * 32 // 256
	b = b * 32 // 256
	for i,pax in enumerate(pal):
		if pax == (r,g,b):
			return i
	raise RuntimeError("BUG: missing colour in palette")


def _extract(file :Path):
	with Image.open(file) as img:
		if (img.width % tile_size) != 0:
			raise RuntimeError(f"Tiles height must be a multiple of {tile_size}: {img.width}")
		if (img.height % tile_size) != 0:
			raise RuntimeError(f"Tiles height must be a multiple of {tile_size}: {img.height}")

		# Build up a colour palette.
		pal = _make_palette(img)

		# Split the data into tiles.
		tw = img.width // tile_size
		th = img.height // tile_size
		data :list[list[int]] = []
		for tj in range(th):
			for ti in range(tw):
				ty = tj * tile_size
				tx = ti * tile_size
				tile = []
				for y in range(tile_size):
					for x in range(tile_size):
						px = img.getpixel((tx + x, ty + y))
						idx = _get_idx(pal, px)
						tile.append(idx)
				data.append(tile)

	return data, pal


def _write_tile(offset :str, tile :list[int], file :TextIOWrapper):
	for i, b in enumerate(tile):
		if b == pal_transparent:
			file.write("engine::graphics::pal_transparent,")
		else:
			file.write(f"{offset} + 0x{b:x},")
		if (i % 15) == 15:
			file.write("\n")


def _write_pal(pal, file :TextIOWrapper):
	for (r,g,b) in pal:
		file.write(f"RGB555({r}, {g}, {b}),\n")


def convert(filename :Path, out :Path):
	print(f"Converting {filename} to {out}")
	tiles, palette = _extract(filename)
	with open(out, "w") as output:
		output.write("#include \"images.h\"\n")
		output.write("namespace game::images {\n")

		# Tile data.
		output.write(f"alignas(uint16_t) constexpr uint8_t {filename.stem}::data[{len(tiles)} * TileSize] = {{\n")
		for tile in tiles:
			_write_tile("pal_offset", tile, output)
			output.write("\n")
		output.write("};\n")

		# Palette.
		output.write(f"constexpr uint16_t {filename.stem}::palette[{len(palette)}] = {{\n")
		_write_pal(palette, output)
		output.write("};\n")

		output.write("} // namespace game::images\n")


if __name__ == "__main__":
	import argparse
	parser = argparse.ArgumentParser()
	parser.add_argument("input")
	parser.add_argument("output")

	args = parser.parse_args()
	convert(Path(args.input), Path(args.output))

