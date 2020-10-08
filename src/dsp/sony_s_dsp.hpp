// Sony S-DSP emulator.
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

#ifndef DSP_SONY_S_DSP_HPP_
#define DSP_SONY_S_DSP_HPP_

#include "exceptions.hpp"
#include <cstring>
#include <cassert>

/// @brief Sony S-DSP chip emulator.
class Sony_S_DSP {
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
    struct __attribute__((packed, aligned(4))) SourceDirectoryEntry {
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

    /// @brief A 9-byte bit-rate reduction (BRR) block. BRR has a 32:9
    /// compression ratio over 16-bit PCM, i.e., 32 bytes of PCM = 9 bytes of
    /// BRR samples.
    struct BitRateReductionBlock {
        enum : unsigned {
            /// the number of 1-byte samples in each block of BRR samples
            NUM_SAMPLES = 8
        };

        enum : uint8_t {
            /// the maximal volume level for a BRR sample block
            MAX_VOLUME = 0x0C
        };

        /// a structure containing the 8-bit header flag with schema:
        // +------+------+------+------+------+------+------+------+
        // | 7    | 6    | 5    | 4    | 3    | 2    | 1    | 0    |
        // +------+------+------+------+------+------+------+------+
        // | Volume (max 0xC0)         | Filter Mode | Loop | End  |
        // +------+------+------+------+------+------+------+------+
        struct Flags {
            /// the end of sample block flag
            uint8_t is_end : 1;
            /// the loop flag determining if this block loops
            uint8_t is_loop : 1;
            /// the filter mode for selecting 1 of 4 filter modes
            uint8_t filter : 2;
            /// the volume level in [0, 12]
            uint8_t volume : 4;

            /// Set the volume level to a new value.
            ///
            /// @param level the level to set the volume to
            /// @details
            /// set the volume to the new level in the range [0, 12]
            ///
            inline void set_volume(uint8_t level) {
                volume = std::min(level, static_cast<uint8_t>(MAX_VOLUME));
            }
        };

        union {
            /// the bit-wise flag representation of the header
            Flags flags;
            /// the encoded header byte
            uint8_t header = 0;
        } __attribute__((packed));
        /// the 8-byte block of sample data
        uint8_t samples[NUM_SAMPLES];
    } __attribute__((packed));

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

    /// @brief A stereo sample in the echo buffer.
    struct __attribute__((packed, aligned(4))) EchoBufferSample {
        enum : unsigned {
            /// the index of the left channel in the samples array
            LEFT = 0,
            /// the index of the right channel in the samples array
            RIGHT = 1,
            /// the number of channels in the sample
            CHANNELS = 2
        };

        /// the 16-bit sample for the left [0] and right [1] channels.
        int16_t samples[CHANNELS] = {0, 0};
    };

    /// @brief Returns the 14-bit pitch based on th given frequency.
    ///
    /// @param frequency the frequency in Hz
    /// @returns the 14-bit pitch corresponding to the S-DSP 32kHz sample rate
    /// @details
    ///
    /// \f$frequency = \f$SAMPLE_RATE\f$ * \frac{pitch}{2^{12}}\f$
    ///
    static inline uint16_t convert_pitch(float frequency) {
        // calculate the pitch based on the known relationship to frequency
        const auto pitch = static_cast<float>(1 << 12) * frequency / SAMPLE_RATE;
        // mask the 16-bit pitch to 14-bit
        return 0x3FFF & static_cast<uint16_t>(pitch);
    }

 private:
    /// The initial value of the envelope.
    static constexpr int ENVELOPE_RATE_INITIAL = 0x7800;

    /// the range of the envelope generator amplitude level (i.e., max value)
    static constexpr int ENVELOPE_RANGE = 0x0800;

