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
        int16_t interp3 = 0;
        /// 3rd most recent samples
        int16_t interp2 = 0;
        /// 2nd most recent samples
        int16_t interp1 = 0;
        /// 1st most recent samples
        int16_t interp0 = 0;
        /// 12-bit fractional position
        int16_t fraction = 0x3FFF;
        /// TODO:
        bool filter1 = true;
        /// TODO:
        bool filter2 = true;
    } voice_state;

 public:
    /// @brief Initialize a new Sony_S_DSP_Gaussian.
    Sony_S_DSP_Gaussian() { }

    /// @brief Run the Gaussian filter for the given input sample.
    ///
    /// @param input the 16-bit PCM sample to pass through the Gaussian filter
    ///
    int16_t run(int16_t input);
};

#endif  // DSP_SONY_S_DSP_GAUSSIAN_HPP_
