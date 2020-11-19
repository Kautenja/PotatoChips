// Lookup tables for YM2612 emulation.
// Copyright 2020 Christian Kauten
// Copyright 2006 Shay Green
// Copyright 2001 Jarek Burczynski
// Copyright 1998 Tatsuyuki Satoh
// Copyright 1997 Nicola Salmoria and the MAME team
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

#ifndef DSP_YAMAHA_YM2612_TABLES_HPP_
#define DSP_YAMAHA_YM2612_TABLES_HPP_

#include <cstdint>
#include <limits>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstring>

/// @brief Yamaha YM2612 emulation components.
namespace YamahaYM2612 {

/// The number of fixed point bits for various functions of the chip.
enum FixedPointBits {
    /// the number of bits for addressing the envelope table
    ENV_BITS = 10,
    /// the number of bits for addressing the sine table
    SIN_BITS = 10,
    /// 16.16 fixed point (frequency calculations)
    FREQ_SH  = 16,
    /// 16.16 fixed point (envelope generator timing)
    EG_SH    = 16,
    ///  8.24 fixed point (LFO calculations)
    LFO_SH   = 24
};

/// a mask for extracting valid phase from the 16-bit phase counter
static constexpr unsigned FREQ_MASK = (1 << FREQ_SH) - 1;

/// the maximal size of an unsigned envelope table index
static constexpr unsigned ENV_LENGTH = 1 << ENV_BITS;
/// the step size of increments in the envelope table
static constexpr float ENV_STEP = 128.0 / ENV_LENGTH;

/// the index of the maximal envelope value
static constexpr int MAX_ATT_INDEX = ENV_LENGTH - 1;
/// the index of the minimal envelope value
static constexpr int MIN_ATT_INDEX = 0;

/// The logical indexes of operators based on semantic name.
enum OperatorIndex {
    /// The index of operator 1
    Op1 = 0,
    /// The index of operator 2
    Op2 = 2,
    /// The index of operator 3
    Op3 = 1,
    /// The index of operator 4
    Op4 = 3
};

/// The logical indexes of operators based on sequential index.
static const uint8_t OPERATOR_INDEXES[4] = {0, 2, 1, 3};

/// 8 bits addressing (real chip)
static constexpr unsigned TL_RESOLUTION_LENGTH = 256;
/// TL_TABLE_LENGTH is calculated as:
/// 13                   - sinus amplitude bits     (Y axis)
/// 2                    - sinus sign bit           (Y axis)
/// TL_RESOLUTION_LENGTH - sinus resolution         (X axis)
static constexpr unsigned TL_TABLE_LENGTH = 13 * 2 * TL_RESOLUTION_LENGTH;

/// The level at which the envelope becomes quiet
static constexpr int ENV_QUIET = TL_TABLE_LENGTH >> 3;

/// the maximal size of an unsigned sine table index
static constexpr unsigned SIN_LENGTH = 1 << SIN_BITS;
/// a bit mask for extracting sine table indexes in the valid range
static constexpr unsigned SIN_MASK = SIN_LENGTH - 1;

/// Sustain level table (3dB per step)
/// bit0, bit1, bit2, bit3, bit4, bit5, bit6
/// 1,    2,    4,    8,    16,   32,   64   (value)
/// 0.75, 1.5,  3,    6,    12,   24,   48   (dB)
///
/// 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)
static const uint32_t SL_TABLE[16] = {
#define SC(db) (uint32_t)(db * (4.0 / ENV_STEP))
    SC(0), SC(1), SC(2), SC(3), SC(4), SC(5), SC(6), SC(7),
    SC(8), SC(9), SC(10), SC(11), SC(12), SC(13), SC(14), SC(31)
#undef SC
};

/// TODO:
static constexpr unsigned ENV_RATE_STEPS = 8;

/// TODO:
static const uint8_t ENV_INCREMENT_TABLE[19 * ENV_RATE_STEPS] = {
// Cycle
//  0    1   2   3   4   5   6   7
    0,   1,  0,  1,  0,  1,  0,  1,  // 0:  rates 00..11 0 (increment by 0 or 1)
    0,   1,  0,  1,  1,  1,  0,  1,  // 1:  rates 00..11 1
    0,   1,  1,  1,  0,  1,  1,  1,  // 2:  rates 00..11 2
    0,   1,  1,  1,  1,  1,  1,  1,  // 3:  rates 00..11 3

    1,   1,  1,  1,  1,  1,  1,  1,  // 4:  rate 12 0 (increment by 1)
    1,   1,  1,  2,  1,  1,  1,  2,  // 5:  rate 12 1
    1,   2,  1,  2,  1,  2,  1,  2,  // 6:  rate 12 2
    1,   2,  2,  2,  1,  2,  2,  2,  // 7:  rate 12 3

    2,   2,  2,  2,  2,  2,  2,  2,  // 8:  rate 13 0 (increment by 2)
    2,   2,  2,  4,  2,  2,  2,  4,  // 9:  rate 13 1
    2,   4,  2,  4,  2,  4,  2,  4,  // 10: rate 13 2
    2,   4,  4,  4,  2,  4,  4,  4,  // 11: rate 13 3

    4,   4,  4,  4,  4,  4,  4,  4,  // 12: rate 14 0 (increment by 4)
    4,   4,  4,  8,  4,  4,  4,  8,  // 13: rate 14 1
    4,   8,  4,  8,  4,  8,  4,  8,  // 14: rate 14 2
    4,   8,  8,  8,  4,  8,  8,  8,  // 15: rate 14 3

    8,   8,  8,  8,  8,  8,  8,  8,  // 16: rates 15 0, 15 1, 15 2, 15 3 (increment by 8)
    16, 16, 16, 16, 16, 16, 16, 16,  // 17: rates 15 2, 15 3 for attack
    0,   0,  0,  0,  0,  0,  0,  0,  // 18: infinity rates for attack and decay(s)
};

/// Envelope Generator rates (32 + 64 rates + 32 RKS).
/// NOTE: there is no O(17) in this table - it's directly in the code
static const uint8_t ENV_RATE_SELECT[32 + 64 + 32] = {
#define O(a) (a * ENV_RATE_STEPS)
    // 32 infinite time rates
    O(18), O(18), O(18), O(18), O(18), O(18), O(18), O(18),
    O(18), O(18), O(18), O(18), O(18), O(18), O(18), O(18),
    O(18), O(18), O(18), O(18), O(18), O(18), O(18), O(18),
    O(18), O(18), O(18), O(18), O(18), O(18), O(18), O(18),
    // rates 00-11
    O(0), O(1), O(2), O(3),
    O(0), O(1), O(2), O(3),
    O(0), O(1), O(2), O(3),
    O(0), O(1), O(2), O(3),
    O(0), O(1), O(2), O(3),
    O(0), O(1), O(2), O(3),
    O(0), O(1), O(2), O(3),
    O(0), O(1), O(2), O(3),
    O(0), O(1), O(2), O(3),
    O(0), O(1), O(2), O(3),
    O(0), O(1), O(2), O(3),
    O(0), O(1), O(2), O(3),
    // rate 12
    O(4), O(5), O(6), O(7),
    // rate 13
    O(8), O(9), O(10), O(11),
    // rate 14
    O(12), O(13), O(14), O(15),
    // rate 15
    O(16), O(16), O(16), O(16),
    // 32 dummy rates (same as 15 3)
    O(16), O(16), O(16), O(16), O(16), O(16), O(16), O(16),
    O(16), O(16), O(16), O(16), O(16), O(16), O(16), O(16),
    O(16), O(16), O(16), O(16), O(16), O(16), O(16), O(16),
    O(16), O(16), O(16), O(16), O(16), O(16), O(16), O(16)
#undef O
};

/// Envelope Generator counter shifts (32 + 64 rates + 32 RKS)
/// rate  0,    1,    2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
/// shift 11,  10,  9,  8,  7,  6,  5,  4,  3,  2, 1,  0,  0,  0,  0,  0
/// mask  2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3, 1,  0,  0,  0,  0,  0
static const uint8_t ENV_RATE_SHIFT[32 + 64 + 32] = {
#define O(a) (a * 1)
    // 32 infinite time rates
    O(0), O(0), O(0), O(0), O(0), O(0), O(0), O(0),
    O(0), O(0), O(0), O(0), O(0), O(0), O(0), O(0),
    O(0), O(0), O(0), O(0), O(0), O(0), O(0), O(0),
    O(0), O(0), O(0), O(0), O(0), O(0), O(0), O(0),
    // rates 00-11
    O(11), O(11), O(11), O(11),
    O(10), O(10), O(10), O(10),
    O(9), O(9), O(9), O(9),
    O(8), O(8), O(8), O(8),
    O(7), O(7), O(7), O(7),
    O(6), O(6), O(6), O(6),
    O(5), O(5), O(5), O(5),
    O(4), O(4), O(4), O(4),
    O(3), O(3), O(3), O(3),
    O(2), O(2), O(2), O(2),
    O(1), O(1), O(1), O(1),
    O(0), O(0), O(0), O(0),
    // rate 12
    O(0), O(0), O(0), O(0),
    // rate 13
    O(0), O(0), O(0), O(0),
    // rate 14
    O(0), O(0), O(0), O(0),
    // rate 15
    O(0), O(0), O(0), O(0),
    // 32 dummy rates (same as 15 3)
    O(0), O(0), O(0), O(0), O(0), O(0), O(0), O(0),
    O(0), O(0), O(0), O(0), O(0), O(0), O(0), O(0),
    O(0), O(0), O(0), O(0), O(0), O(0), O(0), O(0),
    O(0), O(0), O(0), O(0), O(0), O(0), O(0), O(0)
#undef O
};

/// this is YM2151 and YM2612 phase increment data (in 10.10 fixed point format)
static const uint8_t DT_TABLE[4 * 32] = {
    // FD=0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // FD=1
    0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
    2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8,
    // FD=2
    1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5,
    5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 16, 16, 16, 16,
    // FD=3
    2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7,
    8, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 20, 22, 22, 22, 22
};

/// OPN key frequency number -> key code follow table
/// fnum higher 4bit -> keycode lower 2bit
static const uint8_t FREQUENCY_KEYCODE_TABLE[16] = {
    0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3
};

/// 8 LFO speed parameters. Each value represents number of samples that one
/// LFO level will last for
static const uint32_t LFO_SAMPLES_PER_STEP[8] = {108, 77, 71, 67, 62, 44, 8, 5};

/// There are 4 different LFO AM depths available, they are:
/// 0 dB, 1.4 dB, 5.9 dB, 11.8 dB
/// Here is how it is generated (in EG steps):
///
/// 11.8 dB = 0, 2, 4, 6, 8, 10,12,14,16...126,126,124,122,120,118,....4,2,0
///  5.9 dB = 0, 1, 2, 3, 4, 5, 6, 7, 8....63, 63, 62, 61, 60, 59,.....2,1,0
///  1.4 dB = 0, 0, 0, 0, 1, 1, 1, 1, 2,...15, 15, 15, 15, 14, 14,.....0,0,0
///
/// (1.4 dB is losing precision as you can see)
///
/// It's implemented as generator from 0..126 with step 2 then a shift
/// right N times, where N is:
/// -   8 for 0 dB
/// -   3 for 1.4 dB
/// -   1 for 5.9 dB
/// -   0 for 11.8 dB
///
static const uint8_t LFO_AMS_DEPTH_SHIFT[4] = {8, 3, 1, 0};

/// There are 8 different LFO PM depths available, they are:
///   0, 3.4, 6.7, 10, 14, 20, 40, 80 (cents)
///
///   Modulation level at each depth depends on F-NUMBER bits: 4,5,6,7,8,9,10
///   (bits 8,9,10 = FNUM MSB from OCT/FNUM register)
///
///   Here we store only first quarter (positive one) of full waveform.
///   Full table (LFO_PM_TABLE) containing all 128 waveforms is build
///   at run (init) time.
///
///   One value in table below represents 4 (four) basic LFO steps
///   (1 PM step = 4 AM steps).
///
///   For example:
///    at LFO SPEED=0 (which is 108 samples per basic LFO step)
///    one value from "LFO_PM_OUTPUT" table lasts for 432 consecutive
///    samples (4*108=432) and one full LFO waveform cycle lasts for 13824
///    samples (32*432=13824; 32 because we store only a quarter of whole
///             waveform in the table below)
///
static const uint8_t LFO_PM_OUTPUT[7 * 8][8] = {
// 7 bits meaningful (of F-NUMBER), 8 LFO output levels per one depth
// (out of 32), 8 LFO depths
    /* FNUM BIT 4: 000 0001xxxx */
    /* DEPTH 0 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 1 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 2 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 3 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 4 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 5 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 6 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 7 */ {0, 0, 0, 0, 1, 1, 1, 1},
    /* FNUM BIT 5: 000 0010xxxx */
    /* DEPTH 0 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 1 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 2 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 3 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 4 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 5 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 6 */ {0, 0, 0, 0, 1, 1, 1, 1},
    /* DEPTH 7 */ {0, 0, 1, 1, 2, 2, 2, 3},
    /* FNUM BIT 6: 000 0100xxxx */
    /* DEPTH 0 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 1 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 2 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 3 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 4 */ {0, 0, 0, 0, 0, 0, 0, 1},
    /* DEPTH 5 */ {0, 0, 0, 0, 1, 1, 1, 1},
    /* DEPTH 6 */ {0, 0, 1, 1, 2, 2, 2, 3},
    /* DEPTH 7 */ {0, 0, 2, 3, 4, 4, 5, 6},
    /* FNUM BIT 7: 000 1000xxxx */
    /* DEPTH 0 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 1 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 2 */ {0, 0, 0, 0, 0, 0, 1, 1},
    /* DEPTH 3 */ {0, 0, 0, 0, 1, 1, 1, 1},
    /* DEPTH 4 */ {0, 0, 0, 1, 1, 1, 1, 2},
    /* DEPTH 5 */ {0, 0, 1, 1, 2, 2, 2, 3},
    /* DEPTH 6 */ {0, 0, 2, 3, 4, 4, 5, 6},
    /* DEPTH 7 */ {0, 0, 4, 6, 8, 8, 0xa, 0xc},
    /* FNUM BIT 8: 001 0000xxxx */
    /* DEPTH 0 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 1 */ {0, 0, 0, 0, 1, 1, 1, 1},
    /* DEPTH 2 */ {0, 0, 0, 1, 1, 1, 2, 2},
    /* DEPTH 3 */ {0, 0, 1, 1, 2, 2, 3, 3},
    /* DEPTH 4 */ {0, 0, 1, 2, 2, 2, 3, 4},
    /* DEPTH 5 */ {0, 0, 2, 3, 4, 4, 5, 6},
    /* DEPTH 6 */ {0, 0, 4, 6, 8, 8, 0xa, 0xc},
    /* DEPTH 7 */ {0, 0, 8, 0xc, 0x10, 0x10, 0x14, 0x18},
    /* FNUM BIT 9: 010 0000xxxx */
    /* DEPTH 0 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 1 */ {0, 0, 0, 0, 2, 2, 2, 2},
    /* DEPTH 2 */ {0, 0, 0, 2, 2, 2, 4, 4},
    /* DEPTH 3 */ {0, 0, 2, 2, 4, 4, 6, 6},
    /* DEPTH 4 */ {0, 0, 2, 4, 4, 4, 6, 8},
    /* DEPTH 5 */ {0, 0, 4, 6, 8, 8, 0xa, 0xc},
    /* DEPTH 6 */ {0, 0, 8, 0xc, 0x10, 0x10, 0x14, 0x18},
    /* DEPTH 7 */ {0, 0, 0x10, 0x18, 0x20, 0x20, 0x28, 0x30},
    /* FNUM BIT10: 100 0000xxxx */
    /* DEPTH 0 */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* DEPTH 1 */ {0, 0, 0, 0, 4, 4, 4, 4},
    /* DEPTH 2 */ {0, 0, 0, 4, 4, 4, 8, 8},
    /* DEPTH 3 */ {0, 0, 4, 4, 8, 8, 0xc, 0xc},
    /* DEPTH 4 */ {0, 0, 4, 8, 8, 8, 0xc, 0x10},
    /* DEPTH 5 */ {0, 0, 8, 0xc, 0x10, 0x10, 0x14, 0x18},
    /* DEPTH 6 */ {0, 0, 0x10, 0x18, 0x20, 0x20, 0x28, 0x30},
    /* DEPTH 7 */ {0, 0, 0x20, 0x30, 0x40, 0x40, 0x50, 0x60},
};

/// @brief Tables for the Yamaha YM2612 emulation.
class Tables {
 private:
    /// The total level amplitude table for the envelope generator
    int TL_TABLE[TL_TABLE_LENGTH];

