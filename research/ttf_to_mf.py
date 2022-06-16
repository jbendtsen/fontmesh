#!/bin/python

import struct
import sys
from svg_to_poly import *

def hexdump(buf, offset, size):
	def to_ascii(r):
		a = ""
		for c in r:
			if c >= 0x20 and c <= 0x7e:
				a += chr(c)
			else:
				a += "."
		return a

	rows = size // 16
	end = offset + size
	for i in range(rows):
		s = "{:x} | " + ("{:02x} " * 16) + "| "
		r = buf[offset:offset+16]
		print(s.format(offset, *r) + to_ascii(r))
		offset += 16

	if offset < end:
		mod = size % 16
		s = "{:x} | " + ("{:02x} " * mod) + ("   " * (16-mod)) + "| "
		r = buf[offset:end]
		print(s.format(offset, *r) + to_ascii(r))

class TTF:
	def init_from(self, buf):
		self.sections = {}
		self.n_glyphs = 0
		self.is_loca_32 = False

		idx = 0xc
		while idx < len(buf):
			if not (buf[idx] >= 0x20 and buf[idx] <= 0x7e and
			buf[idx+1] >= 0x20 and buf[idx+1] <= 0x7e and
			buf[idx+2] >= 0x20 and buf[idx+2] <= 0x7e and
			buf[idx+3] >= 0x20 and buf[idx+3] <= 0x7e):
				break

			name, unk, offset, size = struct.unpack_from(">4sIII", contents, idx)
			self.sections[str(name, "utf8")] = (offset, size)
			idx += 0x10

		if "maxp" in self.sections:
			offset = self.sections["maxp"][0] + 4
			self.n_glyphs = (buf[offset] << 8) | buf[offset+1]

		if "loca" in self.sections:
			loca_size = self.sections["loca"][1]
			self.is_loca_32 = loca_size >= (self.n_glyphs * 4)

def lookup_glyph(ttf, buf, codepoint):
	if not "cmap" in ttf.sections:
		print("Could not find cmap section")
		return

	offset = ttf.sections["cmap"][0]
	n_subtables = (buf[offset+2] << 8) | buf[offset+3]
	offset += 4 + n_subtables*8

	table = []

	seen_fmt_4 = False
	seen_fmt_12 = False

	for i in range(n_subtables):
		fmt = (buf[offset] << 8) | buf[offset+1]
		size = (buf[offset+2] << 8) | buf[offset+3]
		if fmt == 4 and not seen_fmt_4:
			seen_fmt_4 = True
		elif fmt == 12 and not seen_fmt_12:
			seen_fmt_12 = True

		if seen_fmt_4 and seen_fmt_12:
			break

		offset += size

