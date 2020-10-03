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

#include "exceptions.hpp"
#include <cstring>
#include <cassert>

/// @brief Sony S-DSP chip emulator.
class Sony_S_DSP_ADSR {
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

    /// @brief An entry in the source directory in the 64KB RAM.
    struct SourceDirectoryEntry {
        /// @brief the start address of the sample in the directory.
        /// @details
        /// In hardware this is represented across two bytes; in software we
        /// will skip to the 16-bit representation of the RAM address.
        uint16_t start;
        /// @brief the loop address of the sample in the directory.
        /// @details
        /// In hardware this is represented across two bytes; in software we
        /// will skip to the 16-bit representation of the RAM address.
        uint16_t loop;
    };

    /// @brief Bit-masks for extracting values from the flags registers.
    enum FlagMasks {
        /// a mask for the flag register to extract the noise period parameter
        FLAG_MASK_NOISE_PERIOD = 0x1F,
        /// a mask for the flag register to extract the echo write enabled bit
        FLAG_MASK_ECHO_WRITE = 0x20,
        /// a mask for the flag register to extract the mute voices bit
        FLAG_MASK_MUTE = 0x40,
        /// a mask for the flag register to extract the reset chip bit
        FLAG_MASK_RESET = 0x80
    };

 private:
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
        /// the mapping of register data to the voices on the chip
        RawVoice voices[VOICE_COUNT];
        /// the mapping of register data to the global data on the chip
        GlobalData global;
    };

    /// @brief A pointer to the shared 64KB RAM bank between the S-DSP and
    /// the SPC700.
    /// @details
    /// this must be maintained by the caller in order to provide data to the
    /// S-DSP. This includes input sample data, and the allocated space for the
    /// echo buffer according to the global ECHO_BUFFER_START_OFFSET register
    uint8_t* const ram;

    /// A bit-mask representation of the active voice gates
    int keys;

    /// The stages of the ADSR envelope generator.
    enum class EnvelopeStage : short { Attack, Decay, Sustain, Release };

    /// The state of a synthesizer voice (channel) on the chip.
    struct VoiceState {
        /// TODO
        short volume[2];
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
        short envcnt;
        /// TODO
        short envx;
        /// TODO
        short on_cnt;
        /// the current stage of the envelope generator
        EnvelopeStage envelope_stage;
    } voice_states[VOICE_COUNT];

    /// TODO: make inline so that `run` becomes a leaf function?
    /// @brief Process the envelope for the voice with given index.
    ///
    /// @param voice_index the index of the voice to clock the envelope of
    ///
    int clock_envelope(unsigned voice_idx);

 public:
    /// @brief Initialize a new Sony_S_DSP_ADSR.
    ///
    /// @param ram a pointer to the 64KB shared RAM
    ///
    explicit Sony_S_DSP_ADSR(uint8_t* ram_) : ram(ram_) { }

    /// @brief Clear state and silence everything.
    void reset() {
        keys = 0;
        // reset, mute, echo off
        global.flags = FLAG_MASK_RESET | FLAG_MASK_MUTE | FLAG_MASK_ECHO_WRITE;
        global.key_ons = 0;
        // reset voices
        for (unsigned i = 0; i < VOICE_COUNT; i++) {
            VoiceState& v = voice_states[i];
            v.on_cnt = v.volume[0] = v.volume[1] = 0;
            v.envelope_stage = EnvelopeStage::Release;
        }
    }

    /// @brief Read data from the register at the given address.
    ///
    /// @param address the address of the register to read data from
    ///
    inline uint8_t read(uint8_t address) {
        if (address >= NUM_REGISTERS)  // make sure the given address is valid
            throw AddressSpaceException<uint8_t>(address, 0, NUM_REGISTERS);
        return registers[address];
    }

    /// @brief Write data to the registers at the given address.
    ///
    /// @param address the address of the register to write data to
    /// @param data the data to write to the register
    ///
    void write(uint8_t address, uint8_t data) {
        if (address >= NUM_REGISTERS)  // make sure the given address is valid
            throw AddressSpaceException<uint8_t>(address, 0, NUM_REGISTERS);
        // store the data in the register with given address
        registers[address] = data;
        // get the high 4 bits for indexing the voice / FIR coefficients
        int index = address >> 4;
        // update volume / FIR coefficients
        switch (address & FIR_COEFFICIENTS) {
            // voice volume
            case 0:    // left channel, fall through to next block
            case 1: {  // right channel, process both left and right channels
                short* volume = voice_states[index].volume;
                int left  = (int8_t) registers[address & ~1];
                int right = (int8_t) registers[address |  1];
                volume[0] = left;
                volume[1] = right;
                break;
            }
        }
    }

    /// @brief Run DSP for some samples and write them to the given buffer.
    ///
    /// @param output_buffer the output buffer to write samples to (optional)
    ///
    /// @details
    /// the sample rate of the system is locked to 32kHz just like the SNES
    ///
    void run(int16_t* output_buffer = NULL);
};

#endif  // DSP_SONY_S_DSP_ADSR_HPP_
