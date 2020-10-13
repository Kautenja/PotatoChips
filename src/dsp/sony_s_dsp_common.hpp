// Common functions for Sony S-DSP classes.
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
//

#ifndef DSP_SONY_S_DSP_COMMON_HPP_
#define DSP_SONY_S_DSP_COMMON_HPP_

#include <algorithm>
#include <cstdint>
#include <limits>

enum : unsigned {
    /// the sample rate of the S-DSP in Hz
    SAMPLE_RATE = 32000
};

/// Clamp an integer to a 16-bit value.
///
/// @param n a 32-bit integer value to clip
/// @returns n clipped to a 16-bit value [-32768, 32767]
///
inline int16_t clamp_16(int n) {
    const int lower = std::numeric_limits<int16_t>::min();
    const int upper = std::numeric_limits<int16_t>::max();
    return std::max(lower, std::min(n, upper));
}

/// @brief Returns the 14-bit pitch calculated from the given frequency.
///
/// @param frequency the frequency in Hz
/// @returns the 14-bit pitch corresponding to the S-DSP 32kHz sample rate
/// @details
///
/// \f$frequency = \f$SAMPLE_RATE\f$ * \frac{pitch}{2^{12}}\f$
///
static inline uint16_t get_pitch(float frequency) {
    // calculate the pitch based on the known relationship to frequency
    const auto pitch = static_cast<float>(1 << 12) * frequency / SAMPLE_RATE;
    // mask the 16-bit pitch to 14-bit
    return 0x3FFF & static_cast<uint16_t>(pitch);
}

#endif  // DSP_SONY_S_DSP_COMMON_HPP_