def render_glyph(ttf, buf, index):
	required = ["maxp", "loca", "glyf"]
	missing = []
	for r in required:
		if r not in ttf.sections:
			missing.append(r)

	if len(missing) > 0:
		print("Could not find sections: " + str(missing))
		return

	if index >= ttf.n_glyphs:
		print("index {0} >= glyph count {1}".format(index, ttf.n_glyphs))
		return

	offset = ttf.sections["loca"][0]
	pos = 0
	if ttf.is_loca_32:
		pos = offset + index*4
		pos = (buf[pos] << 24) | (buf[pos+1] << 16) | (buf[pos+2] << 8) | buf[pos+3]
	else:
		pos = offset + index*2
		pos = (buf[pos] << 8) | buf[pos+1]
		pos *= 2

	offset = ttf.sections["glyf"][0]
	offset += pos

	n_contours, xmin, ymin, xmax, ymax = struct.unpack_from(">hhhhh", buf, offset)
	print("contours: {0}, min x: {1}, min y: {2}, max x: {3}, max y: {4}".format(n_contours, xmin, ymin, xmax, ymax))

	if n_contours < 0:
		print("Compound glyphs are currently not supported")
		return
	if n_contours == 0:
		print("Empty glyphs are not supported")
		return

	offset += 10
	contour_ends = list(struct.unpack_from(">{}h".format(n_contours + 1), buf, offset))
	offset += (n_contours+1) * 2
	n_ins = contour_ends.pop()
	n_points = contour_ends[-1] + 1

	offset += n_ins
	flags = []
	idx = 0
	while idx < n_points and len(flags) < n_points:
		b = buf[offset+idx]
		if b & 8:
			flags.extend([b & 0xf7] * (1 + buf[offset+idx+1]))
			idx += 1
		else:
			flags.append(b)
		idx += 1
	offset += idx

	def get_coords(flags, on_curve_list, buf, offset, set_idx):
		point = 0
		coords = []
		idx = 0
		for f in flags:
			is_on_curve = f & 1
			on_curve_list[idx] = not not is_on_curve

			is_byte = f & (1 << (1 + set_idx))
			is_same = f & (1 << (4 + set_idx))

			delta = 0
			if is_byte:
				delta = buf[offset]
				offset += 1
				if not is_same:
					delta = -delta
			else:
				if not is_same:
					delta = (buf[offset] << 8) | buf[offset+1]
					if delta & 0x8000:
						delta -= 0x10000
					offset += 2

			point += delta
			coords.append(point)
			idx += 1

		return coords, offset

	on_curve_list = [None] * n_points
	x_list, offset = get_coords(flags, on_curve_list, buf, offset, 0)
	y_list, offset = get_coords(flags, on_curve_list, buf, offset, 1)

	xstart = x_list[0]
	ystart = y_list[0]
	svg = "M" + str(xstart) + " " + str(ystart)
	xbone = 0
	ybone = 0
	cidx = 0
	was_on_curve = True

	for i in range(1, len(x_list)):
		x = x_list[i]
		y = y_list[i]
		if i > contour_ends[cidx]:
			if was_on_curve:
				svg += "L" + str(xstart) + " " + str(ystart)
			else:
				svg += "Q" + str(xbone) + " " + str(ybone) + " " + str(xstart) + " " + str(ystart)

			svg += "z M" + str(x) + " " + str(y)
			xstart = x
			ystart = y
			was_on_curve = True
			cidx += 1
			continue

		if was_on_curve:
			if on_curve_list[i]:
				svg += "L" + str(x) + " " + str(y)
		else:
			if not on_curve_list[i]:
				x = str((xbone + x_list[i]) / 2)
				y = str((ybone + y_list[i]) / 2)

			svg += "Q" + str(xbone) + " " + str(ybone) + " " + str(x) + " " + str(y)

		if not on_curve_list[i]:
			xbone = x_list[i]
			ybone = y_list[i]

		was_on_curve = on_curve_list[i]

	if was_on_curve:
		svg += "L" + str(xstart) + " " + str(ystart)
	else:
		svg += "Q" + str(xbone) + " " + str(ybone) + " " + str(xstart) + " " + str(ystart)
	svg += "z"

	glyphs = [generate_points(path_to_command_list(svg))]
	plot_all_glyphs(glyphs)

def do_thing(ttf, buf, args):
	if args[0].isdigit():
		index = int(args[0])
		render_glyph(ttf, buf, index)
		return

	if len(args[0]) > 2 and (args[0][0] == "u" or args[0][0] == "U") and args[0][1] == "+":
		n = -1
		try:
			n = int(args[0][2:], 16)
		except:
			pass
		if n >= 0:
			lookup_glyph(ttf, buf, n)
		return
	if len(args[0]) < 4:
		lookup_glyph(ttf, buf, ord(args[0][0]))
		return

	name = args[0]
	if name in ttf.sections:
		offset = ttf.sections[name][0]
		size = ttf.sections[name][1]
		hexdump(buf, offset, size)

with open("font.ttf", "rb") as f:
	contents = f.read()

ttf = TTF()
ttf.init_from(contents)

if len(sys.argv) < 2:
	for s in ttf.sections:
		print(hex(ttf.sections[s][0]), hex(ttf.sections[s][1]), s)
else:
	args = sys.argv[1:]
	do_thing(ttf, contents, args)

