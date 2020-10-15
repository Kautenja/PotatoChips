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
#include "sony_s_dsp_common.hpp"
#include <cstring>

/// @brief Sony S-DSP chip emulator.
class Sony_S_DSP_BRR {
 public:
    enum : unsigned {
        /// the number of sampler voices on the chip
        VOICE_COUNT = 8,
        /// the number of registers on the chip
        NUM_REGISTERS = 128,
        /// the size of the RAM bank in bytes
        SIZE_OF_RAM = 1 << 16
    };

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
    struct __attribute__((packed)) BitRateReductionBlock {
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

        union __attribute__((packed)) {
            /// the bit-wise flag representation of the header
            Flags flags;
            /// the encoded header byte
            uint8_t header = 0;
        };
        /// the 8-byte block of sample data
        uint8_t samples[NUM_SAMPLES];
    };

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
        int8_t unused18;
        /// 0D   Echo Feedback (8-bit signed value)
        int8_t unused17;
        /// padding
        int8_t unused2[14];
        /// 1C   Main Volume Right (8-bit signed value)
        int8_t unused16;
        /// padding
        int8_t unused3[15];
        /// 2C   Echo Volume Left (8-bit signed value)
        int8_t unused15;
        /// 2D   Pitch Modulation on/off for each voice (bit-mask)
        uint8_t pitch_mods;
        /// padding
        int8_t unused4[14];
        /// 3C   Echo Volume Right (8-bit signed value)
        int8_t unused14;
        /// 3D   Noise output on/off for each voice (bit-mask)
        uint8_t unused13;
        /// padding
        int8_t unused5[14];
        /// 4C   Key On for each voice (bit-mask)
        uint8_t key_ons;
        /// 4D   Echo on/off for each voice (bit-mask)
        uint8_t unused12;
        /// padding
        int8_t unused6[14];
        /// 5C   key off for each voice (instantiates release mode) (bit-mask)
        uint8_t key_offs;
        /// 5D   source directory (wave table offsets)
        uint8_t wave_page;
        /// padding
        int8_t unused7[14];
        /// 6C   flags and noise freq (coded 8-bit value)
        uint8_t unused19;
        /// 6D   the page of RAM to use for the echo buffer
        uint8_t unused10;
        /// padding
        int8_t unused8[14];
        /// 7C   whether the sample has ended for each voice (bit-mask)
        uint8_t wave_ended;
        /// 7D   ms >> 4
        uint8_t unused11;
        /// padding
        char unused9[2];
    };

 private:
    /// @brief Return the Gaussian interpolation table value for the given index.
    ///
    /// @param index the index of the Gaussian interpolation to return
    /// @returns the Gaussian interpolation table value at given index
    ///
    static inline const int16_t* getGaussian(unsigned index) {
        // Base normal_gauss table is almost exactly (with an error of 0 or -1 for each entry):
        // int normal_gauss[512];
        // normal_gauss[i] = exp((i-511)*(i-511)*-9.975e-6)*pow(sin(0.00307096*i),1.7358)*1304.45

        // Interleaved gauss table (to improve cache coherency).
        // gauss[i * 2 + j] = normal_gauss[(1 - j) * 256 + i]
        static const int16_t gauss[512] = {
         370,1305, 366,1305, 362,1304, 358,1304, 354,1304, 351,1304, 347,1304, 343,1303,
         339,1303, 336,1303, 332,1302, 328,1302, 325,1301, 321,1300, 318,1300, 314,1299,
         311,1298, 307,1297, 304,1297, 300,1296, 297,1295, 293,1294, 290,1293, 286,1292,
         283,1291, 280,1290, 276,1288, 273,1287, 270,1286, 267,1284, 263,1283, 260,1282,
         257,1280, 254,1279, 251,1277, 248,1275, 245,1274, 242,1272, 239,1270, 236,1269,
         233,1267, 230,1265, 227,1263, 224,1261, 221,1259, 218,1257, 215,1255, 212,1253,
         210,1251, 207,1248, 204,1246, 201,1244, 199,1241, 196,1239, 193,1237, 191,1234,
         188,1232, 186,1229, 183,1227, 180,1224, 178,1221, 175,1219, 173,1216, 171,1213,
         168,1210, 166,1207, 163,1205, 161,1202, 159,1199, 156,1196, 154,1193, 152,1190,
         150,1186, 147,1183, 145,1180, 143,1177, 141,1174, 139,1170, 137,1167, 134,1164,
         132,1160, 130,1157, 128,1153, 126,1150, 124,1146, 122,1143, 120,1139, 118,1136,
         117,1132, 115,1128, 113,1125, 111,1121, 109,1117, 107,1113, 106,1109, 104,1106,
         102,1102, 100,1098,  99,1094,  97,1090,  95,1086,  94,1082,  92,1078,  90,1074,
          89,1070,  87,1066,  86,1061,  84,1057,  83,1053,  81,1049,  80,1045,  78,1040,
          77,1036,  76,1032,  74,1027,  73,1023,  71,1019,  70,1014,  69,1010,  67,1005,
          66,1001,  65, 997,  64, 992,  62, 988,  61, 983,  60, 978,  59, 974,  58, 969,
          56, 965,  55, 960,  54, 955,  53, 951,  52, 946,  51, 941,  50, 937,  49, 932,
          48, 927,  47, 923,  46, 918,  45, 913,  44, 908,  43, 904,  42, 899,  41, 894,
          40, 889,  39, 884,  38, 880,  37, 875,  36, 870,  36, 865,  35, 860,  34, 855,
          33, 851,  32, 846,  32, 841,  31, 836,  30, 831,  29, 826,  29, 821,  28, 816,
          27, 811,  27, 806,  26, 802,  25, 797,  24, 792,  24, 787,  23, 782,  23, 777,
          22, 772,  21, 767,  21, 762,  20, 757,  20, 752,  19, 747,  19, 742,  18, 737,
          17, 732,  17, 728,  16, 723,  16, 718,  15, 713,  15, 708,  15, 703,  14, 698,
          14, 693,  13, 688,  13, 683,  12, 678,  12, 674,  11, 669,  11, 664,  11, 659,
          10, 654,  10, 649,  10, 644,   9, 640,   9, 635,   9, 630,   8, 625,   8, 620,
           8, 615,   7, 611,   7, 606,   7, 601,   6, 596,   6, 592,   6, 587,   6, 582,
           5, 577,   5, 573,   5, 568,   5, 563,   4, 559,   4, 554,   4, 550,   4, 545,
           4, 540,   3, 536,   3, 531,   3, 527,   3, 522,   3, 517,   2, 513,   2, 508,
           2, 504,   2, 499,   2, 495,   2, 491,   2, 486,   1, 482,   1, 477,   1, 473,
           1, 469,   1, 464,   1, 460,   1, 456,   1, 451,   1, 447,   1, 443,   1, 439,
           0, 434,   0, 430,   0, 426,   0, 422,   0, 418,   0, 414,   0, 410,   0, 405,
           0, 401,   0, 397,   0, 393,   0, 389,   0, 385,   0, 381,   0, 378,   0, 374,
        };
        // index as bytes, but return as 16-bit signed
        return reinterpret_cast<int16_t const*>(reinterpret_cast<int8_t const*>(gauss) + index);
    }

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
    int keys = 0;

    // -----------------------------------------------------------------------
    // MARK: Internal Voice State
    // -----------------------------------------------------------------------

    /// the number of samples delay until the voice turns on (after key-on)
    short on_count = 0;
    /// The stages of the ADSR envelope generator.
    enum class EnvelopeStage { On, Release };
    /// the current stage of the envelope generator
    EnvelopeStage envelope_stage = EnvelopeStage::Release;
    /// the output value from the envelope generator
    short envelope_value = 0;
    /// the current address of the sample being played by the voice
    unsigned short addr = 0;
    /// header byte from current block
    short block_header = 0;
    /// number of nibbles remaining in current block
    short block_remain = 0;
    /// the previous four samples for Gaussian interpolation
    int16_t samples[4] = {0, 0, 0, 0};
    /// the 14-bit frequency value
    uint16_t rate = 0;
    /// 12-bit fractional position
    short fraction = 0;
    /// the volume for the left channel output
    int8_t volumeLeft = 0;
    /// the volume for the right channel output
    int8_t volumeRight = 0;

    /// @brief Process the envelope for the voice with given index.
    ///
    /// returns the envelope counter value for given index in the table
    ///
    inline int clock_envelope() {
        // process the release stage
        if (envelope_stage == EnvelopeStage::Release) {
            // Docs: "When in the state of "key off". the "click" sound is
            // prevented by the addition of the fixed value 1/256" WTF???
            // Alright, I'm going to choose to interpret that this way:
            // When a note is keyed off, start the RELEASE state, which
            // subtracts 1/256th each sample period (32kHz).  Note there's
            // no need for a count because it always happens every update.
            envelope_value -= 0x0800 / 256;
            if (envelope_value <= 0) {
                envelope_value = 0;
                keys &= ~1;
                return -1;
            }
            return envelope_value;
        } else {
            // process the on stage
            envelope_value = 127 << 4;
            return envelope_value;
        }
    }

 public:
    /// @brief Initialize a new Sony_S_DSP_BRR.
    ///
    /// @param ram_ a pointer to the 64KB shared RAM
    ///
    explicit Sony_S_DSP_BRR(uint8_t* ram_) : ram(ram_) { }

    /// @brief Clear state and silence everything.
    void reset() {
        keys = 0;
        global.key_ons = 0;
        envelope_stage = EnvelopeStage::Release;
    }

    /// @brief Set the frequency of the low-pass gate to a new value.
    ///
    /// @param freq the frequency to set the low-pass gate to
    ///
    inline void setFrequency(float freq) { rate = get_pitch(freq); }

    /// @brief Set the volume to new level for the left channel.
    ///
    /// @param value the level to set the left channel to
    ///
    inline void setVolumeLeft(int8_t value) { volumeLeft = value; }

    /// @brief Set the volume to new level for the right channel.
    ///
    /// @param value the level to set the right channel to
    ///
    inline void setVolumeRight(int8_t value) { volumeRight = value; }

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
    }

    /// @brief Run DSP for some samples and write them to the given buffer.
    ///
    /// @param output_buffer the output buffer to write samples to (optional)
    /// @param phase_modulation the phase modulation to apply to the voice
    ///
    /// @details
    /// the sample rate of the system is locked to 32kHz just like the SNES
    ///
    void run(int16_t* output_buffer = nullptr, int phase_modulation = 0) {
        // use the global wave page address to lookup a pointer to the first entry
        // in the source directory. the wave page is multiplied by 0x100 to produce
        // the RAM address of the source directory.
        const SourceDirectoryEntry* const source_directory =
            reinterpret_cast<SourceDirectoryEntry*>(&ram[global.wave_page * 0x100]);
        // -------------------------------------------------------------------
        // MARK: Key Off / Key On
        // -------------------------------------------------------------------
        // Here we check for keys on / off. Docs say that successive writes
        // to KON / KOF must be separated by at least 2 T_s periods or risk
        // being neglected.  Therefore, DSP only looks at these during an
        // update, and not at the time of the write. Only need to do this
        // once however, since the regs haven't changed over the whole
        // period we need to catch up with.
        // -------------------------------------------------------------------
        // Keying on a voice resets that bit in ENDX.
        global.wave_ended &= ~global.key_ons;
        // -------------------------------------------------------------------
        // MARK: Voice Processing
        // -------------------------------------------------------------------
        // buffer the outputs of the left and right echo and main channels
        int left = 0;
        int right = 0;
        // get the voice's bit-mask shift value
        const int voice_bit = 1;
        // cache the voice and data structures
        RawVoice& raw_voice = voices[0];
        // ---------------------------------------------------------------
        // MARK: Gate Processing
        // ---------------------------------------------------------------
        if (on_count && !--on_count) {
            // key on
            keys |= voice_bit;
            addr = source_directory[raw_voice.waveform].start;
            block_remain = 1;
            envelope_value = 0;
            block_header = 0;
            // decode three samples immediately
            fraction = 0x3FFF;
            // BRR decoder filter uses previous two samples
            samples[0] = 0;
            samples[1] = 0;
            // NOTE: Real SNES does *not* appear to initialize the
            // envelope counter to anything in particular. The first
            // cycle always seems to come at a random time sooner than
            // expected; as yet, I have been unable to find any
            // pattern.  I doubt it will matter though, so we'll go
            // ahead and do the full time for now.
            envelope_stage = EnvelopeStage::On;
        }
        // key-on = !key-off = true
        if (global.key_ons & voice_bit & ~global.key_offs) {
            global.key_ons &= ~voice_bit;
            on_count = 8;
        }
        // key-off = true
        if (keys & global.key_offs & voice_bit) {
            envelope_stage = EnvelopeStage::Release;
            on_count = 0;
        }

        int envelope;
        if (!(keys & voice_bit) || (envelope = clock_envelope()) < 0) {
            // TODO: return empty samples
            return;
        }
        // ---------------------------------------------------------------
        // MARK: BRR Sample Decoder
        // Decode samples when fraction >= 1.0 (0x1000)
        // ---------------------------------------------------------------
        for (int n = fraction >> 12; --n >= 0;) {
            if (!--block_remain) {
                if (block_header & 1) {
                    global.wave_ended |= voice_bit;
                    if (block_header & 2) {
                        // verified (played endless looping sample and ENDX was set)
                        addr = source_directory[raw_voice.waveform].loop;
                    } else {  // first block was end block; don't play anything
                        goto sample_ended;
                    }
                }
                block_header = ram[addr++];
                block_remain = 16;  // nibbles
            }

            if (
                block_remain == 9 &&
                (ram[addr + 5] & 3) == 1 &&
                (block_header & 3) != 3
            ) {  // next block has end flag set, this block ends early
        sample_ended:
                global.wave_ended |= voice_bit;
                keys &= ~voice_bit;
                envelope_value = 0;
                // add silence samples to interpolation buffer
                do {
                    samples[3] = samples[2];
                    samples[2] = samples[1];
                    samples[1] = samples[0];
                    samples[0] = 0;
                } while (--n >= 0);
                break;
            }
            // get the next sample from RAM
            int delta = ram[addr];
            if (block_remain & 1) {  // use lower nibble
                delta <<= 4;
                addr++;
            }
            // Use sign-extended upper nibble
            delta = int8_t(delta) >> 4;
            // For invalid ranges (D,E,F): if the nibble is negative,
            // the result is F000.  If positive, 0000. Nothing else
            // like previous range, etc seems to have any effect.  If
            // range is valid, do the shift normally.  Note these are
            // both shifted right once to do the filters properly, but
            // the output will be shifted back again at the end.
            int shift = block_header >> 4;
            delta = (delta << shift) >> 1;
            if (shift > 0x0C) delta = (delta >> 14) & ~0x7FF;
            // -----------------------------------------------------------
            // MARK: BRR Reconstruction Filter (1,2,3 point IIR)
            // -----------------------------------------------------------
            int smp1 = samples[0];
            int smp2 = samples[1];
            if (block_header & 8) {
                delta += smp1;
                delta -= smp2 >> 1;
                if (!(block_header & 4)) {
                    delta += (-smp1 - (smp1 >> 1)) >> 5;
                    delta += smp2 >> 5;
                } else {
                    delta += (-smp1 * 13) >> 7;
                    delta += (smp2 + (smp2 >> 1)) >> 4;
                }
            } else if (block_header & 4) {
                delta += smp1 >> 1;
                delta += (-smp1) >> 5;
            }
            samples[3] = samples[2];
            samples[2] = smp2;
            samples[1] = smp1;
            samples[0] = 2 * clamp_16(delta);
        }
        // ---------------------------------------------------------------
        // MARK: Gaussian Interpolation Filter
        // ---------------------------------------------------------------
        // get the 14-bit frequency value
        int phase = 0x3FFF & rate;
        // apply phase modulation
        phase = (phase * (phase_modulation + 32768)) >> 15;
        // Gaussian interpolation using most recent 4 samples
        const int index = fraction >> 2 & 0x3FC;
        fraction = (fraction & 0x0FFF) + phase;
        const auto table1 = getGaussian(index);
        const auto table2 = getGaussian(255 * 4 - index);
        int sample = ((table1[0] * samples[3]) >> 12) +
                     ((table1[1] * samples[2]) >> 12) +
                     ((table2[1] * samples[1]) >> 12);
        sample = static_cast<int16_t>(2 * sample);
        sample +=     (table2[0] * samples[0]) >> 11 & ~1;
        // scale output from this voice
        int output = clamp_16(sample);
        output = (output * envelope) >> 11 & ~1;
        // add the left and right channels to the mix
        left += (volumeLeft * output) >> 7;
        right += (volumeRight * output) >> 7;
        // -------------------------------------------------------------------
        // MARK: Output
        // -------------------------------------------------------------------
        if (output_buffer) {  // write final samples
            // clamp the left and right samples and place them into the buffer
            output_buffer[0] = clamp_16(left);
            output_buffer[1] = clamp_16(right);
        }
    }
};

#endif  // DSP_SONY_S_DSP_HPP_
