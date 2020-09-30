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

/// Functions for working with Pulse Code Modulation (PCM) data.
namespace PCM {

/// A 24-bit signed integer.
using int24_t = struct _int24_t {
    /// internal data for the 24-bit integer
    int32_t data : 24;

    // Constructors (intentionally not explicit to allow implied cast)

    /// Create a new 24-bit integer from an 8-bit signed value.
    _int24_t(int8_t integer) : data(integer) { }

    /// Create a new 24-bit integer from an 8-bit unsigned value.
    _int24_t(uint8_t integer) : data(integer) { }

    /// Create a new 24-bit integer from an 16-bit signed value.
    _int24_t(int16_t integer) : data(integer) { }

    /// Create a new 24-bit integer from an 16-bit unsigned value.
    _int24_t(uint16_t integer) : data(integer) { }

    /// Create a new 24-bit integer from an 32-bit signed value.
    explicit _int24_t(int32_t integer) : data(integer) { }

    /// Create a new 24-bit integer from an 32-bit unsigned value.
    explicit _int24_t(uint32_t integer) : data(integer) { }

    /// Create a new 24-bit integer from an 64-bit signed value.
    explicit _int24_t(uint64_t integer) : data(integer) { }

    /// Create a new 24-bit integer from an 64-bit unsigned value.
    explicit _int24_t(int64_t integer) : data(integer) { }

    // Operators - Assignment

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

    // Operators - Casting

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
};

// Operators - Comparison

/// Return true if this 24-bit value is equal to the given 8-bit signed value.
///
/// @tparam T the type of the value to compare against
/// @param l the int24_t on the left-hand side of the operation
/// @param l the other integer to compare the 24-bit value against
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

/// Return true if this 24-bit value is equal to the given 8-bit signed value.
///
/// @tparam T the type of the value to compare against
/// @param l the int24_t on the left-hand side of the operation
/// @param l the other integer to compare the 24-bit value against
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
// inline float pcm24_to_float(int32_t sample) {
//     return sample / std::numeric_limits<int32_t>::max();
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
// inline int32_t float_to_pcm24(float sample) {
//     return std::numeric_limits<int32_t>::max() * sample;
// }

}  // namespace PCM

#endif  // DSP_PCM_HPP_
