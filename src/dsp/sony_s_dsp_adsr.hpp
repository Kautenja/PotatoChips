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

#ifndef DSP_SONY_S_DSP_ADSR_HPP_
#define DSP_SONY_S_DSP_ADSR_HPP_

#include <cstdint>

/// @brief Sony S-DSP chip emulator.
class Sony_S_DSP_ADSR {
 public:
    /// the sample rate of the S-DSP in Hz
    static constexpr unsigned SAMPLE_RATE = 32000;
    /// the number of sampler voices on the chip TODO: remove
    static constexpr unsigned VOICE_COUNT = 8;
    /// the number of registers on the chip TODO: remove
    static constexpr unsigned NUM_REGISTERS = 128;

    // TODO: remove
    /// @brief the global registers on the S-DSP.
    enum GlobalRegister : uint8_t {
        /// Key-on (1 bit for each voice)
        KEY_ON =  0x4C,
        /// Key-off (1 bit for each voice)
        KEY_OFF = 0x5C,
    };

    // TODO: remove
    /// @brief the channel registers on the S-DSP. To get the register for
    /// channel `n`, perform the logical OR of the register address with `0xn0`.
    enum ChannelRegister : uint8_t {
        /// If bit-7 is set, ADSR is enabled. If cleared GAIN is used.
        ADSR_1       = 0x05,
        /// These two registers control the ADSR envelope.
        ADSR_2       = 0x06,
        /// The DSP writes the current value of the envelope to this register.
        ENVELOPE_OUT = 0x08,
    };

 private:
    // Byte 1
    /// the attack rate (4-bits)
    uint8_t attack : 4;
    /// the decay rate (3-bits)
    uint8_t decay : 3;
    /// a dummy bit for byte alignment
    const uint8_t unused_spacer_for_byte_alignment : 1;

    // Byte 2
    /// the sustain rate (5-bits)
    uint8_t sustain_rate : 5;
    /// the sustain level (3-bits)
    uint8_t sustain_level : 3;

    // Byte 3
    /// the total amplitude level of the envelope generator (8-bit)
    int8_t amplitude;



    // TODO: remove
    /// A structure mapping the register space to a single voice's data fields.
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

    // TODO: remove
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
        uint8_t key_ons = 0;
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

    // TODO: remove
    /// Combine the raw voice, registers, and global data structures into a
    /// single piece of memory to allow easy symbolic access to register data
    union {
        /// the register bank on the chip
        uint8_t registers[NUM_REGISTERS];
        /// the mapping of register data to the voices on the chip
        RawVoice voices[VOICE_COUNT];
        /// the mapping of register data to the global data on the chip
        GlobalData global;
    };

    /// A bit-mask representation of the active voice gates
    int keys = 0;

    /// The stages of the ADSR envelope generator.
    enum class EnvelopeStage : short { Attack, Decay, Sustain, Release };

    // TODO: remove
    /// The state of a synthesizer voice (channel) on the chip.
    struct VoiceState {
        /// TODO
        short volume[2] = {0, 0};
        /// 12-bit fractional position
        short fraction;
        /// padding (placement here keeps interp's in a 64-bit line)
        short unused0;
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
        /// padding (placement here keeps envelope data in a 64-bit line)
        short unused1;
        /// TODO
        short envcnt = 0;
        /// TODO
        short envx = 0;
        /// TODO
        short on_cnt = 0;
        /// the current stage of the envelope generator
        EnvelopeStage envelope_stage = EnvelopeStage::Release;
    } voice_states[VOICE_COUNT];

    /// @brief Process the envelope for the voice with given index.
    ///
    /// @param voice_index the index of the voice to clock the envelope of
    ///
    int clock_envelope();

 public:
    /// @brief Initialize a new Sony_S_DSP_ADSR.
    Sony_S_DSP_ADSR() : unused_spacer_for_byte_alignment(1) {
        attack = 0;
        decay = 0;
        sustain_rate = 0;
        sustain_level = 0;
        amplitude = 0;
    }

    /// @brief Set the attack parameter to a new value.
    ///
    /// @param value the attack rate to use.
    ///
    inline void setAttack(uint8_t value) { attack = value; }

    /// @brief Set the decay parameter to a new value.
    ///
    /// @param value the decay rate to use.
    ///
    inline void setDecay(uint8_t value) { decay = value; }

    /// @brief Set the sustain level parameter to a new value.
    ///
    /// @param value the sustain level to use.
    ///
    inline void setSustainLevel(uint8_t value) { sustain_level = value; }

    /// @brief Set the sustain rate parameter to a new value.
    ///
    /// @param value the sustain rate to use.
    ///
    inline void setSustainRate(uint8_t value) { sustain_rate = value; }

    /// @brief Set the amplitude parameter to a new value.
    ///
    /// @param value the amplitude level to use.
    ///
    inline void setAmplitude(int8_t value) { amplitude = value; }

    /// @brief Read data from the register at the given address.
    ///
    /// @param address the address of the register to read data from
    ///
    inline uint8_t read(uint8_t address) { return registers[address]; }

    /// @brief Write data to the registers at the given address.
    ///
    /// @param address the address of the register to write data to
    /// @param data the data to write to the register
    ///
    inline void write(uint8_t address, uint8_t data) { registers[address] = data; }

    /// @brief Run DSP for some samples and write them to the given buffer.
    void run();
};

#endif  // DSP_SONY_S_DSP_ADSR_HPP_
