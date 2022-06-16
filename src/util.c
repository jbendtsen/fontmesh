#include "util.h"

#include <string.h>
#include <stdlib.h>

u8 *Buffer_add(Buffer *buf, u8 *add_buf, int add_size)
{
	if (add_size <= 0)
		return buf->data;

	if (buf->cap < 32)
		buf->cap = 32;

	int old_cap = buf->cap;
	int new_size = buf->size + add_size;
	while (buf->cap < new_size)
		buf->cap *= 2;

	if (!buf->data || buf->cap > old_cap) {
		u8 *new_data = (u8*)malloc(buf->cap);
		if (buf->data) {
			if (buf->size > 0)
				memcpy(new_data, buf->data, buf->size);

			free(buf->data);
		}
		buf->data = new_data;
	}

	u8 *ptr = &buf->data[buf->size];
	if (add_buf) {
		if (add_size > 1)
			memcpy(ptr, add_buf, add_size);
		else
			*ptr = *add_buf;
	}
	else
		memset(ptr, 0, add_size);

	buf->size = new_size;
	return ptr;
}

void Buffer_close(Buffer *buf) {
	if (buf->data) {
		free(buf->data);
		buf->data = NULL;
		buf->cap = 0;
		buf->size = 0;
	}
}

u64 *BiVector_data(BiVector *vec) {
	return vec->ptr ? vec->ptr : vec->initial;
}

void BiVector_resize(BiVector *vec, int new_len)
{
	if (new_len < 0)
		return;

	if (vec->cap < 8)
		vec->cap = 8;

	int old_cap = vec->cap;
	while (vec->cap < new_len)
		vec->cap *= 2;

	if (vec->cap > old_cap) {
		u64 *new_data = (u64*)malloc(vec->cap * sizeof(u64));
		u64 *old_data = vec->ptr;
		if (!old_data)
			old_data = vec->initial;

		memcpy(new_data, old_data, old_cap * sizeof(u64));
		if (vec->ptr)
			free(vec->ptr);
		vec->ptr = new_data;
	}

	vec->len = new_len;
}

void BiVector_reserve(BiVector *vec, int space)
{
	if (space > vec->cap) {
		int s = vec->len;
		BiVector_resize(vec, space);
		vec->len = s;
	}
}

void BiVector_add(BiVector *vec, u32 a, u32 b)
{
	u64 n = ((u64)a << 32ULL) | (u64)b;

	if (vec->len >= vec->cap)
		BiVector_resize(vec, vec->len+1);
	else
		vec->len++;

	u64 *data = vec->ptr ? vec->ptr : vec->initial;
	data[vec->len-1] = n;
}

void BiVector_get(BiVector *vec, int idx, u32 *a, u32 *b)
{
	u64 n = 0;
	if (idx >= 0 && idx < vec->len) {
		u64 *data = vec->ptr ? vec->ptr : vec->initial;
		n = data[idx];
	}

	if (a) *a = (u32)(n >> 32ULL);
	if (b) *b = (u32)n;
}

void BiVector_close(BiVector *vec) {
	if (vec->ptr) {
		free(vec->ptr);
		vec->ptr = NULL;
		vec->cap = 0;
		vec->len = 0;
	}
}