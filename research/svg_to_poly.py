#!/bin/python

import re
import matplotlib.pyplot as plt

def load_paths_from_svg_file(fname):
	with open(fname) as f:
		buf = f.read()

	glyphs = re.findall(r"<glyph[^>]+", buf)
	slot_regex = re.compile(r"unicode=['\"]([^'\"]+)")
	path_regex = re.compile(r"d=['\"]([^'\"]+)")
	paths = []
	slots = []

	for g in glyphs:
		slot = slot_regex.findall(g)
		path = path_regex.findall(g)
		if slot and path:
			paths.append(path[0])
			slots.append(slot[0])

	return paths, slots

def path_to_command_list(path):
	cmd_list = []
	cmd = None
	n = None
	neg = False
	dot = 0
	for i in range(len(path)):
		c = path[i]
		if c < ' ' or c > '~':
			continue

		if cmd is None:
			cmd = [c]
			continue

		if c == '-':
			neg = True
		elif c == '.':
			dot = 10
		elif c >= '0' and c <= '9':
			if n is None:
				n = 0.0
			d = ord(c) - 0x30
			if dot:
				d /= dot
				dot *= 10
			else:
				n *= 10
			n += d
		else:
			if n is not None:
				n = -n if neg else n
				cmd.append(n)
				n = None
			if c != ' ' and c != ',':
				cmd_list.append(cmd)
				cmd = [c]
			dot = 0
			neg = False

	if cmd is not None:
		cmd_list.append(cmd)
	return cmd_list

def generate_quadratic(points, n_inner_verts, bone_x, bone_y, end_x, end_y):
	start_x = points[-1][0]
	start_y = points[-1][1]
	for i in range(n_inner_verts):
		t = float(i+1) / float(n_inner_verts+1)
		inv_t2 = (1-t)*(1-t)
		x = bone_x + inv_t2*(start_x-bone_x) + t*t*(end_x-bone_x)
		y = bone_y + inv_t2*(start_y-bone_y) + t*t*(end_y-bone_y)
		points.append([x, y])
	points.append([end_x, end_y])

def generate_cubic(points, n_inner_verts, b1_x, b1_y, b2_x, b2_y, end_x, end_y):
	start_x = points[-1][0]
	start_y = points[-1][1]
	for i in range(n_inner_verts):
		t = float(i+1) / float(n_inner_verts+1)
		inv_t = 1-t
		x = inv_t*inv_t*inv_t*start_x + 3*t*inv_t*inv_t*b1_x + 3*t*t*inv_t*b2_x + t*t*t*end_x
		y = inv_t*inv_t*inv_t*start_y + 3*t*inv_t*inv_t*b1_y + 3*t*t*inv_t*b2_y + t*t*t*end_y
		points.append([x, y])
	points.append([end_x, end_y])

def generate_arc(points, n_inner_verts, radius_x, radius_y, rotation, is_large, is_sweep, target_x, target_y):
	points.append([target_x, target_y])

def generate_points(cmds):
	shapes = []
	points = []
	prev_quad_x = 0
	prev_quad_y = 0
	prev_cubic_x = 0
	prev_cubic_y = 0
	for c in cmds:
		prev_x = 0 if not points else points[-1][0]
		prev_y = 0 if not points else points[-1][1]
		if points and (c[0] == 'Z' or c[0] == 'z'):
			shapes.append(points)
			points = []
		elif c[0] == 'M':
			if points:
				shapes.append(points)
				points = []
			points.append([c[1], c[2]])
		elif c[0] == 'm':
			if points:
				shapes.append(points)
				points = []
			points.append([c[1] + prev_x, c[2] + prev_y])
		elif c[0] == 'L':
			points.append([c[1], c[2]])
		elif c[0] == 'l':
			points.append([c[1] + prev_x, c[2] + prev_y])
		elif c[0] == 'H':
			points.append([c[1], prev_y])
		elif c[0] == 'h':
			points.append([c[1] + prev_x, prev_y])
		elif c[0] == 'V':
			points.append([prev_x, c[1]])
		elif c[0] == 'v':
			points.append([prev_x, c[1] + prev_y])
		elif c[0] == 'Q':
			generate_quadratic(points, 10, c[1], c[2], c[3], c[4])
			prev_quad_x = c[1]
			prev_quad_y = c[2]
		elif c[0] == 'q':
			generate_quadratic(points, 10, c[1] + prev_x, c[2] + prev_y, c[3] + prev_x, c[4] + prev_y)
			prev_quad_x = c[1] + prev_x
			prev_quad_y = c[2] + prev_y
		elif c[0] == 'T':
			bone_x = 2*prev_x - prev_quad_x
			bone_y = 2*prev_y - prev_quad_y
			generate_quadratic(points, 10, bone_x, bone_y, c[1], c[2])
			prev_quad_x = bone_x
			prev_quad_y = bone_y
		elif c[0] == 't':
			bone_x = 2*prev_x - prev_quad_x
			bone_y = 2*prev_y - prev_quad_y
			generate_quadratic(points, 10, bone_x, bone_y, c[1] + prev_x, c[2] + prev_y)
			prev_quad_x = bone_x
			prev_quad_y = bone_y
		elif c[0] == 'C':
			generate_cubic(points, 10, c[1], c[2], c[3], c[4], c[5], c[6])
			prev_cubic_x = c[3]
			prev_cubic_y = c[4]
		elif c[0] == 'c':
			generate_cubic(points, 10, c[1] + prev_x, c[2] + prev_y, c[3] + prev_x, c[4] + prev_y, c[5] + prev_x, c[6] + prev_y)
			prev_cubic_x = c[3] + prev_x
			prev_cubic_y = c[4] + prev_y
		elif c[0] == 'S':
			bone_x = 2*prev_x - prev_cubic_x
			bone_y = 2*prev_y - prev_cubic_y
			generate_cubic(points, 10, bone_x, bone_y, c[1], c[2], c[3], c[4])
			prev_cubic_x = bone_x
			prev_cubic_y = bone_y
		elif c[0] == 's':
			bone_x = 2*prev_x - prev_cubic_x
			bone_y = 2*prev_y - prev_cubic_y
			generate_cubic(points, 10, bone_x, bone_y, c[1] + prev_x, c[2] + prev_y, c[3] + prev_x, c[4] + prev_y)
			prev_cubic_x = bone_x
			prev_cubic_y = bone_y
		elif c[0] == 'A':
			generate_arc(points, 10, c[1], c[2], c[3], c[4], c[5], c[6], c[7])
		elif c[0] == 'a':
			generate_arc(points, 10, c[1], c[2], c[3], c[4], c[5], c[6] + prev_x, c[7] + prev_y)

	return shapes

def plot_all_glyphs(glyphs):
	idx = 0
	for g in glyphs:
		for s in g:
			x_offset = (idx  % 8) * 2048
			y_offset = (idx // 8) * 2048
			x_list = [p[0] + x_offset for p in s]
			y_list = [p[1] + y_offset for p in s]
			plt.plot(x_list, y_list)
		idx += 1

	plt.show()

def main():
	paths, slots = load_paths_from_svg_file("font.svg")

	glyphs = []
	for p in paths:
		cmds = path_to_command_list(p)
		glyphs.append(generate_points(cmds))

	plot_all_glyphs(glyphs)
