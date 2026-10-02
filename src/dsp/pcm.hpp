// Functions for working with Pulse Code Modulation (PCM) data.
// Copyright 2020 Christian Kauten
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef DSP_PCM_HPP_
#define DSP_PCM_HPP_

#include <cstdint>
#include <limits>
#include <type_traits>

/// A signed 24-bit PCM integer with explicit little-endian byte storage.
/// Narrowing retains the low 24 bits; decoding sign-extends without relying on
/// compiler bitfield layout or a signed right shift. The Rack ABI is unchanged.
struct int24_t {
    uint8_t bytes[3];

    template<typename T, typename std::enable_if<std::is_integral<T>::value &&
        (sizeof(T) <= 2), int>::type = 0>
    constexpr int24_t(T value) : bytes{uint8_t(uint64_t(value)),
        uint8_t(uint64_t(value) >> 8), uint8_t(uint64_t(value) >> 16)} {}

    template<typename T, typename std::enable_if<std::is_integral<T>::value &&
        (sizeof(T) > 2), int>::type = 0>
    constexpr explicit int24_t(T value) : bytes{uint8_t(uint64_t(value)),
        uint8_t(uint64_t(value) >> 8), uint8_t(uint64_t(value) >> 16)} {}

    /// Decode in the signed 24-bit range, using only representable int32 values.
    constexpr int32_t value() const {
        return int32_t(uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
            (uint32_t(bytes[2]) << 16)) - ((bytes[2] & 0x80) ? 0x1000000 : 0);
    }

    template<typename T>
    typename std::enable_if<std::is_integral<T>::value, int24_t&>::type
    operator=(T integer) {
        *this = int24_t(integer);
        return *this;
    }

    constexpr operator int32_t() const { return value(); }
    constexpr operator int64_t() const { return value(); }
    constexpr explicit operator int8_t() const { return int8_t(value()); }
    constexpr explicit operator uint8_t() const { return uint8_t(value()); }
    constexpr explicit operator int16_t() const { return int16_t(value()); }
    constexpr explicit operator uint16_t() const { return uint16_t(value()); }
    constexpr explicit operator uint32_t() const { return uint32_t(value()); }
    constexpr explicit operator uint64_t() const { return uint64_t(value()); }
};
static_assert(sizeof(int24_t) == 3, "PCM storage must contain exactly three bytes");

inline bool operator==(const int24_t& l, const int24_t& r) {
    return l.value() == r.value();
}

/// Restrict comparisons to numbers so unrelated enums/classes retain their
/// own overloads (including test-framework status enums).
template<typename T>
inline typename std::enable_if<std::is_arithmetic<T>::value, bool>::type
operator==(const int24_t& l, const T& r) { return l.value() == r; }
template<typename T>
inline typename std::enable_if<std::is_arithmetic<T>::value, bool>::type
operator==(const T& l, const int24_t& r) { return l == r.value(); }

// ---------------------------------------------------------------------------
// MARK: Operators - Not Equality Comparison (!=)
// ---------------------------------------------------------------------------

// TODO:

// ---------------------------------------------------------------------------
// MARK: Operators - Greater than Equality Comparison (>=)
// ---------------------------------------------------------------------------

// TODO:

// ---------------------------------------------------------------------------
// MARK: Operators - Greater than Comparison (>)
// ---------------------------------------------------------------------------

// TODO:

// ---------------------------------------------------------------------------
// MARK: Operators - Less than Equality Comparison (<=)
// ---------------------------------------------------------------------------

// TODO:

// ---------------------------------------------------------------------------
// MARK: Operators - Less Comparison (<)
// ---------------------------------------------------------------------------

// TODO:

// ---------------------------------------------------------------------------
// MARK: Operators - Equality Comparison (==)
// ---------------------------------------------------------------------------

// TODO:

// ---------------------------------------------------------------------------
// MARK: Operators - Addition (+)
// ---------------------------------------------------------------------------

// TODO:

// ---------------------------------------------------------------------------
// MARK: Operators - Subtraction (-)
// ---------------------------------------------------------------------------

// TODO:

