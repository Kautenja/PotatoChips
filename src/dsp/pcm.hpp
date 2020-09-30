// Functions for working with Pulse Code Modulation (PCM) data.
// Copyright 2020 Christian Kauten
// Copyright 2006 Shay Green
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
// derived from: Game_Music_Emu 0.5.2
//

#ifndef DSP_PCM_HPP_
#define DSP_PCM_HPP_

#include <cstdint>
#include <limits>

// TODO: constructors from float and double?
// TODO: casters to float and double?
/// A 24-bit signed integer data-type.
using int24_t = struct _int24_t {
    /// internal data for the 24-bit integer
    int32_t data : 24;

    // -----------------------------------------------------------------------
    // MARK: Constructors (intentionally not explicit to allow implied cast)
    // -----------------------------------------------------------------------

    // TODO: templates?

    /// Create a new 24-bit integer from an 8-bit signed value.
    constexpr _int24_t(int8_t integer) : data(integer) { }

    /// Create a new 24-bit integer from an 8-bit unsigned value.
    constexpr _int24_t(uint8_t integer) : data(integer) { }

    /// Create a new 24-bit integer from an 16-bit signed value.
    constexpr _int24_t(int16_t integer) : data(integer) { }

    /// Create a new 24-bit integer from an 16-bit unsigned value.
    constexpr _int24_t(uint16_t integer) : data(integer) { }

    // -----------------------------------------------------------------------
    // MARK: Constructors (explicit to require explicit cast from larger type)
    // -----------------------------------------------------------------------

    // TODO: templates?

    /// Create a new 24-bit integer from an 32-bit signed value.
    constexpr explicit _int24_t(int32_t integer) : data(integer) { }

    /// Create a new 24-bit integer from an 32-bit unsigned value.
    constexpr explicit _int24_t(uint32_t integer) : data(integer) { }

    /// Create a new 24-bit integer from an 64-bit signed value.
    constexpr explicit _int24_t(uint64_t integer) : data(integer) { }

    /// Create a new 24-bit integer from an 64-bit unsigned value.
    constexpr explicit _int24_t(int64_t integer) : data(integer) { }

    // -----------------------------------------------------------------------
    // MARK: Operators - Assignment
    // -----------------------------------------------------------------------

    // TODO: templates?

    /// Assign an 8-bit signed value to this 24-bit container.
    _int24_t& operator=(int8_t const& integer) {
        data = integer;
        return *this;
    }

    /// Assign an 8-bit unsigned value to this 24-bit container.
    _int24_t& operator=(uint8_t const& integer) {
        data = integer;
        return *this;
    }

    /// Assign an 16-bit signed value to this 24-bit container.
    _int24_t& operator=(int16_t const& integer) {
        data = integer;
        return *this;
    }

    /// Assign an 16-bit unsigned value to this 24-bit container.
    _int24_t& operator=(uint16_t const& integer) {
        data = integer;
        return *this;
    }

    // -----------------------------------------------------------------------
    // MARK: Operators - Type Casting
    // -----------------------------------------------------------------------

    // TODO: templates?

    explicit operator int8_t() { return data; }
    explicit operator int8_t() const { return data; }
    explicit operator uint8_t() { return data; }
    explicit operator uint8_t() const { return data; }
    explicit operator int16_t() { return data; }
    explicit operator int16_t() const { return data; }
    explicit operator uint16_t() { return data; }
    explicit operator uint16_t() const { return data; }
    operator int32_t() { return data; }
    operator int32_t() const { return data; }
    explicit operator uint32_t() { return data; }
    explicit operator uint32_t() const { return data; }
    operator int64_t() { return data; }
    operator int64_t() const { return data; }
    explicit operator uint64_t() { return data; }
    explicit operator uint64_t() const { return data; }
} __attribute__((packed));  // packed to consume 3 bytes instead of 4

// ---------------------------------------------------------------------------
// MARK: Operators - Equality Comparison (==)
// ---------------------------------------------------------------------------

/// Return true if this 24-bit value is equal to the other 24-bit value.
///
/// @param l the int24_t on the left-hand side of the operation
/// @param r the other integer to compare the 24-bit value against
/// @returns True if this 24-bit value is equal to the given value
///
inline bool operator==(int24_t const& l, int24_t const& r) {
    return l.data == r.data;
}

/// Return true if this 24-bit value is equal to the given value.
///
/// @tparam T the type of the value to compare against
/// @param l the int24_t on the left-hand side of the operation
/// @param r the other integer to compare the 24-bit value against
/// @returns True if this 24-bit value is equal to the given value
///
template<typename T>
inline bool operator==(int24_t const& l, T const& r) { return l.data == r; }
// 8-bit
template<int8_t> bool operator==(int24_t const&,  int8_t const&);
template<int8_t> bool operator==(int24_t const&, uint8_t const&);
// 16-bit
template<int8_t> bool operator==(int24_t const&,  int16_t const&);
template<int8_t> bool operator==(int24_t const&, uint16_t const&);
// 32-bit
template<int8_t> bool operator==(int24_t const&,  int32_t const&);
template<int8_t> bool operator==(int24_t const&, uint32_t const&);
// 64-bit
template<int8_t> bool operator==(int24_t const&,  int64_t const&);
template<int8_t> bool operator==(int24_t const&, uint64_t const&);

/// Return true if this 24-bit value is equal to the given value.
///
/// @tparam T the type of the value to compare against
/// @param l the int24_t on the left-hand side of the operation
/// @param r the other integer to compare the 24-bit value against
/// @returns True if this 24-bit value is equal to the given value
///
template<typename T>
inline bool operator==(T const& l, int24_t const& r) { return l == r.data; }
// 8-bit
template<int8_t> bool operator==( int8_t const&, int24_t const&);
template<int8_t> bool operator==(uint8_t const&, int24_t const&);
// 16-bit
template<int8_t> bool operator==( int16_t const&, int24_t const&);
template<int8_t> bool operator==(uint16_t const&, int24_t const&);
// 32-bit
template<int8_t> bool operator==( int32_t const&, int24_t const&);
template<int8_t> bool operator==(uint32_t const&, int24_t const&);
// 64-bit
template<int8_t> bool operator==( int64_t const&, int24_t const&);
template<int8_t> bool operator==(uint64_t const&, int24_t const&);

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

/// A numeric limits specialization for the int24_t class.
template<> class std::numeric_limits<int24_t> {
 public:
    static constexpr bool is_specialized = true;

    static constexpr int24_t max() noexcept { return int24_t(0x7fffff); }
    static constexpr int24_t min() noexcept { return int24_t(0xffffff); }
    static constexpr int24_t lowest() noexcept { return int24_t(0xffffff); }

    static constexpr int digits = 1;
    static constexpr int digits10 = 0;
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
