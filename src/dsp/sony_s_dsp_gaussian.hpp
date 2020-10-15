// An emulation of the Gaussian filter from the Sony S-DSP.
// Copyright 2020 Christian Kauten
// Copyright 2006 Shay Green
// Copyright 2002 Brad Martin
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
// Based on Brad Martin's OpenSPC DSP emulator
//

#ifndef DSP_SONY_S_DSP_GAUSSIAN_HPP_
#define DSP_SONY_S_DSP_GAUSSIAN_HPP_

#include "sony_s_dsp_common.hpp"

/// @brief An emulation of the Gaussian filter from the Sony S-DSP.
/// @details
/// The emulator consumes 16 bytes of RAM and is aligned to 16-byte addresses.
class __attribute__((packed, aligned(16))) Sony_S_DSP_Gaussian {
 private:
    // -----------------------------------------------------------------------
    // Byte 1,2, 3,4, 5,6, 7,8
    // -----------------------------------------------------------------------
    /// a history of the four most recent samples
    int16_t samples[4] = {0, 0, 0, 0};
    // -----------------------------------------------------------------------
    // Byte 9,10
    // -----------------------------------------------------------------------
    /// 12-bit fractional position in the Gaussian table
    int16_t fraction = 0x3FFF;
    // -----------------------------------------------------------------------
    // Byte 11,12
    // -----------------------------------------------------------------------
    /// the volume level after the Gaussian filter
    int16_t volume = 0;
    // -----------------------------------------------------------------------
    // Byte 13,14
    // -----------------------------------------------------------------------
    /// the 14-bit frequency value
    uint16_t rate = 0;
    // -----------------------------------------------------------------------
    // Byte 15
    // -----------------------------------------------------------------------
    /// the discrete filter mode (i.e., the set of coefficients to use)
    uint8_t filter = 0;
    // -----------------------------------------------------------------------
    // Byte 16
    // -----------------------------------------------------------------------
    /// a dummy byte for byte alignment to 16-bytes
    const uint8_t unused_spacer_for_byte_alignment;

 public:
    /// the sample rate of the S-DSP in Hz
    static constexpr unsigned SAMPLE_RATE = 32000;

    /// @brief Initialize a new Sony_S_DSP_Gaussian.
    Sony_S_DSP_Gaussian() : unused_spacer_for_byte_alignment(0) { }

    /// @brief Set the filter coefficients to a discrete mode.
    ///
    /// @param filter the new mode for the filter
    ///
    inline void setFilter(uint8_t filter) { this->filter = filter & 0x3; }

    /// @brief Set the volume level of the low-pass gate to a new value.
    ///
    /// @param volume the volume level after the Gaussian low-pass filter
    ///
    inline void setVolume(int8_t volume) { this->volume = volume; }

    /// @brief Set the frequency of the low-pass gate to a new value.
    ///
    /// @param freq the frequency to set the low-pass gate to
    ///
    inline void setFrequency(float freq) { rate = get_pitch(freq); }

    /// @brief Run the Gaussian filter for the given input sample.
    ///
    /// @param input the 16-bit PCM sample to pass through the Gaussian filter
    /// @returns the output from the Gaussian filter system for given input
    ///
    int16_t run(int16_t input) {
        // -------------------------------------------------------------------
        // MARK: Filter
        // -------------------------------------------------------------------
        // VoiceState& voice = voice_state;
        // cast the input to 32-bit to do maths
        int delta = input;
        // One, two and three point IIR filters
        int smp1 = samples[0];
        int smp2 = samples[1];
        switch (filter) {
        case 0:  // !filter1 !filter2
            break;
        case 1:  // !filter1 filter2
            delta += smp1 >> 1;
            delta += (-smp1) >> 5;
            break;
        case 2:  // filter1 !filter2
            delta += smp1;
            delta -= smp2 >> 1;
            delta += (-smp1 - (smp1 >> 1)) >> 5;
            delta += smp2 >> 5;
            break;
        case 3:  // filter1 filter2
            delta += smp1;
            delta -= smp2 >> 1;
            delta += (-smp1 * 13) >> 7;
            delta += (smp2 + (smp2 >> 1)) >> 4;
            break;
        }
        // update sample history
        samples[3] = samples[2];
        samples[2] = smp2;
        samples[1] = smp1;
        samples[0] = 2 * clamp_16(delta);
        // -------------------------------------------------------------------
        // MARK: Interpolation
        // -------------------------------------------------------------------
        // Gaussian interpolation using most recent 4 samples. update the
        // fractional increment based on the 14-bit frequency rate of the voice
        const int index = fraction >> 2 & 0x3FC;
        fraction = (fraction & 0x0FFF) + rate;
        // lookup the interpolation values in the table based on the index and
        // inverted index value
        const auto table1 = getGaussian(index);
        const auto table2 = getGaussian(255 * 4 - index);
        // apply the Gaussian interpolation to the incoming sample
        int sample = ((table1[0] * samples[3]) >> 12) +
                     ((table1[1] * samples[2]) >> 12) +
                     ((table2[1] * samples[1]) >> 12);
        sample = static_cast<int16_t>(2 * sample);
        sample +=    ((table2[0] * samples[0]) >> 11) & ~1;
        // apply the volume/amplitude level
        sample = (sample  * volume) >> 7;
        // return the sample clipped to 16-bit PCM
        return clamp_16(sample);
    }
};

#endif  // DSP_SONY_S_DSP_GAUSSIAN_HPP_
