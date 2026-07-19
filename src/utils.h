#pragma once

#include "debug.h"

#include <stdint.h>

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)
#define STRINGIFY(x) #x

namespace engine::utils {

// Replacement for std::array<>
template <typename T, uint8_t Size>
struct Array {
    T raw[Size];

    constexpr T * data() { return raw; }
    constexpr const T * data() const { return raw; }
    constexpr uint8_t size() const { return Size; }

    constexpr T * begin() { return raw; }
    constexpr T * end() { return raw + Size; }

    constexpr T & operator[](uint8_t i) { ASSERT(i < Size); return raw[i]; }
    constexpr const T & operator[](uint8_t i) const { ASSERT(i < Size); return raw[i]; }

    constexpr bool operator==(const Array & o) const {
        for (int i = 0; i < Size; i++) {
            if (raw[i] != o.raw[i]) return false;
        }
        return true;
    }
    constexpr bool operator!=(const Array & o) const {
        return !operator==(o);
    }
};

// std::move()
#define MOVE(x) static_cast<decltype(x)&&>(x)

// std::swap()
template <typename T>
constexpr inline void swap(T & a, T & b) {
    T c = MOVE(a);
    a = MOVE(b);
    b = MOVE(c);
}

// std::reverse()
template <typename Iter>
constexpr inline void reverse(Iter b, Iter e) {
    while (true) {
        if (b == e || b + 1 == e) break;
        --e;
        swap(*b, *e);
        ++b;
    }
}

// int to string
constexpr inline auto to_string(int32_t i) {
    // Longest str is "-4294967296", so 11+NUL.
    Array<char, 12> result{}; // TODO: unnecessary initialisation
    char *d = result.data();
    if (i < 0) {
        *d++ = '-';
        i = -i; // assuming min isn't going to be used.
    }
    char * const start = d;
    if (i == 0) *d++ = '0';
    while (i) {
        *d++ = '0' + (i % 10);
        i /= 10;
    }
    reverse(start, d);
    *d++ = '\0';
    return result;
}

// std::size()
template <typename T, uint32_t N>
constexpr inline uint32_t size(const T (&)[N]) {
    return N;
}



// Sanity check that the above works.
namespace test {

constexpr int check_reverse() {
    {
        Array<char, 1> in = { 1 };
        reverse(in.data(), in.data());
        if (in[0] != 1) return 0;
    }
    {
        Array<char, 1> in = { 1 };
        const Array<char, 1> out = { 1 };
        reverse(in.begin(), in.end());
        if (in != out) return 1;
    }
    {
        Array<char, 2> in = { 1, 2 };
        const Array<char, 2> out = { 2, 1 };
        reverse(in.begin(), in.end());
        if (in != out) return 2;
    }
    {
        Array<char, 3> in = { 1, 2, 3 };
        const Array<char, 3> out = { 3, 2, 1 };
        reverse(in.begin(), in.end());
        if (in != out) return 3;
    }
    {
        Array<char, 4> in = { 1, 2, 3, 4 };
        const Array<char, 4> out = { 4, 3, 2, 1 };
        reverse(in.begin(), in.end());
        if (in != out) return 4;
    }
    return -1;
}
static_assert(check_reverse() == -1);

constexpr int check_to_string() {
    {
        constexpr auto str = to_string(0);
        constexpr Array<char, 12> e = { '0' };
        if (str != e) return 0;
    }
    {
        constexpr auto str = to_string(1);
        constexpr Array<char, 12> e = { '1' };
        if (str != e) return 1;
    }
    {
        constexpr auto str = to_string(-1);
        constexpr Array<char, 12> e = { '-', '1' };
        if (str != e) return 2;
    }
    {
        constexpr auto str = to_string(123);
        constexpr Array<char, 12> e = { '1', '2', '3' };
        if (str != e) return 3;
    }
    {
        constexpr auto str = to_string(-123);
        constexpr Array<char, 12> e = { '-', '1', '2', '3' };
        if (str != e) return 4;
    }
    return -1;
}
static_assert(check_to_string() == -1);

constexpr int check_size() {
    {
        constexpr char a[1] = {};
        if (size(a) != 1) return 1;
    }
    {
        constexpr char a[2] = {};
        if (size(a) != 2) return 2;
    }
    {
        constexpr uint32_t a[3] = {};
        if (size(a) != 3) return 3;
    }
    return -1;
}
static_assert(check_size() == -1);

} // test

} // engine::utils