    /// Return the envelope rate for the given index in the table.
    ///
    /// @param index the index of the envelope rate to return in the table
    /// @returns the envelope rate at given index in the table
    ///
    static inline uint16_t getEnvelopeRate(unsigned index) {
        // This table is for envelope timing.  It represents the number of
        // counts that should be subtracted from the counter each sample
        // period (32kHz). The counter starts at 30720 (0x7800). Each count
        // divides exactly into 0x7800 without remainder.
        static const uint16_t ENVELOPE_RATES[0x20] = {
            0x0000, 0x000F, 0x0014, 0x0018, 0x001E, 0x0028, 0x0030, 0x003C,
            0x0050, 0x0060, 0x0078, 0x00A0, 0x00C0, 0x00F0, 0x0140, 0x0180,
            0x01E0, 0x0280, 0x0300, 0x03C0, 0x0500, 0x0600, 0x0780, 0x0A00,
            0x0C00, 0x0F00, 0x1400, 0x1800, 0x1E00, 0x2800, 0x3C00, 0x7800
        };
        return ENVELOPE_RATES[index];
    }

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

    /// The values of the FIR filter coefficients from the register bank. This
    /// allows the FIR coefficients to be stored as 16-bit
    short fir_coeff[FIR_COEFFICIENT_COUNT];

    /// @brief A pointer to the shared 64KB RAM bank between the S-DSP and
    /// the SPC700.
    /// @details
    /// this must be maintained by the caller in order to provide data to the
    /// S-DSP. This includes input sample data, and the allocated space for the
    /// echo buffer according to the global ECHO_BUFFER_START_OFFSET register
    uint8_t* const ram;

    /// A bit-mask representation of the active voice gates
    int keys;

    /// The number of samples until the LFSR will sample a new value
    int noise_count;
    /// The discrete sampled LFSR register noise value
    int noise;
    /// The amplified LFSR register noise sample
    int noise_amp;

    /// A pointer to the head of the echo buffer in RAM
    int echo_ptr;

    /// fir_buf[i + 8] == fir_buf[i], to avoid wrap checking in FIR code
    short fir_buf[16][2];
    /// (0 to 7)
    int fir_offset;

    /// The values of the Gaussian filter on the DAC of the SNES
    static int16_t const gauss[];

    /// The stages of the ADSR envelope generator.
    enum class EnvelopeStage : short { Attack, Decay, Sustain, Release };

    /// The state of a synthesizer voice (channel) on the chip.
    struct VoiceState {
        /// the volume level of the voice
        short volume[2];
        /// 12-bit fractional position
        short fraction;
        /// padding (placement here keeps interp's in a 64-bit line)
        short unused0;
        /// most recent four decoded samples for the Gaussian filter
        int16_t interp[4];
        /// number of nibbles remaining in current block
        short block_remain;
        /// the current address of the sample being played by the voice
        unsigned short addr;
        /// header byte from current block
        short block_header;
        /// padding (placement here keeps envelope data in a 64-bit line)
        short unused1;
        /// the envelope generator sample counter
        short envcnt;
        /// the output value from the envelope generator
        short envx;
        /// the number of samples delay until the voice turns on (after key-on)
        short on_cnt;
        /// the current stage of the envelope generator
        EnvelopeStage envelope_stage;
    } voice_states[VOICE_COUNT];

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

    /// @brief Clear state and silence everything.
    void reset() {
        keys = echo_ptr = noise_count = fir_offset = 0;
        noise = 1;
        // reset, mute, echo off
        global.flags = FLAG_MASK_RESET | FLAG_MASK_MUTE | FLAG_MASK_ECHO_WRITE;
        global.key_ons = 0;
        // reset voices
        for (unsigned i = 0; i < VOICE_COUNT; i++) {
            VoiceState& v = voice_states[i];
            v.on_cnt = v.volume[0] = v.volume[1] = 0;
            v.envelope_stage = EnvelopeStage::Release;
        }
        // clear the echo buffer
        memset(fir_buf, 0, sizeof fir_buf);
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
            case FIR_COEFFICIENTS:  // update FIR coefficients
                // sign-extend
                fir_coeff[index] = (int8_t) data;
                break;
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

#endif  // DSP_SONY_S_DSP_HPP_
