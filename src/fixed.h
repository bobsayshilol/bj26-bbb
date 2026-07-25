#pragma once

#include "debug.h"

namespace engine::utils {

template <typename T> struct ToUnsigned;
template <> struct ToUnsigned<int32_t> { using type = uint32_t; };
template <> struct ToUnsigned<int16_t> { using type = uint16_t; };
template <> struct ToUnsigned<int8_t> { using type = uint8_t; };
template <> struct ToUnsigned<uint32_t> { using type = uint32_t; };
template <> struct ToUnsigned<uint16_t> { using type = uint16_t; };
template <> struct ToUnsigned<uint8_t> { using type = uint8_t; };

template <typename T> struct ToSigned;
template <> struct ToSigned<int32_t> { using type = int32_t; };
template <> struct ToSigned<int16_t> { using type = int16_t; };
template <> struct ToSigned<int8_t> { using type = int8_t; };
template <> struct ToSigned<uint32_t> { using type = int32_t; };
template <> struct ToSigned<uint16_t> { using type = int16_t; };
template <> struct ToSigned<uint8_t> { using type = int8_t; };

template <typename T> struct IsSigned; // Intentionally explicit
template <> struct IsSigned<int32_t> { static inline constexpr bool value = true; };
template <> struct IsSigned<int16_t> { static inline constexpr bool value = true; };
template <> struct IsSigned<int8_t> { static inline constexpr bool value = true; };
template <> struct IsSigned<uint32_t> { static inline constexpr bool value = false; };
template <> struct IsSigned<uint16_t> { static inline constexpr bool value = false; };
template <> struct IsSigned<uint8_t> { static inline constexpr bool value = false; };

// Signed fixed point number.
template <typename Integer, typename Storage, uint8_t Shift>
struct FixedT {
    Storage raw;

    static inline constexpr bool IsSigned = engine::utils::IsSigned<Integer>::value;
    using UInteger = typename ToUnsigned<Integer>::type;
    using SStorage = typename ToSigned<Storage>::type;

    constexpr Integer value() const { return static_cast<Integer>(raw >> Shift); }

    constexpr static FixedT div(Integer num, Integer den) {
        FixedT f;
        f.raw = static_cast<SStorage>(static_cast<UInteger>(num) << Shift) / den;
        return f;
    }
    constexpr static FixedT from(Integer num) { return div(num, 1); }

    constexpr explicit FixedT() : raw{0} {}

    constexpr FixedT & operator+=(const FixedT & f) { raw += f.raw; return *this; }
    constexpr FixedT & operator-=(const FixedT & f) { raw -= f.raw; return *this; }

    constexpr FixedT operator+(const FixedT & f) { FixedT t = *this; t += f; return t; }
    constexpr FixedT operator-(const FixedT & f) { FixedT t = *this; t -= f; return t; }

    constexpr FixedT operator-() {
        FixedT t = *this;
        t.raw = 1 + ~t.raw;
        return t;
    }
};

using FixedS1616 = FixedT<int16_t, uint32_t, 16>;
//using FixedS248 = FixedT<int32_t, uint32_t, 8>;
using FixedS88 = FixedT<int8_t, uint16_t, 8>;
using FixedU1616 = FixedT<uint16_t, uint32_t, 16>;
//using FixedU248 = FixedT<uint32_t, uint32_t, 8>;
using FixedU88 = FixedT<uint16_t, uint32_t, 16>;

namespace test {

template <typename Fixed>
constexpr int check_fixed() {
    Fixed fixed;
    {
        fixed = Fixed::from(0); if (fixed.value() != 0) return 0;
        fixed = Fixed::from(1); if (fixed.value() != 1) return 1;
        fixed = Fixed::from(100); if (fixed.value() != 100) return 2;
    }
    if constexpr (Fixed::IsSigned) {
        fixed = Fixed::from(-1); if (fixed.value() != -1) return 3;
        fixed = Fixed::from(-100); if (fixed.value() != -100) return 4;
    }
    {
        fixed = Fixed::from(0) + Fixed::from(0); if (fixed.value() != 0) return 5;
        fixed = Fixed::from(0) + Fixed::from(1); if (fixed.value() != 1) return 5;
        fixed = Fixed::from(1) + Fixed::from(0); if (fixed.value() != 1) return 5;
        fixed = Fixed::from(1) + Fixed::from(1); if (fixed.value() != 2) return 5;
    }
    if constexpr (Fixed::IsSigned) {
        fixed = Fixed::from(0) + Fixed::from(-1); if (fixed.value() != -1) return 5;
        fixed = Fixed::from(-1) + Fixed::from(0); if (fixed.value() != -1) return 5;
        fixed = Fixed::from(-1) + Fixed::from(-1); if (fixed.value() != -2) return 5;
    }
    {
        fixed = Fixed::div(0, 2); if (fixed.value() != 0) return 6;
        fixed = Fixed::div(1, 2); if (fixed.value() != 0) return 6;
        fixed = Fixed::div(2, 2); if (fixed.value() != 1) return 6;
        fixed = Fixed::div(3, 2); if (fixed.value() != 1) return 6;
        fixed = Fixed::div(1, 2) + Fixed::div(1, 2); if (fixed.value() != 1) return 6;
    }
    if constexpr (Fixed::IsSigned) {
        fixed = Fixed::div(0, 2); if (fixed.value() != 0) return 7;
        fixed = Fixed::div(-1, 2); if (fixed.value() != -1) return 7; // TODO: unexpected?
        fixed = Fixed::div(-2, 2); if (fixed.value() != -1) return 7;
        fixed = Fixed::div(-3, 2); if (fixed.value() != -2) return 7; // TODO: unexpected?
        fixed = Fixed::div(-1, 2) + Fixed::div(-1, 2); if (fixed.value() != -1) return 7;
    }
    if constexpr (Fixed::IsSigned) {
        fixed = -Fixed::from(0); if (fixed.value() != 0) return 8;
        fixed = -Fixed::from(1); if (fixed.value() != -1) return 8;
        fixed = -Fixed::from(2); if (fixed.value() != -2) return 8;
    }
    return -1;
};
static_assert(check_fixed<FixedS1616>() == -1);
//static_assert(check_fixed<FixedS248>() == -1);
static_assert(check_fixed<FixedS88>() == -1);
static_assert(check_fixed<FixedU1616>() == -1);
//static_assert(check_fixed<FixedU248>() == -1);
static_assert(check_fixed<FixedU88>() == -1);

} // namespace test

} // namespace engine::utils
