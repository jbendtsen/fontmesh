#include "util.h"

u8 *Buffer::add(u8 *add_buf, int add_size)
{
	if (add_size <= 0)
		return data;

	if (cap < 32)
		cap = 32;

	int old_cap = cap;
	int new_size = size + add_size;
	while (cap < new_size)
		cap *= 2;

	if (cap > old_cap) {
		u8 *new_data = new u8[cap];
		if (data) {
			if (size > 0)
				memcpy(new_data, data, size);

			delete[] data;
		}
		data = new_data;
	}

	u8 *ptr = &data[size];
	if (add_buf) {
		if (add_size > 1)
			memcpy(ptr, add_buf, add_size);
		else
			*ptr = *add_buf;
	}
	else
		memset(ptr, 0, add_size);

	size = new_size;
	return ptr;
}

u64 *Bi_Vector::data() {
	return ptr ? ptr : initial;
}

void Bi_Vector::resize(int new_len)
{
	if (new_size < 0)
		return;

	if (cap < 8)
		cap = 8;

	int old_cap = cap;
	while (cap < new_len)
		cap *= 2;

	if (cap > old_cap) {
		u64 *new_data = new u64[cap];
		u64 *old_data = ptr;
		if (!old_data)
			old_data = initial;

		memcpy(new_data, old_data, old_cap * sizeof(u64));
		if (ptr)
			delete[] ptr;
		ptr = new_data;
	}

	len = new_len;
}

void Bi_Vector::reserve(int space)
{
	if (space > cap) {
		int s = len;
		resize(space);
		len = s;
	}
}

void Bi_Vector::add(u32 a, u32 b)
{
	u64 n = ((u64)a << 32ULL) | (u64)b;

	if (len >= cap)
		resize(len+1);
	else
		len++;

	data()[len-1] = n;
}

void Bi_Vector::get(int idx, u32 *a, u32 *b)
{
	u64 n = 0;
	if (idx >= 0 && idx < len)
		n = data()[idx];

	if (a) *a = (u32)(n >> 32ULL);
	if (b) *b = (u32)n;
}
