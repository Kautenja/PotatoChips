// An emulation of the Gaussian filter from the Sony S-DSP.
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

#ifndef DSP_SONY_S_DSP_GAUSSIAN_HPP_
#define DSP_SONY_S_DSP_GAUSSIAN_HPP_

#include "exceptions.hpp"
#include <cstring>

/// @brief An emulation of the Gaussian filter from the Sony S-DSP.
class Sony_S_DSP_Gaussian {
 public:
    /// the sample rate of the S-DSP in Hz
    static constexpr unsigned SAMPLE_RATE = 32000;

 private:
    /// The values of the Gaussian filter on the DAC of the SNES
    static int16_t const gauss[];

    /// The state of a synthesizer voice (channel) on the chip.
    struct VoiceState {
        /// 4th most recent samples
        short interp3 = 0;
        /// 3rd most recent samples
        short interp2 = 0;
        /// 2nd most recent samples
        short interp1 = 0;
        /// 1st most recent samples
        short interp0 = 0;
        /// 12-bit fractional position
        short fraction = 0x3FFF;
    } voice_state;

 public:
    /// @brief Initialize a new Sony_S_DSP_Gaussian.
    ///
    /// @param ram a pointer to the 64KB shared RAM
    ///
    Sony_S_DSP_Gaussian() { }

    /// @brief Run DSP for some samples and write them to the given buffer.
    ///
    /// @param output_buffer the output buffer to write samples to (optional)
    ///
    /// @details
    /// the sample rate of the system is locked to 32kHz just like the SNES
    ///
    void run(int16_t input, int16_t* output_buffer = NULL);
};

#endif  // DSP_SONY_S_DSP_GAUSSIAN_HPP_
