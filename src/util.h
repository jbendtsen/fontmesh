#pragma once

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef struct {
	u8 *data;
	int size;
	int cap;
} Buffer;

u8 *Buffer_add(Buffer *buf, u8 *add_buf, int add_size); // add_buf may be null
void Buffer_close(Buffer *buf);

typedef struct {
	u64 initial[8];
	u64 *ptr;
	int cap;
	int len;
} BiVector;

u64 *BiVector_data(BiVector *vec);
void BiVector_resize(BiVector *vec, int new_size);
void BiVector_reserve(BiVector *vec, int space);
void BiVector_add(BiVector *vec, u32 a, u32 b);
void BiVector_get(BiVector *vec, int idx, u32 *a, u32 *b);
void BiVector_close(BiVector *vec);