#pragma once

///--- Core
#include <ciso646>
#include <cstdio>
#include <cstdlib>
#include <string>

using i32 = int32_t;
using i64 = int64_t;
using u8 = uint8_t;
using u8x4 = u8[4];
using u32 = uint32_t;
using u32x3 = u32[3];
using u32x4 = u32[4];
using u64 = uint64_t;
using f32 = float;
using f64 = double;

template<typename T> T max(T a, T b) { return a > b ? a : b; }
template<typename T> T min(T a, T b) { return a < b ? a : b; }
template<typename T> T clamp(T a, T l, T h) { return max(l, min(a, h)); }
template<typename T> T saturate(T a) { return max(T{0}, min(a, T{1})); }
template<typename C, typename T> C && reinterpret_move(T & t) { return reinterpret_cast<C &&>(t);}

template<typename T>
struct unique_one
{
	T * thing;
	unique_one() : thing(nullptr) {}
	unique_one(T * && raw) : thing(raw) { raw = nullptr; }
    unique_one(unique_one const &) = delete;
    unique_one& operator=(unique_one const &) = delete;
    unique_one(unique_one && o) : thing(o.thing) { o.thing = nullptr; }
    unique_one& operator=(unique_one && o) { delete thing; thing = o.thing; o.thing = nullptr; return *this;}
	~unique_one() { delete thing; }

	operator bool() const { return thing != nullptr; }
	const T & operator ->() const { return *thing; }
};

template<typename T>
struct unique_array
{
	T * things;
	unique_array() : things(nullptr) {}
	unique_array(T * && raw) : things(raw) { raw = nullptr; }
    unique_array(unique_array const &) = delete;
    unique_array& operator=(unique_array const &) = delete;
    unique_array(unique_array && o) : things(o.things) { o.things = nullptr; }
    unique_array& operator=(unique_array && o) { delete[] things; things = o.things; o.things = nullptr; return *this;}
	~unique_array() { delete[] things; }

	operator bool() const { return things != nullptr; }
	T & operator [](int idx) const { return things[idx]; }
	operator T *() const { return things; }

    // TODO(bekorn): maybe keep the size and have begin()/end()
};

template<typename T>
struct span {
	T * ptr = nullptr;
	int size = 0;

	bool empty() const { return size == 0; }
    T & operator[](size_t idx) { return ptr[idx]; }
    T * begin() const { return ptr; }
    T * end() const { return ptr + size; }
};


///--- Utils
#include <stdarg.h>

// see https://c-faq.com/varargs/handoff.html for the *_err functions
void exit_err(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

	exit(EXIT_FAILURE);
}

void print_err(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}


///--- Graphics
struct Image
{
    // TODO(bekorn): const Image == const storage/x/y + mutable pixels
	unique_array<u8x4> pixels;
	i32 x, y;

    Image(int x, int y, nullptr_t) : pixels(new u8x4[x * y]), x(x), y(y) {}
    Image(int x, int y, u8x4 * && pixels) : pixels(std::move(pixels)), x(x), y(y) {}

    void blit_into(Image & o) const
    { memcpy(o.pixels, pixels, o.x * o.y * sizeof(u8x4)); }
};
