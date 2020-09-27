// Sony S-DSP emulator.
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
#include <iostream>

/// @brief Sony S-DSP chip emulator.
class Sony_S_DSP_Echo {
 public:
    /// the sample rate of the S-DSP in Hz
    static constexpr unsigned SAMPLE_RATE = 32000;
    /// the number of sampler voices on the chip
    static constexpr unsigned VOICE_COUNT = 8;
    /// the number of registers on the chip
    static constexpr unsigned NUM_REGISTERS = 128;
    /// the size of the RAM bank in bytes
    static constexpr unsigned SIZE_OF_RAM = 1 << 16;
    /// the number of FIR coefficients used by the chip's echo filter
    static constexpr unsigned FIR_COEFFICIENT_COUNT = 8;

    /// @brief the global registers on the S-DSP.
    enum GlobalRegister : uint8_t {
        /// The volume for the left channel of the main output
        MAIN_VOLUME_LEFT =         0x0C,
        /// Echo Feedback
        ECHO_FEEDBACK =            0x0D,
        /// The volume for the right channel of the main output
        MAIN_VOLUME_RIGHT =        0x1C,
        /// The volume for the left channel of the echo effect
        ECHO_VOLUME_LEFT =         0x2C,
        /// pitch modulation
        PITCH_MODULATION =         0x2D,
        /// The volume for the right channel of the echo effect
        ECHO_VOLUME_RIGHT =        0x3C,
        /// Noise enable
        NOISE_ENABLE =             0x3D,
        /// Key-on (1 bit for each voice)
        KEY_ON =                   0x4C,
        /// Echo enable
        ECHO_ENABLE =              0x4D,
        /// Key-off (1 bit for each voice)
        KEY_OFF =                  0x5C,
        /// Offset of source directory
        /// (`OFFSET_SOURCE_DIRECTORY * 0x100` = memory offset)
        OFFSET_SOURCE_DIRECTORY =  0x5D,
        /// DSP flags for RESET, MUTE, ECHO, NOISE PERIOD
        FLAGS =                    0x6C,
        /// Echo buffer start offset
        /// (`ECHO_BUFFER_START_OFFSET * 0x100` = memory offset)
        ECHO_BUFFER_START_OFFSET = 0x6D,
        /// ENDX - 1 bit for each voice.
        ENDX =                     0x7C,
        /// Echo delay, 4-bits, higher values require more memory.
        ECHO_DELAY =               0x7D
    };

    /// @brief the channel registers on the S-DSP. To get the register for
    /// channel `n`, perform the logical OR of the register address with `0xn0`.
    enum ChannelRegister : uint8_t {
        /// Left channel volume (8-bit signed value).
        VOLUME_LEFT      = 0x00,
        /// Right channel volume (8-bit signed value).
        VOLUME_RIGHT     = 0x01,
        /// Lower 8 bits of pitch.
        PITCH_LOW        = 0x02,
        /// Higher 8-bits of pitch.
        PITCH_HIGH       = 0x03,
        /// Source number (\f$\in [0, 255]\f$). (references the source directory)
        SOURCE_NUMBER    = 0x04,
        /// If bit-7 is set, ADSR is enabled. If cleared GAIN is used.
        ADSR_1           = 0x05,
        /// These two registers control the ADSR envelope.
        ADSR_2           = 0x06,
        /// This register provides function for software envelopes.
        GAIN             = 0x07,
        /// The DSP writes the current value of the envelope to this register.
        ENVELOPE_OUT     = 0x08,
        /// The DSP writes the current waveform value after envelope
        /// multiplication and before volume multiplication.
        WAVEFORM_OUT     = 0x09,
        /// 8-tap FIR Filter coefficients
        FIR_COEFFICIENTS = 0x0F
    };

    /// @brief A stereo sample in the echo buffer.
    struct BufferSample {
        /// the index of the left channel in the samples array
        static constexpr unsigned LEFT = 0;
        /// the index of the right channel in the samples array
        static constexpr unsigned RIGHT = 1;
        /// the 16-bit sample for the left [0] and right [1] channels.
        int16_t samples[2];
    };

 private:

    /// A structure mapping the register space to symbolic global data fields.
    struct GlobalData {
        /// padding
        int8_t unused1[12];
        /// 0C Main Volume Left (8-bit signed value)
        int8_t left_volume;
        /// 0D   Echo Feedback (8-bit signed value)
        int8_t echo_feedback;
        /// padding
        int8_t unused2[14];
        /// 1C   Main Volume Right (8-bit signed value)
        int8_t right_volume;
        /// padding
        int8_t unused3[15];
        /// 2C   Echo Volume Left (8-bit signed value)
        int8_t left_echo_volume;
        /// 2D   Pitch Modulation on/off for each voice (bit-mask)
        uint8_t pitch_mods;
        /// padding
        int8_t unused4[14];
        /// 3C   Echo Volume Right (8-bit signed value)
        int8_t right_echo_volume;
        /// 3D   Noise output on/off for each voice (bit-mask)
        uint8_t noise_enables;
        /// padding
        int8_t unused5[14];
        /// 4C   Key On for each voice (bit-mask)
        uint8_t key_ons;
        /// 4D   Echo on/off for each voice (bit-mask)
        uint8_t echo_ons;
        /// padding
        int8_t unused6[14];
        /// 5C   key off for each voice (instantiates release mode) (bit-mask)
        uint8_t key_offs;
        /// 5D   source directory (wave table offsets)
        uint8_t wave_page;
        /// padding
        int8_t unused7[14];
        /// 6C   flags and noise freq (coded 8-bit value)
        uint8_t flags;
        /// 6D   the page of RAM to use for the echo buffer
        uint8_t echo_page;
        /// padding
        int8_t unused8[14];
        /// 7C   whether the sample has ended for each voice (bit-mask)
        uint8_t wave_ended;
        /// 7D   ms >> 4
        uint8_t echo_delay;
        /// padding
        char unused9[2];
    };

    /// Combine the raw voice, registers, and global data structures into a
    /// single piece of memory to allow easy symbolic access to register data
    union {
        /// the register bank on the chip
        uint8_t registers[NUM_REGISTERS];
        /// the mapping of register data to the global data on the chip
        GlobalData global;
    };

    /// The values of the FIR filter coefficients from the register bank. This
    /// allows the FIR coefficients to be stored as 16-bit
    short fir_coeff[FIR_COEFFICIENT_COUNT];

    /// the number of \f$16ms\f$ delay levels
    static constexpr unsigned DELAY_LEVELS = 15;
    /// the number of bytes per delay level, i.e., 2KB
    static constexpr unsigned DELAY_LEVEL_BYTES = 2 * (1 << 10);
    /// the RAM for the echo buffer. `2KB` for each \f$16ms\f$ delay level
    uint8_t ram[DELAY_LEVELS * DELAY_LEVEL_BYTES];

    /// A pointer to the head of the echo buffer in RAM
    unsigned buffer_head;

    /// fir_buf[i + 8] == fir_buf[i], to avoid wrap checking in FIR code
    short fir_buf[16][2];
    /// (0 to 7)
    int fir_offset;

    /// the delay level
    uint8_t delay;
    /// the feedback level
    int8_t feedback;
    /// the mix level for the left channel
    int8_t mixLeft;
    /// the mix level for the right channel
    int8_t mixRight;

 public:
    /// @brief Initialize a new Sony_S_DSP_Echo.
    Sony_S_DSP_Echo() { }

    /// @brief Clear state and silence everything.
    void reset() {
        buffer_head = fir_offset = 0;
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
