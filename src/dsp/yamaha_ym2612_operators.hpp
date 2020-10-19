// YM2612 FM sound chip emulator interface
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
// derived from: Game_Music_Emu 0.5.2
// Version 1.4 (final beta)
//

#ifndef DSP_YAMAHA_YM2612_OPERATORS_HPP_
#define DSP_YAMAHA_YM2612_OPERATORS_HPP_

#include <cstdint>
#include <limits>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "exceptions.hpp"

/// The number of fixed point bits for various functions of the chip.
enum FixedPointBits {
    /// the number of bits for addressing the envelope table
    ENV_BITS = 10,
    /// the number of bits for addressing the sine table
    SIN_BITS = 10,
    /// 16.16 fixed point (timers calculations)
    TIMER_SH = 16,
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

/// The stages of the envelope generator.
enum EnvelopeStage {
    /// the off stage, i.e., 0 output
    EG_OFF = 0,
    /// the release stage, i.e., falling to 0 after note-off from any stage
    EG_REL = 1,
    /// the sustain stage, i.e., holding until note-off after the decay stage
    /// ends
    EG_SUS = 2,
    /// the decay stage, i.e., falling to sustain level after the attack stage
    /// reaches the total level
    EG_DEC = 3,
    /// the attack stage, i.e., rising from 0 to the total level
    EG_ATT = 4
};

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
/// The total level amplitude table for the envelope generator
static int TL_TABLE[TL_TABLE_LENGTH];

/// The level at which the envelope becomes quiet, i.e., goes to 0
static constexpr int ENV_QUIET = TL_TABLE_LENGTH >> 3;

/// the maximal size of an unsigned sine table index
static constexpr unsigned SIN_LENGTH = 1 << SIN_BITS;
/// a bit mask for extracting sine table indexes in the valid range
static constexpr unsigned SIN_MASK = SIN_LENGTH - 1;
/// Sinusoid waveform table in 'decibel' scale
static unsigned SIN_TABLE[SIN_LENGTH];

/// @brief Return the value of operator (2,3,4) given phase, envelope, and PM.
///
/// @param phase the current phase of the operator's oscillator
/// @param env the value of the operator's envelope
/// @param pm the amount of phase modulation for the operator
///
static inline signed int op_calc(uint32_t phase, unsigned int env, signed int pm) {
    uint32_t p = (env << 3) + SIN_TABLE[(((signed int)((phase & ~FREQ_MASK) + (pm << 15))) >> FREQ_SH) & SIN_MASK];
    if (p >= TL_TABLE_LENGTH) return 0;
    return TL_TABLE[p];
}

/// @brief Return the value of operator (1) given phase, envelope, and PM.
///
/// @param phase the current phase of the operator's oscillator
/// @param env the value of the operator's envelope
/// @param pm the amount of phase modulation for the operator
///
static inline signed int op_calc1(uint32_t phase, unsigned int env, signed int pm) {
    uint32_t p = (env << 3) + SIN_TABLE[(((signed int)((phase & ~FREQ_MASK) + pm        )) >> FREQ_SH) & SIN_MASK];
    if (p >= TL_TABLE_LENGTH) return 0;
    return TL_TABLE[p];
}

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

/// @brief all 128 LFO PM waveforms
/// @details
/// 128 combinations of 7 bits meaningful (of F-NUMBER), 8 LFO depths, 32 LFO
/// output levels per one depth
static int32_t LFO_PM_TABLE[128 * 8 * 32];

// TODO: replace with constant tables, i.e., compile elsewhere, run, and extract the tables
/// Initialize generic tables.
/// @details
/// This function is not meant to be called directly. It is marked with the
/// constructor attribute to ensure the function is executed automatically and
/// precisely once before control enters the scope of the `main` function.
///
static __attribute__((constructor)) void init_tables() {
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

// ---------------------------------------------------------------------------
// MARK: Global Operator State
// ---------------------------------------------------------------------------

/// @brief The global state for all FM operators.
struct GlobalOperatorState {
    /// frequency base
    float freqbase = 0;

    /// there are 2048 FNUMs that can be generated using FNUM/BLK registers
    /// but LFO works with one more bit of a precision so we really need 4096
    /// elements. fnumber->increment counter
    uint32_t fn_table[4096];
    /// maximal phase increment (used for phase overflow)
    uint32_t fn_max = 0;
    /// freq latch
    uint8_t fn_h = 0;

    /// DETune table
    int32_t dt_table[8][32];

    /// global envelope generator counter
    uint32_t eg_cnt = 0;
    /// global envelope generator counter works at frequency = chipclock/144/3
    uint32_t eg_timer = 0;
    /// step of eg_timer
    uint32_t eg_timer_add = 0;
    /// envelope generator timer overflows every 3 samples (on real chip)
    uint32_t eg_timer_overflow = 0;

    /// current LFO phase (out of 128)
    uint8_t lfo_cnt = 0;
    /// current LFO phase runs at LFO frequency
    uint32_t lfo_timer = 0;
    /// step of lfo_timer
    uint32_t lfo_timer_add = 0;
    /// LFO timer overflows every N samples (depends on LFO frequency)
    uint32_t lfo_timer_overflow = 0;
    /// current LFO AM step
    uint32_t lfo_AM_step = 0;
    /// current LFO PM step
    uint32_t lfo_PM_step = 0;

    // TODO: make private
    // TODO: inline in set_sample_rate?
    /// @brief Initialize time tables.
    void init_timetables() {
        // DeTune table
        for (int d = 0; d <= 3; d++) {
            for (int i = 0; i <= 31; i++) {
                // -10 because chip works with 10.10 fixed point, while we use 16.16
                float rate = ((float) DT_TABLE[d * 32 + i]) * freqbase * (1 << (FREQ_SH - 10));
                dt_table[d][i] = (int32_t) rate;
                dt_table[d + 4][i] = -dt_table[d][i];
            }
        }
        // there are 2048 FNUMs that can be generated using FNUM/BLK registers
        // but LFO works with one more bit of a precision so we really need 4096
        // elements. calculate fnumber -> increment counter table
        for (int i = 0; i < 4096; i++) {
            // freq table for octave 7
            // phase increment counter = 20bit
            // the correct formula is
            //     F-Number = (144 * fnote * 2^20 / M) / 2^(B-1)
            // where sample clock is: M / 144
            // this means the increment value for one clock sample is
            //     FNUM * 2^(B-1) = FNUM * 64
            // for octave 7
            // we also need to handle the ratio between the chip frequency and
            // the emulated frequency (can be 1.0)
            // NOTE:
            // -10 because chip works with 10.10 fixed point, while we use 16.16
            fn_table[i] = (uint32_t)((float) i * 32 * freqbase * (1 << (FREQ_SH - 10)));
        }
        // maximal frequency is required for Phase overflow calculation, register
        // size is 17 bits (Nemesis)
        fn_max = (uint32_t)((float) 0x20000 * freqbase * (1 << (FREQ_SH - 10)));
    }

    /// @brief Set the output sample rate and clock rate.
    ///
    /// @param sample_rate the number of samples per second
    /// @param clock_rate the number of source clock cycles per second
    ///
    void set_sample_rate(float sample_rate, float clock_rate) {
        if (sample_rate == 0) throw Exception("sample_rate must be above 0");
        if (clock_rate == 0) throw Exception("clock_rate must be above 0");
        // frequency base
        freqbase = clock_rate / sample_rate;
        // TODO: why is it necessary to scale these increments by a factor of 1/16
        //       to get the correct timings from the EG and LFO?
        // EG timer increment (updates every 3 samples)
        eg_timer_add = (1 << EG_SH) * freqbase / 16;
        eg_timer_overflow = 3 * (1 << EG_SH) / 16;
        // LFO timer increment (updates every 16 samples)
        lfo_timer_add = (1 << LFO_SH) * freqbase / 16;
        // make time tables
        init_timetables();
    }

    /// @brief Advance LFO to next sample.
    inline void advance_lfo() {
        if (lfo_timer_overflow) {  // LFO enabled
            // increment LFO timer
            lfo_timer += lfo_timer_add;
            // when LFO is enabled, one level will last for
            // 108, 77, 71, 67, 62, 44, 8 or 5 samples
            while (lfo_timer >= lfo_timer_overflow) {
                lfo_timer -= lfo_timer_overflow;
                // There are 128 LFO steps
                lfo_cnt = ( lfo_cnt + 1 ) & 127;
                // triangle (inverted)
                // AM: from 126 to 0 step -2, 0 to 126 step +2
                if (lfo_cnt<64)
                    lfo_AM_step = (lfo_cnt ^ 63) << 1;
                else
                    lfo_AM_step = (lfo_cnt & 63) << 1;
                // PM works with 4 times slower clock
                lfo_PM_step = lfo_cnt >> 2;
            }
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: FM operators
// ---------------------------------------------------------------------------

/// @brief A single FM operator
struct Operator {
    /// attack rate
    uint32_t ar = 0;
    /// total level: TL << 3
    uint32_t tl = 0;
    /// decay rate
    uint32_t d1r = 0;
    /// sustain level:SL_TABLE[SL]
    uint32_t sl = 0;
    /// sustain rate
    uint32_t d2r = 0;
    /// release rate
    uint32_t rr = 0;

    /// detune :dt_table[DT]
    int32_t* DT = 0;
    /// multiple :ML_TABLE[ML]
    uint32_t mul = 0;

    /// phase counter
    uint32_t phase = 0;
    /// phase step
    int32_t phase_increment = 0;

    /// envelope counter
    int32_t volume = 0;
    /// current output from EG circuit (without AM from LFO)
    uint32_t vol_out = 0;

    /// key scale rate :3-KSR
    uint8_t KSR = 0;
    /// key scale rate :kcode>>(3-KSR)
    uint8_t ksr = 0;

    /// phase type
    uint8_t state = 0;

    /// attack state
    uint8_t eg_sh_ar = 0;
    /// attack state
    uint8_t eg_sel_ar = 0;
    /// decay state
    uint8_t eg_sh_d1r = 0;
    /// decay state
    uint8_t eg_sel_d1r = 0;
    /// sustain state
    uint8_t eg_sh_d2r = 0;
    /// sustain state
    uint8_t eg_sel_d2r = 0;
    /// release state
    uint8_t eg_sh_rr = 0;
    /// release state
    uint8_t eg_sel_rr = 0;

    /// SSG-EG waveform
    uint8_t ssg = 0;
    /// SSG-EG negated output
    uint8_t ssgn = 0;

    /// 0=last key was KEY OFF, 1=KEY ON
    uint32_t key = 0;

    /// AM enable flag
    uint32_t AMmask = 0;

    /// attack rate and key-scaling control register
    uint8_t ar_ksr = 0;

    /// @brief Reset the operator to its initial/default state.
    inline void reset() {
        ssg = 0;
        ssgn = 0;
        state = EG_OFF;
        volume = MAX_ATT_INDEX;
        vol_out = MAX_ATT_INDEX;
    }

    /// @brief Set the key-on flag for the given operator.
    inline void set_keyon() {
        if (key) return;
        key = 1;
        // restart Phase Generator
        phase = 0;
        ssgn = (ssg & 0x04) >> 1;
        state = EG_ATT;
    }

    /// @brief Set the key-off flag for the given operator.
    inline void set_keyoff() {
        if (!key) return;
        key = 0;
        // phase -> Release
        if (state > EG_REL)
            state = EG_REL;
    }

    /// @brief Set the 7-bit total level.
    ///
    /// @param value the value for the total level (TL)
    ///
    inline void set_tl(uint8_t value) { tl = (value & 0x7f) << (ENV_BITS - 7); }

    /// @brief Set the decay 1 rate, i.e., decay rate.
    ///
    /// @param value the value for the decay 1 rate (D1R)
    ///
    inline void set_dr(uint8_t value) {
        d1r = (value & 0x1f) ? 32 + ((value & 0x1f) << 1) : 0;
        eg_sh_d1r = ENV_RATE_SHIFT[d1r + ksr];
        eg_sel_d1r = ENV_RATE_SELECT[d1r + ksr];
    }

    /// @brief Set the sustain level rate.
    ///
    /// @param value the value to index from the sustain level table
    ///
    inline void set_sl(uint8_t value) { sl = SL_TABLE[value]; }

    /// @brief Set the decay 2 rate, i.e., sustain rate.
    ///
    /// @param value the value for the decay 2 rate (D2R)
    ///
    inline void set_sr(uint8_t value) {
        d2r = (value & 0x1f) ? 32 + ((value & 0x1f) << 1) : 0;
        eg_sh_d2r = ENV_RATE_SHIFT[d2r + ksr];
        eg_sel_d2r = ENV_RATE_SELECT[d2r + ksr];
    }

    /// @brief Set the release rate.
    ///
    /// @param value the value for the release rate (RR)
    ///
    inline void set_rr(uint8_t value) {
        rr = 34 + (value << 2);
        eg_sh_rr = ENV_RATE_SHIFT[rr + ksr];
        eg_sel_rr = ENV_RATE_SELECT[rr + ksr];
    }

    /// @brief set the SSG register to a new value.
    ///
    /// @param value the value for the looping envelope generator register (SSG)
    /// @details
    /// the first three bits describe the mode of the looping EG, the fourth
    /// bit enables / disables the looping EG
    ///
    inline void set_ssg(uint8_t value) {
        if (ssg == value) return;
        ssg = value;
        // recalculate EG output
        if ((ssg & 0x08) && (ssgn ^ (ssg & 0x04)) && (state > EG_REL))
            vol_out = ((uint32_t) (0x200 - volume) & MAX_ATT_INDEX) + tl;
        else
            vol_out = (uint32_t) volume + tl;
    }

    /// @brief SSG-EG update process.
    ///
    /// @details
    /// The behavior is based upon Nemesis tests on real hardware. This is
    /// actually executed before each sample.
    ///
    inline void update_ssg_eg_channel() {
        // detect SSG-EG transition. this is not required during release phase
        // as the attenuation has been forced to MAX and output invert flag is
        // not used. If an Attack Phase is programmed, inversion can occur on
        // each sample.
        if ((ssg & 0x08) && (volume >= 0x200) && (state > EG_REL)) {
            if (ssg & 0x01) {  // bit 0 = hold SSG-EG
                // set inversion flag
                if (ssg & 0x02) ssgn = 4;
                // force attenuation level during decay phases
                if ((state != EG_ATT) && !(ssgn ^ (ssg & 0x04)))
                    volume = MAX_ATT_INDEX;
            } else {  // loop SSG-EG
                // toggle output inversion flag or reset Phase Generator
                if (ssg & 0x02)
                    ssgn ^= 4;
                else
                    phase = 0;
                // same as Key ON
                if (state != EG_ATT) {
                    if ((ar + ksr) < 32 + 62) {  // attacking
                        state = (volume <= MIN_ATT_INDEX) ?
                            ((sl == MIN_ATT_INDEX) ? EG_SUS : EG_DEC) : EG_ATT;
                    } else {  // Attack Rate @ max -> jump to next stage
                        volume = MIN_ATT_INDEX;
                        state = (sl == MIN_ATT_INDEX) ? EG_SUS : EG_DEC;
                    }
                }
            }
            // recalculate EG output
            if (ssgn ^ (ssg & 0x04))
                vol_out = ((uint32_t) (0x200 - volume) & MAX_ATT_INDEX) + tl;
            else
                vol_out = (uint32_t) volume + tl;
        }
    }

    /// Update the envelope generator for the operator.
    ///
    /// @param eg_cnt the counter for the envelope generator
    ///
    inline void update_eg_channel(uint32_t eg_cnt) {
        unsigned int swap_flag = 0;
        switch (state) {
        case EG_ATT:  // attack phase
            if (!(eg_cnt & ((1 << eg_sh_ar) - 1))) {
                volume += (~volume * (ENV_INCREMENT_TABLE[eg_sel_ar + ((eg_cnt >> eg_sh_ar) & 7)])) >> 4;
                if (volume <= MIN_ATT_INDEX) {
                    volume = MIN_ATT_INDEX;
                    state = EG_DEC;
                }
            }
            break;
        case EG_DEC:  // decay phase
            if (ssg & 0x08) {  // SSG EG type envelope selected
                if (!(eg_cnt & ((1 << eg_sh_d1r) - 1))) {
                    volume += 4 * ENV_INCREMENT_TABLE[eg_sel_d1r + ((eg_cnt >> eg_sh_d1r) & 7)];
                    if ( volume >= static_cast<int32_t>(sl) )
                        state = EG_SUS;
                }
            } else {
                if (!(eg_cnt & ((1 << eg_sh_d1r) - 1))) {
                    volume += ENV_INCREMENT_TABLE[eg_sel_d1r + ((eg_cnt >> eg_sh_d1r) & 7)];
                    if (volume >= static_cast<int32_t>(sl))
                        state = EG_SUS;
                }
            }
            break;
        case EG_SUS:  // sustain phase
            if (ssg & 0x08) {  // SSG EG type envelope selected
                if (!(eg_cnt & ((1 << eg_sh_d2r) - 1))) {
                    volume += 4 * ENV_INCREMENT_TABLE[eg_sel_d2r + ((eg_cnt >> eg_sh_d2r) & 7)];
                    if (volume >= ENV_QUIET) {
                        volume = MAX_ATT_INDEX;
                        if (ssg & 0x01) {  // bit 0 = hold
                            if (ssgn & 1) {  // have we swapped once ???
                                // yes, so do nothing, just hold current level
                            } else {  // bit 1 = alternate
                                swap_flag = (ssg & 0x02) | 1;
                            }
                        } else {  // same as KEY-ON operation
                            // restart of the Phase Generator should be here
                            phase = 0;
                            // phase -> Attack
                            volume = 511;
                            state = EG_ATT;
                            // bit 1 = alternate
                            swap_flag = (ssg & 0x02);
                        }
                    }
                }
            } else {
                if (!(eg_cnt & ((1 << eg_sh_d2r) - 1))) {
                    volume += ENV_INCREMENT_TABLE[eg_sel_d2r + ((eg_cnt >> eg_sh_d2r) & 7)];
                    if (volume >= MAX_ATT_INDEX) {
                        volume = MAX_ATT_INDEX;
                        // do not change state (verified on real chip)
                    }
                }
            }
            break;
        case EG_REL:  // release phase
            if (!(eg_cnt & ((1 << eg_sh_rr) - 1))) {
                // SSG-EG affects Release phase also (Nemesis)
                volume += ENV_INCREMENT_TABLE[eg_sel_rr + ((eg_cnt >> eg_sh_rr) & 7)];
                if (volume >= MAX_ATT_INDEX) {
                    volume = MAX_ATT_INDEX;
                    state = EG_OFF;
                }
            }
            break;
        }
        // get the output volume from the slot
        unsigned int out = static_cast<uint32_t>(volume);
        // negate output (changes come from alternate bit, init comes from
        // attack bit)
        if ((ssg & 0x08) && (ssgn & 2) && (state > EG_REL))
            out ^= MAX_ATT_INDEX;
        // we need to store the result here because we are going to change
        // ssgn in next instruction
        vol_out = out + tl;
        // reverse oprtr inversion flag
        ssgn ^= swap_flag;
    }
};

// ---------------------------------------------------------------------------
// MARK: 4-Operator FM Synthesis Voices
// ---------------------------------------------------------------------------

/// @brief A single 4-operator FM voice.
struct Voice {
    /// four operators
    Operator operators[4];

    /// algorithm
    uint8_t algorithm = 0;
    /// feedback shift
    uint8_t feedback = 0;
    /// operator 1 output for feedback
    int32_t op1_out[2] = {0, 0};

    /// Op1 output pointer
    int32_t *connect1 = nullptr;
    /// Op3 output pointer
    int32_t *connect3 = nullptr;
    /// Op2 output pointer
    int32_t *connect2 = nullptr;
    /// Op4 output pointer
    int32_t *connect4 = nullptr;

    /// where to put the delayed sample (MEM)
    int32_t *mem_connect = nullptr;
    /// delayed sample (MEM) value
    int32_t mem_value = 0;

    /// channel phase modulation sensitivity (PMS)
    int32_t pms = 0;
    /// channel amplitude modulation sensitivity (AMS)
    uint8_t ams = 0;

    /// fnum, blk : adjusted to sample rate
    uint32_t fc = 0;
    /// key code:
    uint8_t kcode = 0;
    /// current blk / fnum value for this slot (can be different between slots
    /// of one channel in 3 slot mode)
    uint32_t block_fnum = 0;

    /// @brief Reset the voice to its initial/default state.
    inline void reset() {
        fc = 0;
        for (auto &oprtr : operators) oprtr.reset();
    }

    /// @brief Set the feedback amount.
    ///
    /// @param feedback the amount of feedback for the first operator
    ///
    inline void set_feedback(uint8_t value) {
        value = value & 7;
        feedback = value ? value + 6 : 0;
    }

    /// @brief Set attack rate & key scale
    ///
    /// @param oprtr_idx the index of the operator
    /// @param value the value for the attack rate (AR) and key-scale rate (KSR)
    ///
    inline void set_ar_ksr(unsigned oprtr_idx, int value) {
        Operator* oprtr = &operators[oprtr_idx];
        uint8_t old_KSR = oprtr->KSR;
        oprtr->ar = (value & 0x1f) ? 32 + ((value & 0x1f) << 1) : 0;
        oprtr->KSR = 3 - (value >> 6);
        if (oprtr->KSR != old_KSR) operators[Op1].phase_increment = -1;
        // refresh Attack rate
        if (oprtr->ar + oprtr->ksr < 32 + 62) {
            oprtr->eg_sh_ar = ENV_RATE_SHIFT[oprtr->ar + oprtr->ksr ];
            oprtr->eg_sel_ar = ENV_RATE_SELECT[oprtr->ar + oprtr->ksr ];
        } else {
            oprtr->eg_sh_ar = 0;
            oprtr->eg_sel_ar = 17 * ENV_RATE_STEPS;
        }
    }

    // /// @brief set detune & multiplier.
    // ///
    // /// @param oprtr_idx the index of the operator
    // /// @param state TODO
    // /// @param value the value for the set detune (DT) & multiplier (MUL)
    // ///
    // inline void set_det_mul(unsigned oprtr_idx, GlobalOperatorState* state, int value) {
    //     Operator* oprtr = &operators[oprtr_idx];
    //     oprtr->mul = (value & 0x0f) ? (value & 0x0f) * 2 : 1;
    //     oprtr->DT = state->dt_table[(value >> 4) & 7];
    //     operators[Op1].phase_increment = -1;
    // }
};

#endif  // DSP_YAMAHA_YM2612_OPERATORS_HPP_
