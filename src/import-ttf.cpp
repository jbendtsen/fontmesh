#include "meshfont.h"
#include "util.h"

#define GET_BE32(buf, off) \
	(((u32)buf[off] << 24) | ((u32)buf[off+1] << 16) | ((u32)buf[off+2] << 8) | (u32)buf[off+3])

#define FOUR_EQUALS(buf, off, str) \
	(buf[off] == str[0] && buf[off+1] == str[1] && buf[off+2] == str[2] && buf[off+3] == str[3])

struct TTF {
	int n_glyghs;
	int is_loca_32;
	u32 maxp_offset;
	u32 maxp_size;
	u32 cmap_offset;
	u32 cmap_size;
	u32 loca_offset;
	u32 loca_size;
	u32 glyf_offset;
	u32 glyf_size;
};

TTF parse_ttf_header(u8 *buf, int size)
{
	TTF ttf = {0};

	int idx = 0xc;
	while (idx < len(buf)) {
		if (!(buf[idx] >= 0x20 && buf[idx] <= 0x7e &&
			buf[idx+1] >= 0x20 && buf[idx+1] <= 0x7e &&
			buf[idx+2] >= 0x20 && buf[idx+2] <= 0x7e &&
			buf[idx+3] >= 0x20 && buf[idx+3] <= 0x7e)
		) {
			break;
		}

		if FOUR_EQUALS(buf, idx, "maxp") {
			ttf.maxp_offset = GET_BE32(buf, idx+8);
			ttf.maxp_size   = GET_BE32(buf, idx+12);
		}
		else if FOUR_EQUALS(buf, idx, "cmap") {
			ttf.cmap_offset = GET_BE32(buf, idx+8);
			ttf.cmap_size   = GET_BE32(buf, idx+12);
		}
		else if FOUR_EQUALS(buf, idx, "loca") {
			ttf.loca_offset = GET_BE32(buf, idx+8);
			ttf.loca_size   = GET_BE32(buf, idx+12);
		}
		else if FOUR_EQUALS(buf, idx, "glyf") {
			ttf.glyf_offset = GET_BE32(buf, idx+8);
			ttf.glyf_size   = GET_BE32(buf, idx+12);
		}

		idx += 0x10;
	}

	ttf.n_glyphs = (buf[ttf.maxp_offset + 4] << 8) | buf[ttf.maxp_offset + 5];
	ttf.is_loca_32 = ttf.loca_size >= (self.n_glyphs * 4);

	return ttf;
}

u32 render_ttf_glyph(TTF& ttf, Buffer& mfc, u32 index, Bi_Vector& coords, Buffer& flags)
{
	u32 offset = ttf.loca_offset;
	if (ttf.is_loca_32) {
		offset += index*4;
		offset = GET_BE32(buf, pos);
	}
	else {
		offset += index*2;
		offset = (buf[offset] << 8) | buf[offset+1];
		offset *= 2;
	}

	offset += ttf.glyf_offset;
	int n_contours = (buf[offset] << 8) | buf[offset+1];

	u8 *contour_ends = &buf[offset + 10];
	int n_points = (contour_ends[0] << 8) | contour_ends[1];
	int n_ins = (contour_ends[2] << 8) | contour_ends[3];

	offset += 12 + (n_contours*2) + n_ins;
	flags.size = 0;

	int j;
	for (j = 0; j < n_points && flags.size < n_points; j++) {
		u8 b = buf[offset+j];
		if (b & 8) {
			j++;
			int len = 1 + buf[offset+j];
			u8 *repeated = flags.add(nullptr, len);
			memset(repeated, b & 0xf7, len);
		}
		else {
			flags.add(&b, 1);
		}
	}
	offset += j;

	coords.resize(n_points);
	coords.size = 0;

	u32 x_off = offset;
	u32 y_off = offset;
	int x = 0;
	int y = 0;
	short dx = 0;
	short dy = 0;
	int min_x = 0;
	int min_y = 0;
	int max_x = 0;
	int max_y = 0;

	for (j = 0; j < n_points; j++) {
		u8 f = flags.data()[j];

		dx = 0;
		if (f & 2) {
			dx = buf[x_off];
			x_off += 1;
			if ((f & 0x10) == 0) dx = -dx;
		}
		else if ((f & 0x10) == 0) {
			dx = (buf[x_off] << 8) | buf[x_off+1];
			x_off += 2;
		}

		dy = 0;
		if (f & 4) {
			dy = buf[y_off];
			y_off += 1;
			if ((f & 0x20) == 0) dy = -dy;
		}
		else if ((f & 0x20) == 0) {
			dy = (buf[y_off] << 8) | buf[y_off+1];
			y_off += 2;
		}

		x += dx;
		y += dy;
		if (j == 0 || x < min_x) min_x = x;
		if (j == 0 || y < min_y) min_y = y;
		if (j == 0 || x > max_x) max_x = x;
		if (j == 0 || y > max_y) max_y = y;

		bool on_curve = (f & 1) != 0;
		coords.add(x * 2 + on_curve, y);
	}

	int baseline = max_y;
	int add_x = min_x < 0 ? -min_x : 0;
	int add_y = min_y < 0 ? -min_y : 0;
	max_x += add_x;
	max_y += add_y;

	int start_x;
	int start_y;
	coords.get((u32*)&start_x, (u32*)&start_y);
	start_x = (start_x / 2) + add_x;
	start_y = max_y - (start_y + add_y);

	int prev_x = 0;
	int prev_y = 0;
	int joint_x = 0;
	int joint_y = 0;
	int cidx = 0;
	bool was_on_curve = true;

	short segment[6];
	u32 seg_size = 0;
	u32 out_size = 0;

	for (j = 1; j < n_points; j++) {
		int x;
		int y;
		coords.get((u32*)&x, (u32*)&y);
		bool on_curve = x & 1;
		x = (x / 2) + add_x;
		y = max_y - (y + add_y);

		seg_size = 0;

		if (j > ((contour_ends[cidx] << 8) | contour_ends[cidx])) {
			if (start_x == prev_x && start_y == prev_y) {
				segment[0] = 0;
				seg_size = 2;
			}
			else if (was_on_curve) {
				segment[0] = 1;
				segment[1] = start_x;
				segment[2] = start_y;
				segment[3] = 0;
				seg_size = 8;
			}
			else {
				segment[0] = 2;
				segment[1] = start_x;
				segment[2] = start_y;
				segment[3] = joint_x;
				segment[4] = joint_y;
				segment[5] = 0;
				seg_size = 12;
			}

			xstart = x;
			ystart = y;
			was_on_curve = true;
			cidx += 1;
		}
		else {
			if (was_on_curve) {
				if (on_curve) {
					segment[0] = 1;
					segment[1] = x;
					segment[2] = y;
					seg_size = 6;
				}
			}
			else {
				segment[0] = 2;
				segment[1] = on_curve ? x : (joint_x + x) / 2;
				segment[2] = on_curve ? y : (joint_y + y) / 2;
				segment[3] = joint_x;
				segment[4] = joint_y;
				seg_size = 10;
			}

			if (!on_curve) {
				joint_x = x;
				joint_y = y;
			}
		}

		if (seg_size > 0) {
			mfc.add((u8*)segment, seg_size);
			out_size += seg_size;
		}
		if (on_curve) {
			prev_x = x;
			prev_y = y;
		}

		was_on_curve = on_curve;
	}

	if (start_x == prev_x && start_y == prev_y) {
		segment[0] = 0;
		seg_size = 2;
	}
	else if (was_on_curve) {
		segment[0] = 1;
		segment[1] = start_x;
		segment[2] = start_y;
		segment[3] = 0;
		seg_size = 8;
	}
	else {
		segment[0] = 2;
		segment[1] = start_x;
		segment[2] = start_y;
		segment[3] = joint_x;
		segment[4] = joint_y;
		segment[5] = 0;
		seg_size = 12;
	}

	mfc.add((u8*)segment, seg_size);
	out_size += seg_size;

	return out_size;
}

