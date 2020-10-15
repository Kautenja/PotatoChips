// An emulation of the BRR sample playback engine from the Sony S-DSP.
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

#ifndef DSP_SONY_S_DSP_BRR_HPP_
#define DSP_SONY_S_DSP_BRR_HPP_

#include "sony_s_dsp_common.hpp"

/// @brief An emulation of the BRR sample playback engine from the Sony S-DSP.
class Sony_S_DSP_BRR {
 public:
    enum : unsigned {
        /// the size of the RAM bank in bytes
        SIZE_OF_RAM = 1 << 16
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

    // -----------------------------------------------------------------------
    // MARK: Global S-SMP Chip State
    // -----------------------------------------------------------------------

    /// @brief A pointer to the shared 64KB RAM bank between the S-DSP and
    /// the SPC700.
    /// @details
    /// this must be maintained by the caller in order to provide data to the
    /// S-DSP. This includes input sample data, and the allocated space for the
    /// echo buffer according to the global ECHO_BUFFER_START_OFFSET register
    uint8_t* const ram = nullptr;

    /// source directory (wave table offsets)
    uint8_t wave_page = 0;

    // -----------------------------------------------------------------------
    // MARK: Internal Voice State
    // -----------------------------------------------------------------------

    /// the number of samples delay until the voice turns on (after key-on)
    short on_count = 0;
    /// The stages of the ADSR envelope generator.
    enum class EnvelopeStage { On, Release, Off };
    /// the current stage of the envelope generator
    EnvelopeStage envelope_stage = EnvelopeStage::Off;
    /// the output value from the envelope generator
    short envelope_value = 0;
    /// the index of the starting sample of the waveform
    uint8_t wave_index = 0;
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
        // the initial value of the envelope
        static constexpr uint16_t ENVELOPE_INITIAL = 0x0800;
        // process the release stage
        if (envelope_stage == EnvelopeStage::Release) {
            // Docs: "When in the state of "key off". the "click" sound is
            // prevented by the addition of the fixed value 1/256" WTF???
            // Alright, I'm going to choose to interpret that this way:
            // When a note is keyed off, start the RELEASE state, which
            // subtracts 1/256th each sample period (32kHz).  Note there's
            // no need for a count because it always happens every update.
            envelope_value -= ENVELOPE_INITIAL / 256;
            if (envelope_value <= 0) {
                envelope_stage = EnvelopeStage::Off;
                envelope_value = 0;
                return -1;
            }
            return envelope_value;
        }
        // process the on stage
        envelope_value = ENVELOPE_INITIAL;
        return envelope_value;
    }

 public:
    /// @brief Initialize a new Sony_S_DSP_BRR.
    ///
    /// @param ram_ a pointer to the 64KB shared RAM
    ///
    explicit Sony_S_DSP_BRR(uint8_t* ram_) : ram(ram_) { }

    /// @brief Clear state and silence everything.
    void reset() {
        // TODO:
    }

    /// @brief Set the page of samples in RAM to read samples from.
    ///
    /// @param address the address in RAM to start the wave page from
    ///
    /// @details
    /// Source Directory Offset.
    ///
    /// DIR
    ///          7     6     5     4     3     2     1     0
    ///       +-----+-----+-----+-----+-----+-----+-----+-----+
    /// $5D   |                  Offset value                 |
    ///       +-----+-----+-----+-----+-----+-----+-----+-----+
    /// This register points to the source(sample) directory in external
    /// RAM. The pointer is calculated by Offset*0x100. This is because each
    /// directory is 4-bytes (0x100).
    ///
    /// The source directory contains sample start and loop point offsets.
    /// Its a simple array of 16-bit values.
    ///
    /// SAMPLE DIRECTORY
    ///
    /// OFFSET  SIZE    DESC
    /// dir+0   16-BIT  SAMPLE-0 START
    /// dir+2   16-BIT  SAMPLE-0 LOOP START
    /// dir+4   16-BIT  SAMPLE-1 START
    /// dir+6   16-BIT  SAMPLE-1 LOOP START
    /// dir+8   16-BIT  SAMPLE-2 START
    /// ...
    /// This can continue for up to 256 samples. (SRCN can only reference
    /// 256 samples)
    ///
    inline void setWavePage(uint8_t address) { wave_page = address; }

    /// @brief Set the index of the sample in the source directory to play.
    ///
    /// @param index the offset of the sample from the wave page
    ///
    /// @details
    /// Source number is a reference to the "Source Directory" (see DIR).
    /// The DSP will use the sample with this index from the directory.
    /// I'm not sure what happens when you change the SRCN when the
    /// channel is active, but it probably doesn't have any effect
    /// until KON is set.
    ///          7     6     5     4     3     2     1     0
    ///       +-----+-----+-----+-----+-----+-----+-----+-----+
    /// $x4   |                 Source Number                 |
    ///       +-----+-----+-----+-----+-----+-----+-----+-----+
    ///
    inline void setWaveIndex(uint8_t index) { wave_index = index; }

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

    /// @brief Run DSP for some samples and write them to the given buffer.
    ///
    /// @param output_buffer the output buffer to write samples to (optional)
    /// @param phase_modulation the phase modulation to apply to the voice
    ///
    /// @details
    /// the sample rate of the system is locked to 32kHz just like the SNES
    ///
    void run(bool trigger, bool gate_on, int16_t* output_buffer = nullptr, int phase_modulation = 0) {
        // use the global wave page address to lookup a pointer to the first entry
        // in the source directory. the wave page is multiplied by 0x100 to produce
        // the RAM address of the source directory.
        const SourceDirectoryEntry* const source_directory =
            reinterpret_cast<SourceDirectoryEntry*>(&ram[wave_page * 0x100]);

        if (on_count && !--on_count) {
            addr = source_directory[wave_index].start;
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

        if (trigger)  // trigger the voice (8 sample delay until voice on)
            on_count = 8;
        if (!gate_on) {  // enter the release stage
            envelope_stage = EnvelopeStage::Release;
            on_count = 0;
        }

        // trigger the envelope generator
        if (envelope_stage == EnvelopeStage::Off) return;
        int envelope = clock_envelope();
        if (envelope < 0) return;

        // ---------------------------------------------------------------
        // MARK: BRR Sample Decoder
        // Decode samples when fraction >= 1.0 (0x1000)
        // ---------------------------------------------------------------
        for (int n = fraction >> 12; --n >= 0;) {
            if (!--block_remain) {
                if (block_header & 1) {
                    if (block_header & 2) {
                        // verified (played endless looping sample and ENDX was set)
                        addr = source_directory[wave_index].loop;
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
                envelope_stage = EnvelopeStage::Off;
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
        // -------------------------------------------------------------------
        // MARK: Output
        // -------------------------------------------------------------------
        if (output_buffer) {  // write final samples
            // clamp the left and right samples and place them into the buffer
            output_buffer[0] = clamp_16((volumeLeft * output) >> 7);
            output_buffer[1] = clamp_16((volumeRight * output) >> 7);
        }
    }
};

#endif  // DSP_SONY_S_DSP_BRR_HPP_
