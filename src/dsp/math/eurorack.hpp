// Constants defined by the eurorack standard.
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

#ifndef DSP_EURORACK_HPP
#define DSP_EURORACK_HPP

#include "functions.hpp"

/// @brief Basic mathematical functions.
namespace Math {

/// @brief Constants defined by the Eurorack standard.
namespace Eurorack {

/// the maximal DC voltage in the Eurorack standard
static const float DC_MAX_VOLTS = 10.f;
/// the minimal DC voltage in the Eurorack standard
static const float DC_MIN_VOLTS = -10.f;
/// the peak-to-peak DC voltage in the Eurorack standard
static const float DC_VOLTS_P2P = DC_MAX_VOLTS - DC_MIN_VOLTS;

/// @brief Normalize a DC voltage into the range \f$[-1, 1]\f$.
///
/// @param voltage the DC voltage to normalize to \f$[-1, 1]\f$
/// @returns the input DC voltage scaled to the range \f$[-1, 1]\f$
/// @details
/// If the DC voltage exceed the saturation range of Eurorack
/// (\f$[-10, 10]V\f$), the function _will not_ clip the voltage.
///
inline float fromDC(const float& voltage) {
    return voltage / DC_MAX_VOLTS;
}

/// @brief Return a DC voltage from the normalized range of \f$[-1, 1]\f$.
///
/// @param value the value in the normalized range \f$[-1, 1]\f$
/// @returns the output DC voltage scaled to the range \f$[-10, 10]\f$
/// @details
/// If the DC voltage exceed the saturation range of Eurorack
/// (\f$[-10, 10]V\f$), the function _will not_ clip the voltage.
///
inline float toDC(const float& value) {
    return value * DC_MAX_VOLTS;
}

/// the maximal AC voltage in the Eurorack standard
static const float AC_MAX_VOLTS = 5.f;
/// the minimal AC voltage in the Eurorack standard
static const float AC_MIN_VOLTS = -AC_MAX_VOLTS;
/// the peak-to-peak AC voltage in the Eurorack standard
static const float AC_VOLTS_P2P = AC_MAX_VOLTS - AC_MIN_VOLTS;

/// @brief Return a AC voltage normalized into the range \f$[-1, 1]\f$.
///
/// @param voltage the AC voltage to normalize to \f$[-1, 1]\f$
/// @returns the input AC voltage scaled to the range \f$[-1, 1]\f$
/// @details
/// If the AC voltage exceed the saturation range of Eurorack (\f$[-5, 5]V\f$),
/// the function _will not_ clip the voltage.
///
inline float fromAC(const float& voltage) {
    return voltage / AC_MAX_VOLTS;
}

/// @brief Return an AC voltage from the normalized range of \f$[-1, 1]\f$.
///
/// @param value the value in the normalized range \f$[-1, 1]\f$
/// @returns the output AC voltage scaled to the range \f$[-10, 10]\f$
/// @details
/// If the AC voltage exceed the saturation range of Eurorack
/// (\f$[-5, 5]V\f$), the function _will not_ clip the voltage.
///
inline float toAC(const float& value) {
    return value * AC_MAX_VOLTS;
}

/// @brief Convert the input voltage in V/OCT format to a frequency in Hertz.
///
/// @param voltage the voltage to convert to frequency in Hertz
/// @returns the frequency of corresponding to the input voltage based on V/OCT
/// @details
/// The frequency will be clamped to the audible range of \f$[0, 20000]Hz\f$.
///
inline float voct2freq(float voltage) {
    return clip(rack::dsp::FREQ_C4 * powf(2.0, voltage), 0.0f, 20000.0f);
}

}  // namespace Eurorack

}  // namespace Math

#endif  // DSP_EURORACK_HPP
