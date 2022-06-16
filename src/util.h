#pragma once

#include <string.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

struct Buffer {
	u8 *data = nullptr;
	int size = 0;
	int cap = 0;

	Buffer() = default;
	~Buffer() {
		if (data) delete[] data;
	}

	void add(u8 *add_buf, int add_size); // add_buf may be null
};

struct Bi_Vector {
	u64 initial[8];
	u64 *ptr = nullptr;
	int cap = 0;
	int len = 0;

	Bi_Vector() = default;
	~Bi_Vector() {
		if (ptr) delete[] ptr;
	}

	u64 *data();
	void resize(int new_size);
	void reserve(int space);
	void add(u32 a, u32 b);
	void get(int idx, u32 *a, u32 *b);
};