    /// @brief Initialize the TL table.
    void init_tl_table() {
        // build Linear Power Table
        for (unsigned x = 0; x < TL_RESOLUTION_LENGTH; x++) {
            float m = (1 << 16) / pow(2, (x + 1) * (ENV_STEP / 4.0) / 8.0);
            m = floor(m);
            // we never reach (1 << 16) here due to the (x+1)
            // result fits within 16 bits at maximum
            // 16 bits here
            signed int n = (int) m;
            // 12 bits here
            n >>= 4;
            if (n & 1)  // round to nearest
                n = (n >> 1) + 1;
            else
                n = n>>1;
            // 11 bits here (rounded)
            // 13 bits here (as in real chip)
            n <<= 2;
            // 14 bits (with sign bit)
            TL_TABLE[x * 2 + 0] = n;
            TL_TABLE[x * 2 + 1] = -TL_TABLE[x * 2 + 0];
            // one entry in the 'Power' table use the following format,
            //     xxxxxyyyyyyyys with:
            //        s = sign bit
            // yyyyyyyy = 8-bits decimal part (0-TL_RESOLUTION_LENGTH)
            // xxxxx    = 5-bits integer 'shift' value (0-31) but, since Power
            //            table output is 13 bits, any value above 13 (included)
            //            would be discarded.
            for (int i = 1; i < 13; i++) {
                TL_TABLE[x * 2 + 0 + i * 2 * TL_RESOLUTION_LENGTH] =  TL_TABLE[x * 2 + 0] >> i;
                TL_TABLE[x * 2 + 1 + i * 2 * TL_RESOLUTION_LENGTH] = -TL_TABLE[x * 2 + 0 + i * 2 * TL_RESOLUTION_LENGTH];
            }
        }
    }