// ---------------------------------------------------------------------------
// MARK: Operators - Multiplication (*)
// ---------------------------------------------------------------------------

// TODO:

// ---------------------------------------------------------------------------
// MARK: Operators - Division (/)
// ---------------------------------------------------------------------------

// TODO:

// ---------------------------------------------------------------------------
// MARK: Numeric Limits
// ---------------------------------------------------------------------------

namespace std {

/// A numeric limits specialization for the int24_t class.
template<> class numeric_limits<int24_t> {
 public:
    static constexpr bool is_specialized = true;

    static constexpr int24_t max() noexcept { return int24_t(0x7fffff); }
    static constexpr int24_t min() noexcept { return int24_t(-0x800000); }
    static constexpr int24_t lowest() noexcept { return min(); }

    static constexpr int digits = 23;
    static constexpr int digits10 = 6;
    static constexpr int max_digits10 = 0;

    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr int radix = 2;
    static constexpr bool epsilon() noexcept { return 0; }
    static constexpr bool round_error() noexcept { return 0; }

    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;

    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr float_denorm_style has_denorm = denorm_absent;
    static constexpr bool has_denorm_loss = false;
    static constexpr bool infinity() noexcept { return 0; }
    static constexpr bool quiet_NaN() noexcept { return 0; }
    static constexpr bool signaling_NaN() noexcept { return 0; }
    static constexpr bool denorm_min() noexcept { return 0; }

    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;

    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;
    static constexpr float_round_style round_style = round_toward_zero;
};

}  // namespace std

/// Functions for working with Pulse Code Modulation (PCM) data.
namespace PCM {

// ---------------------------------------------------------------------------
// MARK: PCM to floating point
// ---------------------------------------------------------------------------

// /// Convert a PCM sample to a floating point value \f$\in [-1, 1]\f$.
// ///
// /// @tparam T the data-type of the PCM sample
// /// @param sample the PCM sample to convert to floating point
// /// @returns the PCM sample as a floating point value \f$\in [-1, 1]\f$
// ///
// template<typename T>
// inline float pcm_to_float(T sample) {
//     return sample / std::numeric_limits<T>::max();
// }

/// Convert a 16-bit PCM sample to a floating point value \f$\in [-1, 1]\f$.
///
/// @param sample the PCM sample to convert to floating point
/// @returns the PCM sample as a floating point value \f$\in [-1, 1]\f$
///
inline float pcm16_to_float(int16_t sample) {
    return sample / std::numeric_limits<int16_t>::max();
}

// /// Convert a 24-bit PCM sample to a floating point value \f$\in [-1, 1]\f$.
// ///
// /// @param sample the PCM sample to convert to floating point
// /// @returns the PCM sample as a floating point value \f$\in [-1, 1]\f$
// ///
// inline float pcm24_to_float(int24_t sample) {
//     return sample / std::numeric_limits<int24_t>::max();
// }

// ---------------------------------------------------------------------------
// MARK: Floating point to PCM
// ---------------------------------------------------------------------------

// /// Convert a floating point PCM sample to a finite representation of given type.
// ///
// /// @tparam T the integer data-type to convert the PCM sample to
// /// @param sample the floating point PCM sample \f$\in [-1, 1]\f$
// /// @returns the PCM sample scaled into the new data-types binary space
// ///
// template<typename T>
// inline T float_to_pcm(float sample) {
//     return std::numeric_limits<T>::max() * sample;
// }

/// Convert a floating point PCM sample to 16-bit PCM.
///
/// @param sample the floating point PCM sample \f$\in [-1, 1]\f$
/// @returns the PCM sample scaled into a 16-bit integer data-type
///
inline int16_t float_to_pcm16(float sample) {
    return std::numeric_limits<int16_t>::max() * sample;
}

// /// Convert a floating point PCM sample to 24-bit PCM.
// ///
// /// @param sample the floating point PCM sample \f$\in [-1, 1]\f$
// /// @returns the PCM sample scaled into a 24-bit integer data-type
// ///
// inline int24_t float_to_pcm24(float sample) {
//     return std::numeric_limits<int24_t>::max() * sample;
// }

}  // namespace PCM

#endif  // DSP_PCM_HPP_
