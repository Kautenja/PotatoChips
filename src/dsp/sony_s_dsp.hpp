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

#ifndef DSP_SONY_S_DSP_HPP_
#define DSP_SONY_S_DSP_HPP_

#include "exceptions.hpp"
#include <cassert>

/// @brief Sony S-DSP chip emulator.
class Sony_S_DSP {
 public:
    /// the number of oscillators on the chip
    static constexpr unsigned VOICE_COUNT = 8;
    /// the number of RAM registers on the chip
    static constexpr unsigned NUM_REGISTERS = 128;

    /// the global registers on the S-DSP.
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
        /// (OFFSET_SOURCE_DIRECTORY * 100h = memory offset)
        OFFSET_SOURCE_DIRECTORY =  0x5D,
        /// DSP flags for MUTE, ECHO, RESET, NOISE CLOCK
        FLAGS =                    0x6C,
        /// Echo buffer start offset
        /// (ECHO_BUFFER_START_OFFSET * 100h = memory offset)
        ECHO_BUFFER_START_OFFSET = 0x6D,
        /// ENDX - 1 bit for each voice.
        ENDX =                     0x7C,
        /// Echo delay, 4-bits, higher values require more memory.
        ECHO_DELAY =               0x7D
    };

    /// the channel registers on the S-DSP. To get the register for channel
    /// `n`, perform the logical OR of the register address with `0xn0`.
    enum ChannelRegister : uint8_t {
        /// Left channel volume (8-bit signed value).
        VOLUME_LEFT      = 0x00,
        /// Right channel volume (8-bit signed value).
        VOLUME_RIGHT     = 0x01,
        /// Lower 8 bits of pitch.
        PITCH_LOW        = 0x02,
        /// Higher 8-bits of pitch.
        PITCH_HIGH       = 0x03,
        /// Source number (0-255). (references the source directory)
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
        // FIR_COEFFICIENTS = 0x0F
    };

    /// Returns the 14-bit pitch based on th given frequency.
    ///
    /// @param frequency the frequency in Hz
    /// @returns the 14-bit pitch corresponding to the S-DSP 32kHz sample rate
    /// @details
    ///
    /// $frequency = 32000 * (pitch / 2^12)$
    ///
    static inline uint16_t convert_pitch(float frequency) {
        const auto pitch = static_cast<float>(1 << 12) * frequency / 32000.f;
        return 0x3FFF & static_cast<uint16_t>(pitch);
    }

 private:
    /// a single synthesizer voice (channel) on the chip.
    struct RawVoice {
        /// the volume of the left channel
        int8_t left_vol;
        /// the volume of the right channel
        int8_t right_vol;
        /// the rate of the oscillator
        uint8_t rate[2];
        /// the oscillator's waveform sample
        uint8_t waveform;
        /// envelope rates for attack, decay, and sustain
        uint8_t adsr[2];
        /// envelope gain (if not using ADSR)
        uint8_t gain;
        /// current envelope level
        int8_t envx;
        /// current sample
        int8_t outx;
        /// filler bytes to align to 16-bytes
        int8_t unused[6];
    };

    /// global data structure used by the chip
    struct GlobalData {
        /// padding
        int8_t unused1[12];
        /// 0C   Main Volume Left (-.7)
        int8_t left_volume;
        /// 0D   Echo Feedback (-.7)
        int8_t echo_feedback;
        /// padding
        int8_t unused2[14];
        /// 1C   Main Volume Right (-.7)
        int8_t right_volume;
        /// padding
        int8_t unused3[15];
        /// 2C   Echo Volume Left (-.7)
        int8_t left_echo_volume;
        /// 2D   Pitch Modulation on/off for each voice
        uint8_t pitch_mods;
        /// padding
        int8_t unused4[14];
        /// 3C   Echo Volume Right (-.7)
        int8_t right_echo_volume;
        /// 3D   Noise output on/off for each voice
        uint8_t noise_enables;
        /// padding
        int8_t unused5[14];
        /// 4C   Key On for each voice
        uint8_t key_ons;
        /// 4D   Echo on/off for each voice
        uint8_t echo_ons;
        /// padding
        int8_t unused6[14];
        /// 5C   key off for each voice (instantiates release mode)
        uint8_t key_offs;
        /// 5D   source directory (wave table offsets)
        uint8_t wave_page;
        /// padding
        int8_t unused7[14];
        /// 6C   flags and noise freq
        uint8_t flags;
        /// 6D
        uint8_t echo_page;
        /// padding
        int8_t unused8[14];
        /// 7C
        uint8_t wave_ended;
        /// 7D   ms >> 4
        uint8_t echo_delay;
        /// padding
        char unused9[2];
    };

    union {
        /// the voices on the chip
        RawVoice voice[VOICE_COUNT];
        /// the register bank on the chip
        uint8_t reg[NUM_REGISTERS];
        /// global data on the chip
        GlobalData g;
    };

    /// a pointer to the shared RAM bank
    uint8_t* const ram;

    /// Cache of echo FIR values for faster access
    short fir_coeff[VOICE_COUNT];

    /// fir_buf[i + 8] == fir_buf[i], to avoid wrap checking in FIR code
    short fir_buf[16][2];
    /// (0 to 7)
    int fir_offset;

    /// TODO
    static constexpr int EMU_GAIN_BITS = 8;
    /// TODO
    int emu_gain;

    /// TODO
    int keys;

    /// TODO
    int echo_ptr;
    /// TODO
    int noise_amp;
    /// TODO
    int noise;
    /// TODO
    int noise_count;

    /// TODO
    int surround_threshold;

    /// TODO
    static int16_t const gauss[];

    /// The states of the ADSR envelope generator.
    enum EnvelopeState : short {
        /// the attack stage of the envelope generator
        state_attack,
        /// the decay stage of the envelope generator
        state_decay,
        /// the sustain stage of the envelope generator
        state_sustain,
        /// the release stage of the envelope generator
        state_release
    };

    /// The state of a synthesizer voice (channel) on the chip.
    struct VoiceState {
        /// TODO
        short volume[2];
        /// 12-bit fractional position
        short fraction;
        /// most recent four decoded samples
        short interp3;
        /// TODO
        short interp2;
        /// TODO
        short interp1;
        /// TODO
        short interp0;
        /// number of nibbles remaining in current block
        short block_remain;
        /// TODO
        unsigned short addr;
        /// header byte from current block
        short block_header;
        /// TODO
        short envcnt;
        /// TODO
        short envx;
        /// TODO
        short on_cnt;
        /// 7 if enabled, 31 if disabled
        short enabled;
        /// TODO
        short envstate;  // TODO: change type to EnvelopeState
        /// pad to power of 2
        short unused;
    };

    /// The states of the voices on the chip.
    VoiceState voice_state[VOICE_COUNT];

    /// @brief Process the envelope for the voice with given index.
    ///
    /// @param voice_index the index of the voice to clock the envelope of
    ///
    int clock_envelope(unsigned voice_idx);

 public:
    /// @brief Initialize a new Sony_S_DSP.
    ///
    /// @param ram a pointer to the 64KB shared RAM
    ///
    explicit Sony_S_DSP(uint8_t* ram);

    /// @brief Mute voice `n` if bit $n$ ($1 << n$) of mask is clear.
    ///
    /// @param mask the bit-mask representing the activation of voices
    ///
    void mute_voices(uint8_t mask);

    /// @brief Clear state and silence everything.
    void reset();

    /// @brief Set gain, where 1.0 is normal. When greater than 1.0, output is
    /// clamped to the 16-bit sample range.
    ///
    /// @param value the value to set the gain to
    ///
    inline void set_gain(double value) {
        emu_gain = static_cast<int>(value * (1 << EMU_GAIN_BITS));
    }

    /// @brief If true, prevent channels and global volumes from being phase-negated.
    ///
    /// @param disable true to disable surround, false to enable surround
    ///
    inline void disable_surround(bool disable) {
        surround_threshold = disable ? 0 : -0x7FFF;
    }

    /// @brief Read data from the register at the given address.
    ///
    /// @param address the address of the register to read data from
    ///
    inline int read(int address) {
        if (address >= NUM_REGISTERS)  // make sure the given address is legal
            throw AddressSpaceException<uint16_t>(address, 0, NUM_REGISTERS);
        return reg[address];
    }

    /// @brief Write data to the registers at the given address.
    ///
    /// @param address the address of the register to write data to
    /// @param data the data to write to the register
    ///
    void write(unsigned address, int data);

    /// @brief Run DSP for 'count' samples. Write resulting samples to 'buf'
    /// if not NULL.
    ///
    /// @param num_samples the number of samples to run on the DSP
    /// @param buffer the output buffer to write samples to
    ///
    /// @details
    /// the sample rate of the system is locked to 32kHz just like the SNES
    ///
    /// TODO: make clock_envelope() inline so that this becomes a leaf function?
    ///
    void run(long num_samples, short* buffer = NULL);
};

#endif  // DSP_SONY_S_DSP_HPP_