    /// Sinusoid waveform table in 'decibel' scale
    unsigned SIN_TABLE[SIN_LENGTH];

    /// @brief Initialize the sine table.
    void init_sin_table() {
        // build Logarithmic Sinus table
        for (unsigned i = 0; i < SIN_LENGTH; i++) {
            // non-standard sinus (checked against the real chip)
            float m = sin(((i * 2) + 1) * M_PI / SIN_LENGTH);
            // we never reach zero here due to ((i * 2) + 1)
            // convert to decibels
            float o;
            if (m > 0.0)
                o = 8 * log(1.0 / m) / log(2.0);
            else
                o = 8 * log(-1.0 / m) / log(2.0);
            o = o / (ENV_STEP / 4);
            signed int n = (int)(2.0 * o);
            if (n & 1)  // round to nearest
                n = (n >> 1) + 1;
            else
                n = n >> 1;
            // 13-bits (8.5) value is formatted for above 'Power' table
            SIN_TABLE[i] = n * 2 + (m >= 0.0 ? 0 : 1);
        }
    }

    /// @brief all 128 LFO PM waveforms
    /// @details
    /// 128 combinations of 7 bits meaningful (of F-NUMBER), 8 LFO depths, 32
    /// LFO output levels per one depth
    int32_t LFO_PM_TABLE[128 * 8 * 32];

