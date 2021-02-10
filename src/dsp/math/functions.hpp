// Basic mathematical functions.
// Copyright 2020 Christian Kauten
//
// Author: Christian Kauten (kautenja@auburn.edu)
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

#ifndef DSP_MATH_FUNCTIONS_HPP
#define DSP_MATH_FUNCTIONS_HPP

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include "constants.hpp"

/// @brief Basic mathematical functions.
namespace Math {

/// @brief Clip the given value within the given limits.
///
/// @tparam the type of values to clamp
/// @param x the value to clamp
/// @param lower the lower bound to clamp the value to
/// @param upper the upper bound to clamp the value to
/// @returns the value clamped within \f$[lower, upper]\f$
///
template <typename T>
inline T clip(const T& x, const T& lower, const T& upper) {
    return std::max(lower, std::min(x, upper));
}

/// @brief Return the sign of the given value.
///
/// @tparam the type of the value to return the sign of
/// @param x the floating point number to get the sign of
/// @returns \f$1\f$ if the number is positive, \f$-1\f$ otherwise
///
template<typename T>
inline T sgn(const T& x) {
    return std::signbit(x) ? -1 : 1;
}

/// @brief Return the modulo operation between two values.
///
/// @param a the input to the modulo function
/// @param b the upper bound of the modulo function
/// @returns \f$a \mathrm{mod} b\f$
///
template<typename T>
inline T mod(const T& a, const T& b) {
    return (a % b + b) % b;
}

/// @brief Return `base` raised to the power of 2.
///
/// @param x the base value to raise to the power of 2
/// @returns the evaluation of \f$\texttt{base}^2\f$
///
template<typename T>
inline T squared(const T& x) {
    return x * x;
}

/// @brief Return `base` raised to the power of 3.
///
/// @param x the base value to raise to the power of 3
/// @returns the evaluation of \f$\texttt{base}^3\f$
///
template<typename T>
inline T cubed(const T& x) {
    return x * x * x;
}

/// @brief Return the input value converted to decibels.
///
/// @param x the value to convert to decibel scale
/// @returns \f$20 * \mathrm{log}_{10}(\texttt{value})\f$
///
template<typename T>
inline T decibels(const T& x) {
    return static_cast<T>(20) * log10(abs(x));
}

/// @brief Quantize a single-precision float value to the given number of bits.
///
/// @tparam the type of number to quantize
/// @param value the value \f$\in [-1, 1]\f$ to quantize
/// @param bits to number of bits to quantize the value to
/// @returns the value quantized to the given number of bits
/// @details
/// Numbers are quantized by:
///
/// 1. computing the maximal unsigned value of the number system as
///    \f$max \gets 2^{bits} - 1\f$;
/// 2. scaling the input value from \f$[-1, 1]\f$ to \f$[-max, max]\f$;
/// 3. truncating the scaled value to its integral component; and
/// 4. scaling the quantized integer from \f$[-max, max]\f$ back to floating
///    point in \f$[-1, 1]\f$
///
inline float quantize(const float& value, const uint8_t& bits) {
    // determine the maximal value in the number system, i.e., (2^bits) - 1
    const float max = (1 << bits) - 1;
    // convert from floating point in [-1.f, 1.f] to integer in [-max, max]
    const int32_t integer_value = value * max;
    // convert the integer in [-max, max] back to quantized T in [-1.f, 1.f]
    return integer_value / max;
}

/// @brief Return the output of the sine function.
///
/// @tparam the type of values to compute
/// @param x the input to the sine function in radians
/// @returns the output of the sine function for given input
/// @details
/// This implements the _Bhaskara_ approximation of the sine function:
///
/// \f$\sin(x) \approx \frac{16x(\pi - x)}{5\pi^2 - 4x(\pi - x)}\f$
///
template <typename T>
inline T sin_bhaskara(const T& x) {
    return (16 * x * (PI - x)) / (5 * squared(PI) - 4 * x * (PI - x));
}

/// @brief Return the output of the sine function.
///
/// @tparam the type of values to compute
/// @param x the input to the sine function in radians \f$\in [0, 2\pi]\f$
/// @returns the output of the sine function for given input
/// @details
/// This implements a polynomial approximation of sine computed by Ridge
/// regression.
///
template <typename T>
inline T sin_poly3(const T& x) {
    static constexpr T a = 0.09093347;
    static constexpr T b = -0.85559925;
    static constexpr T c = 1.84028261;
    return a * cubed(x) + b * squared(x) + c * x;
}

/// @brief Return the output of the cosine function.
///
/// @tparam the type of values to compute
/// @param x the input to the cosine function in radians
/// @returns the output of the cosine function for given input
/// @details
/// This implements the _Bhaskara_ approximation of the cosine function:
///
/// \f$\cos(y) \approx \frac{\pi^2 - 4y^2}{\pi^2 + y^2}\f$
///
template <typename T>
inline T cos_bhaskara(const T& x) {
    return (squared(PI) - 4 * squared(x)) / (squared(PI) + squared(x));
}

}  // namespace Math

#endif  // DSP_MATH_FUNCTIONS_HPP
