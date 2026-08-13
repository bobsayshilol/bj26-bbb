#pragma once

#include "utils.h"

namespace engine::utils {

// std::vector-like
template <typename T, uint8_t Capacity>
class Vector {
    T m_data[Capacity] = {}; // for constexpr
    uint8_t m_size = 0;

public:
    constexpr T * data() { return m_data; }
    constexpr const T * data() const { return m_data; }
    constexpr uint8_t size() const { return m_size; }
    constexpr bool empty() const { return m_size == 0; }
    constexpr bool full() const { return m_size == Capacity; }

    constexpr T * begin() { return m_data; }
    constexpr T * end() { return m_data + m_size; }

    constexpr T & front() { return m_data[0]; }
    constexpr T & back() { return m_data[m_size - 1]; }

    constexpr T & operator[](uint8_t i) { ASSERT(i < m_size); return m_data[i]; }
    constexpr const T & operator[](uint8_t i) const { ASSERT(i < m_size); return m_data[i]; }

    constexpr T & push_back(T val) { ASSERT(m_size < Capacity); return m_data[m_size++] = MOVE(val); }
    constexpr void pop_back() { ASSERT(m_size > 0); m_size--; }
    constexpr void clear() { m_size = 0; }

    // Doesn't maintain order.
    constexpr void remove_fast(uint8_t idx) {
        ASSERT(idx < m_size);
        swap(m_data[idx], m_data[m_size - 1]);
        pop_back();
    }
};

namespace test {

constexpr int check_vec() {
    {
        Vector<int, 3> v;
        if (v.size() != 0) return 0;
        if (v.full()) return 0;
        if (!v.empty()) return 0;

        v.push_back(1); // [1]
        if (v.size() != 1) return 1;
        if (v.full()) return 1;
        if (v.empty()) return 1;
        if (v[0] != 1) return 1;
        if (v.front() != 1) return 1;
        if (v.back() != 1) return 1;

        v.push_back(2); // [1,2]
        if (v.size() != 2) return 2;
        if (v[0] != 1) return 2;
        if (v[1] != 2) return 2;
        if (v.front() != 1) return 2;
        if (v.back() != 2) return 2;

        v.push_back(3); // [1,2,3]
        if (v.size() != 3) return 3;
        if (!v.full()) return 3;
        if (v.empty()) return 3;
        if (v[0] != 1) return 3;
        if (v[1] != 2) return 3;
        if (v[2] != 3) return 3;
        if (v.back() != 3) return 3;

        v.pop_back(); // [1,2]
        if (v.size() != 2) return 4;
        if (v[0] != 1) return 4;
        if (v[1] != 2) return 4;
        if (v.back() != 2) return 4;

        v.remove_fast(0); // [2]
        if (v.size() != 1) return 5;
        if (v[0] != 2) return 5;
        if (v.back() != 2) return 5;
    }
    return -1;
}
static_assert(check_vec() == -1);

} // namespace test

} // namespace engine::utils