    /// @brief Initialize the LFO PM table.
    void init_lfo_pm_table() {
        // build LFO PM modulation table
        for (int i = 0; i < 8; i++) {  // 8 PM depths
            for (uint8_t fnum = 0; fnum < 128; fnum++) {  // 7 bits of F-NUMBER
                for (uint8_t step = 0; step < 8; step++) {
                    uint8_t value = 0;
                    for (uint32_t bit_tmp = 0; bit_tmp < 7; bit_tmp++) {  // 7 bits
                        if (fnum & (1 << bit_tmp)) {
                            uint32_t offset_fnum_bit = bit_tmp * 8;
                            value += LFO_PM_OUTPUT[offset_fnum_bit + i][step];
                        }
                    }
                    // 32 steps for LFO PM (sinus)
                    LFO_PM_TABLE[(fnum * 32 * 8) + (i * 32) +  step      +  0] =  value;
                    LFO_PM_TABLE[(fnum * 32 * 8) + (i * 32) + (step ^ 7) +  8] =  value;
                    LFO_PM_TABLE[(fnum * 32 * 8) + (i * 32) +  step      + 16] = -value;
                    LFO_PM_TABLE[(fnum * 32 * 8) + (i * 32) + (step ^ 7) + 24] = -value;
                }
            }
        }
    }

