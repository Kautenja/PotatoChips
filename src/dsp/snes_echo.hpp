// An emulation of the echo effect from the Sony S-DSP.
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

#ifndef DSP_SONY_S_DSP_ECHO_HPP_
#define DSP_SONY_S_DSP_ECHO_HPP_

#include "exceptions.hpp"
#include <cstring>
#include <cassert>

/// @brief An emulation of the echo effect from the Sony S-DSP.
class Sony_S_DSP_Echo {
 public:
    /// the sample rate of the S-DSP in Hz
    static constexpr unsigned SAMPLE_RATE = 32000;
    /// the size of the RAM bank in bytes
    // static constexpr unsigned SIZE_OF_RAM = 1 << 16;
    /// the number of FIR coefficients used by the chip's echo filter
    static constexpr unsigned FIR_COEFFICIENT_COUNT = 8;

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
    /// the number of \f$16ms\f$ delay levels
    static constexpr unsigned DELAY_LEVELS = 15;
    /// the number of bytes per delay level, i.e., 2KB
    static constexpr unsigned DELAY_LEVEL_BYTES = 2 * (1 << 10);
    /// the RAM for the echo buffer. `2KB` for each \f$16ms\f$ delay level
    uint8_t ram[DELAY_LEVELS * DELAY_LEVEL_BYTES];
    /// A pointer to the head of the echo buffer in RAM
    unsigned buffer_head = 0;

    /// fir_buf[i + 8] == fir_buf[i], to avoid wrap checking in FIR code
    BufferSample fir_buf[16];
    /// (0 to 7)
    int fir_offset = 0;

    // -----------------------------------------------------------------------
    // MARK: Echo Parameters
    // -----------------------------------------------------------------------

    /// The values of the FIR filter coefficients from the register bank. This
    /// allows the FIR coefficients to be stored as 16-bit
    int16_t fir_coeff[FIR_COEFFICIENT_COUNT] = {127,0,0,0,0,0,0,0};

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
        memset(fir_buf, 0, sizeof fir_buf);
        memset(ram, 0, sizeof ram);
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
    inline void setFIR(unsigned index, int8_t value) { fir_coeff[index] = value; }

    /// @brief Run DSP for some samples and write them to the given buffer.
    ///
    /// @param output_buffer the output buffer to write samples to (optional)
    ///
    /// @details
    /// the sample rate of the system is locked to 32kHz just like the SNES
    ///
    void run(int left, int right, int16_t* output_buffer = NULL);
};

#endif  // DSP_SONY_S_DSP_ECHO_HPP_
