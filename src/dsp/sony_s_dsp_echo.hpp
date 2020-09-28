// An emulation of the echo effect from the Sony S-DSP.
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

#ifndef DSP_SONY_S_DSP_ECHO_HPP_
#define DSP_SONY_S_DSP_ECHO_HPP_

#include <algorithm>
#include <cstdint>
#include <cstring>
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

/// @brief An emulation of the echo effect from the Sony S-DSP.
class Sony_S_DSP_Echo {
 public:
    /// the size of the RAM bank in bytes
    // static constexpr unsigned SIZE_OF_RAM = 1 << 16;
    /// the sample rate of the S-DSP in Hz
    static constexpr unsigned SAMPLE_RATE = 32000;
    /// the number of FIR coefficients used by the chip's echo filter
    static constexpr unsigned FIR_COEFFICIENT_COUNT = 8;
    /// the number of milliseconds per discrete delay level
    static constexpr unsigned MILLISECONDS_PER_DELAY_LEVEL = 16;
    /// the number of \f$16ms\f$ delay levels
    static constexpr unsigned DELAY_LEVELS = 31;
    /// the number of bytes per delay level (2KB)
    static constexpr unsigned DELAY_LEVEL_BYTES = 2 * (1 << 10);

    /// @brief A stereo sample in the echo buffer.
    struct BufferSample {
        /// the index of the left channel in the samples array
        static constexpr unsigned LEFT = 0;
        /// the index of the right channel in the samples array
        static constexpr unsigned RIGHT = 1;
        /// the 16-bit sample for the left [0] and right [1] channels.
        int16_t samples[2] = {0, 0};
    };

 private:
    // Echo Internal Buffers

    /// the RAM for the echo buffer. `2KB` for each \f$16ms\f$ delay level
    /// multiplied by the total number of delay levels
    uint8_t ram[DELAY_LEVELS * DELAY_LEVEL_BYTES];
    /// A pointer to the head of the echo buffer in RAM
    unsigned buffer_head = 0;

    /// fir_buffer[i + 8] == fir_buffer[i], to avoid wrap checking in FIR code
    BufferSample fir_buffer[2 * FIR_COEFFICIENT_COUNT];
    /// the size of the FIR ring buffer
    static constexpr int FIR_MAX_INDEX = 7;
    /// the head index of the FIR ring buffer (0 to 7)
    int fir_offset = 0;

    // Echo Parameters

    /// The values of the FIR filter coefficients from the register bank. This
    /// allows the FIR coefficients to be stored as 16-bit
    int16_t fir_coeff[FIR_COEFFICIENT_COUNT] = {127, 0, 0, 0, 0, 0, 0, 0};
    /// the delay level
    uint8_t delay = 0;
    /// the feedback level
    int8_t feedback = 0;
    /// the mix level for the left channel
    int8_t mixLeft = 0;
    /// the mix level for the right channel
    int8_t mixRight = 0;

 public:
    /// @brief Initialize a new Sony_S_DSP_Echo.
    Sony_S_DSP_Echo() { reset(); }

    /// @brief Clear state and silence everything.
    void reset() {
        buffer_head = fir_offset = delay = feedback = mixLeft = mixRight = 0;
        memset(ram, 0, sizeof ram);
        memset(fir_buffer, 0, sizeof fir_buffer);
    }

    /// @brief Set the delay parameter to a new value
    ///
    /// @param value the delay level to use. the delay in time is
    /// \f$16 * \f$`value`\f$ ms\f$
    ///
    inline void setDelay(uint8_t value) { delay = value & DELAY_LEVELS; }

    /// @brief Set the feedback to a new level.
    ///
    /// @param value the level to set the feedback to
    ///
    inline void setFeedback(int8_t value) { feedback = value; }

    /// @brief Set the mix to new level for the left channel.
    ///
    /// @param value the level to set the left channel to
    ///
    inline void setMixLeft(int8_t value) { mixLeft = value; }

    /// @brief Set the mix to new level for the right channel.
    ///
    /// @param value the level to set the right channel to
    ///
    inline void setMixRight(int8_t value) { mixRight = value; }

