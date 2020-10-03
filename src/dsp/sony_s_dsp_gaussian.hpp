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

#include <algorithm>
#include <cstdint>
#include <limits>

/// Clamp an integer to a 16-bit value.
///
/// @param n a 32-bit integer value to clip
/// @returns n clipped to a 16-bit value [-32768, 32767]
///
inline int clamp_16(int n) {
    const int lower = std::numeric_limits<int16_t>::min();
    const int upper = std::numeric_limits<int16_t>::max();
    return std::max(lower, std::min(n, upper));
}

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
        /// a history of the four most recent samples
        int16_t samples[4];
        /// 12-bit fractional position in the Gaussian table
        int16_t fraction = 0x3FFF;
        /// the volume level after the Gaussian filter
        int16_t volume = 0;
        /// the 14-bit frequency value
        uint16_t rate = 0;
        /// the discrete filter mode (i.e., the set of coefficients to use)
        uint8_t filter = 0;
    } voice_state;

 public:
    /// @brief Initialize a new Sony_S_DSP_Gaussian.
    Sony_S_DSP_Gaussian() : voice_state() { }

    /// Set the filter coefficients to a discrete mode.
    ///
    /// @param filter the new mode for the filter
    ///
    inline void setFilter(uint8_t filter) { voice_state.filter = filter & 0x3; }

    /// Set the volume level of the low-pass gate to a new value.
    ///
    /// @param volume the volume level after the Gaussian low-pass filter
    ///
    inline void setVolume(int8_t volume) { voice_state.volume = volume; }

    /// Set the frequency of the low-pass gate to a new value.
    ///
    /// @param freq the frequency to set the low-pass gate to
    ///
    inline void setFrequency(float freq) {
        // calculate the pitch based on the known relationship to frequency
        const auto pitch = static_cast<float>(1 << 12) * freq / SAMPLE_RATE;
        voice_state.rate = 0x3FFF & static_cast<uint16_t>(pitch);
    }

    /// @brief Run the Gaussian filter for the given input sample.
    ///
    /// @param input the 16-bit PCM sample to pass through the Gaussian filter
    ///
    int16_t run(int16_t input) {
        // VoiceState& voice = voice_state;
        // cast the input to 32-bit to do maths
        int delta = input;
        // One, two and three point IIR filters
        int smp1 = voice_state.samples[0];
        int smp2 = voice_state.samples[1];
        switch (voice_state.filter) {
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
        voice_state.samples[3] = voice_state.samples[2];
        voice_state.samples[2] = smp2;
        voice_state.samples[1] = smp1;
        voice_state.samples[0] = 2 * clamp_16(delta);
        // Gaussian interpolation using most recent 4 samples. update the
        // fractional increment based on the 14-bit frequency rate of the voice
        const int index = voice_state.fraction >> 2 & 0x3FC;
        voice_state.fraction = (voice_state.fraction & 0x0FFF) + voice_state.rate;
        // get a pointer to the Gaussian interpolation table as bytes
        const auto gauss8 = reinterpret_cast<uint8_t const*>(gauss);
        // lookup the interpolation values in the table based on the index and
        // inverted index value
        const auto table1 = reinterpret_cast<int16_t const*>(gauss8 + index);
        const auto table2 = reinterpret_cast<int16_t const*>(gauss8 + (255 * 4 - index));
        // apply the Gaussian interpolation to the incoming sample
        int sample = ((table1[0] * voice_state.samples[3]) >> 12) +
                     ((table1[1] * voice_state.samples[2]) >> 12) +
                     ((table2[1] * voice_state.samples[1]) >> 12);
        sample = static_cast<int16_t>(2 * sample);
        sample +=    ((table2[0] * voice_state.samples[0]) >> 11) & ~1;
        sample  = (sample  * voice_state.volume) >> 7;
        // return the sample clipped to 16-bit PCM
        return clamp_16(sample);
    }
};

#endif  // DSP_SONY_S_DSP_GAUSSIAN_HPP_
