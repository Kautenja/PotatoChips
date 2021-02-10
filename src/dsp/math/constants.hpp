// Constants for math functions.
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

#ifndef DSP_MATH_CONSTANTS_HPP
#define DSP_MATH_CONSTANTS_HPP

/// @brief Basic mathematical functions.
namespace Math {

/// the value of \f$\epsilon\f$ in single-precision floating point numbers
static constexpr float EPSILON = 1e-7;

/// the value of \f$\epsilon\f$ in single-precision floating point numbers
// static constexpr double EPSILON = ?;

/// the value of \f$\pi\f$ in single-precision floating point
static constexpr float PI = 3.1415926535897932384626433832795028841971693993751058209749445923078164062;

/// the value of \f$\pi\f$ in double-precision floating point
// static constexpr double PI = 3.1415926535897932384626433832795028841971693993751058209749445923078164062;

}  // namespace Math

#endif  // DSP_MATH_CONSTANTS_HPP