    /// @brief Set FIR coefficient at given index to a new value.
    ///
    /// @param index the index of the FIR coefficient to set
    /// @param value the value to set the FIR coefficient to
    ///
    inline void setFIR(unsigned index, int8_t value) {
        fir_coeff[index] = value;
    }

    /// @brief Return the FIR coefficient at given index.
    ///
    /// @param index the index of the FIR coefficient to get
    ///
    inline uint8_t getFIR(unsigned index) const { return fir_coeff[index]; }

    /// @brief Run echo effect on input samples and write to the output buffer
    ///
    /// @param left the sample of the left channel
    /// @param right the sample of the right channel
    ///
    BufferSample run(int left, int right) {
        // get the current feedback sample in the echo buffer
        auto const echo = reinterpret_cast<BufferSample*>(&ram[buffer_head]);
        // increment the echo pointer by the size of the echo buffer sample
        buffer_head += sizeof(BufferSample);
        // check if for the end of the ring buffer and wrap the pointer around
        if (buffer_head >= (delay & DELAY_LEVELS) * DELAY_LEVEL_BYTES)
            buffer_head = 0;
        // cache the feedback value (sign-extended to 32-bit)
        int feedback_left = echo->samples[BufferSample::LEFT];
        int feedback_right = echo->samples[BufferSample::RIGHT];

        // put samples in history ring buffer
        auto const fir_samples = &fir_buffer[fir_offset];
        // move backwards one step
        fir_offset = (fir_offset + FIR_MAX_INDEX) & FIR_MAX_INDEX;
        // put sample into the first sample in the buffer
        fir_samples[0].samples[BufferSample::LEFT]  = feedback_left;
        fir_samples[0].samples[BufferSample::RIGHT] = feedback_right;
        // duplicate at +8 eliminates wrap checking below
        fir_samples[8].samples[BufferSample::LEFT]  = feedback_left;
        fir_samples[8].samples[BufferSample::RIGHT] = feedback_right;

        // FIR left channel
        feedback_left =                  feedback_left * fir_coeff[7] +
            fir_samples[1].samples[BufferSample::LEFT] * fir_coeff[6] +
            fir_samples[2].samples[BufferSample::LEFT] * fir_coeff[5] +
            fir_samples[3].samples[BufferSample::LEFT] * fir_coeff[4] +
            fir_samples[4].samples[BufferSample::LEFT] * fir_coeff[3] +
            fir_samples[5].samples[BufferSample::LEFT] * fir_coeff[2] +
            fir_samples[6].samples[BufferSample::LEFT] * fir_coeff[1] +
            fir_samples[7].samples[BufferSample::LEFT] * fir_coeff[0];
        // FIR right channel
        feedback_right =                 feedback_right * fir_coeff[7] +
            fir_samples[1].samples[BufferSample::RIGHT] * fir_coeff[6] +
            fir_samples[2].samples[BufferSample::RIGHT] * fir_coeff[5] +
            fir_samples[3].samples[BufferSample::RIGHT] * fir_coeff[4] +
            fir_samples[4].samples[BufferSample::RIGHT] * fir_coeff[3] +
            fir_samples[5].samples[BufferSample::RIGHT] * fir_coeff[2] +
            fir_samples[6].samples[BufferSample::RIGHT] * fir_coeff[1] +
            fir_samples[7].samples[BufferSample::RIGHT] * fir_coeff[0];

        // put the echo samples into the buffer
        echo->samples[BufferSample::LEFT] =
            clamp_16(left + ((feedback_left  * feedback) >> 14));
        echo->samples[BufferSample::RIGHT] =
            clamp_16(right + ((feedback_right * feedback) >> 14));

        // (1) add the echo to the samples for the left and right channel,
        // (2) clamp the left and right samples, and (3) place them into
        // the buffer
        Sony_S_DSP_Echo::BufferSample output_buffer;
        output_buffer.samples[BufferSample::LEFT] =
            clamp_16(left + ((feedback_left  * mixLeft) >> 14));
        output_buffer.samples[BufferSample::RIGHT] =
            clamp_16(right + ((feedback_right * mixRight) >> 14));

        return output_buffer;
    }
};

#endif  // DSP_SONY_S_DSP_ECHO_HPP_
