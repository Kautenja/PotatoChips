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

/// @brief Return the input decibel value converted to amplitude.
///
/// @tparam T the type of number to convert to amplitude
/// @param x the decibel value to convert to linear scale
/// @returns the amplitude value \f$10^{\frac{x}{20}}\f$
/// @details
/// It is assumed that signals exist in the unit domain \f$\in [-1, 1]\f$.
///
template<typename T>
inline T decibels2amplitude(const T& x) {
    return pow(T(10), x / T(20));
}

}  // namespace Math

#endif  // DSP_MATH_FUNCTIONS_HPP