    /// @brief initialize a new set of tables. (private)
    Tables() { init_tl_table(); init_sin_table(); init_lfo_pm_table(); }

    /// Disable the copy constructor.
    Tables(const Tables&);

    /// Disable the assignment operator
    Tables& operator=(const Tables&);

    /// @brief Return the singleton instance.
    ///
    /// @returns a pointer to the global singleton instance
    ///
    static Tables *instance() {
        // create a static instance of the singleton
        static Tables instance_ = Tables();
        // return a pointer to the global static instance
        return &instance_;
    }

 public:
    /// @brief Return the total level value for the given index.
    ///
    /// @param index the index of the total level value to return
    /// @returns the total level value for the given index
    ///
    static inline int get_tl(unsigned index) {
        return instance()->TL_TABLE[index];
    }

    /// @brief Return the sin value for the given index.
    ///
    /// @param index the index of the sin value to return
    /// @returns the sin value for the given index
    ///
    static inline unsigned get_sin(unsigned index) {
        return instance()->SIN_TABLE[index];
    }

    /// @brief Return the LFO PM value for the given index.
    ///
    /// @param index the index of the LFO PM value to return
    /// @returns the LFO PM value for the given index
    ///
    static inline int32_t get_lfo_pm(unsigned index) {
        return instance()->LFO_PM_TABLE[index];
    }
};

};  // namespace YamahaYM2612

#endif  // DSP_YAMAHA_YM2612_TABLES_HPP_