Buffer ttf_to_mfc(TTF& ttf, u8 *buf, int buf_size)
{
	Buffer mfc = {0};
	if (!ttf.cmap_offset || !ttf.loca_offset || !ttf.glyf_offset)
		return mfc;

	u32 offset = cmap_offset;
	int n_subtables = (buf[offset+2] << 8) | buf[offset+3];
	offset += 4 + n_subtables*8;

	Bi_Vector table;
	table.reserve(0x1000);

	while (offset < cmap_size) {
		int fmt  = ((int)buf[offset]   << 8) | (int)buf[offset+1];
		int size = ((int)buf[offset+2] << 8) | (int)buf[offset+3];

		if (fmt == 4) {
			int segs_x2 = ((int)buf[offset+6] << 8) | (int)buf[offset+7];

			u8 *end_code   = &buf[offset+14];
			u8 *start_code = &end_code[segs_x2+2];
			u8 *deltas     = &start_code[segs_x2];
			u8 *range_offs = &deltas[segs_x2];
			//u8 *id_array   = &range_offs[segs_x2];

			for (int i = 0; i < segs_x2; i += 2) {
				int ro    = (range_offs[i] << 8) | range_offs[i+1];
				int start = (start_code[i] << 8) | start_code[i+1];
				int end   = (end_code[i] << 8) | end_code[i+1];
				int delta = (deltas[i] << 8) | deltas[i+1];

				if (ro) {
					u8 *extra = &range_offs[i] + ro;
					int n_chars = end - start + 1;
					for (int j = 0; j < n_chars*2; j += 2) {
						u16 idx = (extra[j] << 8) | extra[j+1];
						table.add(j, idx);
					}
				}
				else {
					for (int j = start; j <= end; j++) {
						u16 idx = j + delta;
						table.add(j, idx);
					}
				}
			}
		}
		else if (fmt == 12) {
			size = GET_BE32(buf, offset+4);
			int n_groups = GET_BE32(buf, offset+12);
			offset += 16;

			for (int i = 0; i < n_groups; i++) {
				u32 start = GET_BE32(buf, offset);
				u32 end = GET_BE32(buf, offset+4);
				u32 base = GET_BE32(buf, offset+8);
				u32 n_chars = end - start + 1;
				for (int j = 0; j < n_chars; j++) {
					table.add(start + j, base + j);
				}
			}
		}

		offset += size;
	}

	mfc.add(nullptr, table.len * sizeof(u64));
	u32 out_offset = mfc.size;

	Buffer flags;
	Bi_Vector coords;

	for (int i = 0; i < table.len; i++) {
		u64 item = table.data()[i];
		u32 id = (u32)item;
		u32 glyph_size = render_ttf_glyph(ttf, mfc, id, coords, flags);
		table.data()[i] = ((item >> 32ULL) << 32ULL) | out_offset;
		out_offset += glyph_size;
	}

	memcpy(mfc.data, table.data(), table.len * sizeof(u64));
}
