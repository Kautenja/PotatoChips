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

#ifndef DSP_YAMAHA_YM2612_HPP_
#define DSP_YAMAHA_YM2612_HPP_

#include <cstdint>
#include <limits>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstring>

#define TYPE_SSG 0x01     // SSG support
#define TYPE_LFOPAN 0x02  // OPN type LFO and PAN
#define TYPE_6CH 0x04     // FM 6CH / 3CH
#define TYPE_DAC 0x08     // YM2612's DAC device
#define TYPE_ADPCM 0x10   // two ADPCM units
#define TYPE_YM2612 (TYPE_DAC | TYPE_LFOPAN | TYPE_6CH)

#define FREQ_SH  16  // 16.16 fixed point (frequency calculations)
#define EG_SH    16  // 16.16 fixed point (envelope generator timing)
#define LFO_SH   24  //  8.24 fixed point (LFO calculations)
#define TIMER_SH 16  // 16.16 fixed point (timers calculations)

#define FREQ_MASK ((1 << FREQ_SH) - 1)

#define ENV_BITS 10
#define ENV_LEN (1 << ENV_BITS)
#define ENV_STEP (128.0 / ENV_LEN)

#define MAX_ATT_INDEX (ENV_LEN - 1)
#define MIN_ATT_INDEX (0)

#define EG_ATT 4
#define EG_DEC 3
#define EG_SUS 2
#define EG_REL 1
#define EG_OFF 0

#define SIN_BITS 10
#define SIN_LEN (1 << SIN_BITS)
#define SIN_MASK (SIN_LEN - 1)

// 8 bits addressing (real chip)
#define TL_RES_LEN (256)

#define FINAL_SH (0)
#define MAXOUT (+32767)
#define MINOUT (-32768)

// register number to channel number, slot offset
#define OPN_CHAN(N) (N & 3)
#define OPN_SLOT(N) ((N >> 2) & 3)

#define SLOT1 0
#define SLOT2 2
#define SLOT3 1
#define SLOT4 3

/// bit0   = Right enable
#define OUTD_RIGHT 1
/// bit1   = Left enable
#define OUTD_LEFT 2
/// bit1&2 = Center
#define OUTD_CENTER 3

/// TL_TAB_LEN is calculated as:
/// 13 - sinus amplitude bits     (Y axis)
/// 2  - sinus sign bit           (Y axis)
/// TL_RES_LEN - sinus resolution (X axis)
#define TL_TAB_LEN (13 * 2 * TL_RES_LEN)
static signed int tl_tab[TL_TAB_LEN];

#define ENV_QUIET (TL_TAB_LEN >> 3)

/// sin waveform table in 'decibel' scale
static unsigned int sin_tab[SIN_LEN];

/// sustain level table (3dB per step)
/// bit0, bit1, bit2, bit3, bit4, bit5, bit6
/// 1,    2,    4,    8,    16,   32,   64   (value)
/// 0.75, 1.5,  3,    6,    12,   24,   48   (dB)
///
/// 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)
static const uint32_t sl_table[16] = {
#define SC(db) (uint32_t)(db * (4.0 / ENV_STEP))
    SC(0), SC(1), SC(2), SC(3), SC(4), SC(5), SC(6), SC(7),
    SC(8), SC(9), SC(10), SC(11), SC(12), SC(13), SC(14), SC(31)
#undef SC
};

static const uint8_t slots_idx[4] = {0, 2, 1, 3};

#define RATE_STEPS (8)
static const uint8_t eg_inc[19 * RATE_STEPS] = {
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
static const uint8_t eg_rate_select[32 + 64 + 32] = {
#define O(a) (a * RATE_STEPS)
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

// rate  0,    1,    2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
// shift 11,  10,  9,  8,  7,  6,  5,  4,  3,  2, 1,  0,  0,  0,  0,  0
// mask  2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3, 1,  0,  0,  0,  0,  0

/// Envelope Generator counter shifts (32 + 64 rates + 32 RKS)
static const uint8_t eg_rate_shift[32 + 64 + 32] = {
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
static const uint8_t dt_tab[4 * 32] = {
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
static const uint8_t opn_fktable[16] = {0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3};

/// 8 LFO speed parameters. Each value represents number of samples that one
/// LFO level will last for
static const uint32_t lfo_samples_per_step[8] = {108, 77, 71, 67, 62, 44, 8, 5};

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
static const uint8_t lfo_ams_depth_shift[4] = {8, 3, 1, 0};

/// There are 8 different LFO PM depths available, they are:
///   0, 3.4, 6.7, 10, 14, 20, 40, 80 (cents)
///
///   Modulation level at each depth depends on F-NUMBER bits: 4,5,6,7,8,9,10
///   (bits 8,9,10 = FNUM MSB from OCT/FNUM register)
///
///   Here we store only first quarter (positive one) of full waveform.
///   Full table (lfo_pm_table) containing all 128 waveforms is build
///   at run (init) time.
///
///   One value in table below represents 4 (four) basic LFO steps
///   (1 PM step = 4 AM steps).
///
///   For example:
///    at LFO SPEED=0 (which is 108 samples per basic LFO step)
///    one value from "lfo_pm_output" table lasts for 432 consecutive
///    samples (4*108=432) and one full LFO waveform cycle lasts for 13824
///    samples (32*432=13824; 32 because we store only a quarter of whole
///             waveform in the table below)
///
static const uint8_t lfo_pm_output[7 * 8][8] = {
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
static int32_t lfo_pm_table[128 * 8 * 32];

/// Initialize generic tables.
/// @details
/// This function is not meant to be called directly. It is marked with the
/// constructor attribute to ensure the function is executed automatically and
/// precisely once before control enters the scope of the `main` function
static __attribute__((constructor)) void init_tables() {
    // build Linear Power Table
    for (int x = 0; x < TL_RES_LEN; x++) {
        double m = (1 << 16) / pow(2, (x + 1) * (ENV_STEP / 4.0) / 8.0);
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
        tl_tab[x * 2 + 0] = n;
        tl_tab[x * 2 + 1] = -tl_tab[x * 2 + 0];
        // one entry in the 'Power' table use the following format,
        //     xxxxxyyyyyyyys with:
        //        s = sign bit
        // yyyyyyyy = 8-bits decimal part (0-TL_RES_LEN)
        // xxxxx    = 5-bits integer 'shift' value (0-31) but, since Power
        //            table output is 13 bits, any value above 13 (included)
        //            would be discarded.
        for (int i = 1; i < 13; i++) {
            tl_tab[x * 2 + 0 + i * 2 * TL_RES_LEN] =  tl_tab[x * 2 + 0] >> i;
            tl_tab[x * 2 + 1 + i * 2 * TL_RES_LEN] = -tl_tab[x * 2 + 0 + i * 2 * TL_RES_LEN];
        }
    }
    // build Logarithmic Sinus table
    for (int i = 0; i < SIN_LEN; i++) {
        // non-standard sinus (checked against the real chip)
        double m = sin(((i * 2) + 1) * M_PI / SIN_LEN);
        // we never reach zero here due to ((i * 2) + 1)
        // convert to decibels
        double o;
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
        sin_tab[i] = n * 2 + (m >= 0.0 ? 0 : 1);
    }
    // build LFO PM modulation table
    for (int i = 0; i < 8; i++) {  // 8 PM depths
        for (uint8_t fnum = 0; fnum < 128; fnum++) {  // 7 bits of F-NUMBER
            for (uint8_t step = 0; step < 8; step++) {
                uint8_t value = 0;
                for (uint32_t bit_tmp = 0; bit_tmp < 7; bit_tmp++) {  // 7 bits
                    if (fnum & (1 << bit_tmp)) {
                        uint32_t offset_fnum_bit = bit_tmp * 8;
                        value += lfo_pm_output[offset_fnum_bit + i][step];
                    }
                }
                // 32 steps for LFO PM (sinus)
                lfo_pm_table[(fnum * 32 * 8) + (i * 32) +  step      +  0] =  value;
                lfo_pm_table[(fnum * 32 * 8) + (i * 32) + (step ^ 7) +  8] =  value;
                lfo_pm_table[(fnum * 32 * 8) + (i * 32) +  step      + 16] = -value;
                lfo_pm_table[(fnum * 32 * 8) + (i * 32) + (step ^ 7) + 24] = -value;
            }
        }
    }
}

static inline signed int op_calc(uint32_t phase, unsigned int env, signed int pm) {
    uint32_t p = (env << 3) + sin_tab[(((signed int)((phase & ~FREQ_MASK) + (pm << 15))) >> FREQ_SH) & SIN_MASK];
    if (p >= TL_TAB_LEN) return 0;
    return tl_tab[p];
}

static inline signed int op_calc1(uint32_t phase, unsigned int env, signed int pm) {
    uint32_t p = (env << 3) + sin_tab[(((signed int)((phase & ~FREQ_MASK) + pm        )) >> FREQ_SH) & SIN_MASK];
    if (p >= TL_TAB_LEN) return 0;
    return tl_tab[p];
}

// ---------------------------------------------------------------------------
// MARK: FM operators
// ---------------------------------------------------------------------------

#define YM_CH_PART(ch) (ch/3)
#define YM_CH_OFFSET(reg, ch) (reg + (ch % 3))

/// @brief A single FM operator (SLOT)
struct Operator {
    /// detune :dt_tab[DT]
    int32_t *DT = 0;
    /// key scale rate :3-KSR
    uint8_t KSR = 0;
    /// attack rate
    uint32_t ar = 0;
    /// decay rate
    uint32_t d1r = 0;
    /// sustain rate
    uint32_t d2r = 0;
    /// release rate
    uint32_t rr = 0;
    /// key scale rate :kcode>>(3-KSR)
    uint8_t ksr = 0;
    /// multiple :ML_TABLE[ML]
    uint32_t mul = 0;

    /// phase counter
    uint32_t phase = 0;
    /// phase step
    int32_t Incr = 0;

    /// phase type
    uint8_t state = 0;
    /// total level: TL << 3
    uint32_t tl = 0;
    /// envelope counter
    int32_t volume = 0;
    /// sustain level:sl_table[SL]
    uint32_t sl = 0;
    /// current output from EG circuit (without AM from LFO)
    uint32_t vol_out = 0;

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

    /// detune and multiplier control register
    uint8_t det_mul = 0;
    /// attack rate and key-scaling control register
    uint8_t ar_ksr = 0;
    /// the sustain level and release rate control register
    uint8_t sl_rr = 0;
    /// the decay rate control register
    uint8_t dr = 0;
};

/// @brief The state of an FM operator.
struct OperatorState {
    /// master clock (Hz)
    int clock = 0;
    /// sampling rate (Hz)
    int rate = 0;
    /// frequency base
    double freqbase = 0;
    /// timer prescaler
    int timer_prescaler = 0;
    /// address register
    uint8_t address = 0;
    /// interrupt level
    uint8_t irq = 0;
    /// IRQ mask
    uint8_t irqmask = 0;
    /// status flag
    uint8_t status = 0;
    /// mode  CSM / 3SLOT
    uint32_t mode = 0;
    /// pre-scaler selector
    uint8_t prescaler_sel = 0;
    /// freq latch
    uint8_t fn_h = 0;
    /// timer A
    int32_t TA = 0;
    /// timer A counter
    int32_t TAC = 0;
    /// timer B
    uint8_t TB = 0;
    /// timer B counter
    int32_t TBC = 0;
    /// DETune table
    int32_t dt_tab[8][32];
};

/// @brief A single 4-operator FM voice.
struct Voice {
    /// four SLOTs (operators)
    Operator SLOT[4];

    /// algorithm
    uint8_t ALGO = 0;
    /// feedback shift
    uint8_t FB = 0;
    /// operator 1 output for feedback
    int32_t op1_out[2] = {0, 0};

    /// SLOT1 output pointer
    int32_t *connect1 = nullptr;
    /// SLOT3 output pointer
    int32_t *connect3 = nullptr;
    /// SLOT2 output pointer
    int32_t *connect2 = nullptr;
    /// SLOT4 output pointer
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

    /// Feedback amount and algorithm selection
    uint8_t FB_ALG = 0;
    /// L+R enable, AM sensitivity, FM sensitivity
    uint8_t LR_AMS_FMS = 0;
};

/// OPN Mode Register Write
static inline void set_timers(OperatorState *ST, int v) {
    // b7 = CSM MODE
    // b6 = 3 slot mode
    // b5 = reset b
    // b4 = reset a
    // b3 = timer enable b
    // b2 = timer enable a
    // b1 = load b
    // b0 = load a
    ST->mode = v;
    // load b
    if (v & 0x02) {
        if (ST->TBC == 0) ST->TBC = (256 - ST->TB) << 4;
    } else {  // stop timer b
        ST->TBC = 0;
    }
    // load a
    if (v & 0x01) {
        if (ST->TAC == 0) ST->TAC = (1024 - ST->TA);
    } else {  // stop timer a
        ST->TAC = 0;
    }
}

/// @brief Timer A Overflow, clear or reload the counter.
///
/// @param state the operator state for which timer A is over
///
static inline void timer_A_over(OperatorState *state) {
    state->TAC = (1024 - state->TA);
}

/// @brief Timer B Overflow, clear or reload the counter.
///
/// @param state the operator state for which timer B is over
///
static inline void timer_B_over(OperatorState *state) {
    state->TBC = (256 - state->TB) << 4;
}

/// Set the key-on flag for the given voice and slot.
///
/// @param voice the voice to set the key-on flag for
/// @param slot the slot to set the key-on flag for
///
static inline void set_keyon(Voice *voice, unsigned slot) {
    Operator *SLOT = &voice->SLOT[slot];
    if (!SLOT->key) {
        SLOT->key = 1;
        // restart Phase Generator
        SLOT->phase = 0;
        SLOT->ssgn = (SLOT->ssg & 0x04) >> 1;
        SLOT->state = EG_ATT;
    }
}

/// Set the key-off flag for the given voice and slot.
///
/// @param voice the voice to set the key-off flag for
/// @param slot the slot to set the key-off flag for
///
static inline void set_keyoff(Voice *voice, unsigned slot) {
    Operator *SLOT = &voice->SLOT[slot];
    if ( SLOT->key ) {
        SLOT->key = 0;
        if (SLOT->state>EG_REL)  // phase -> Release
            SLOT->state = EG_REL;
    }
}

/// set detune & multiplier.
static inline void set_det_mul(OperatorState *ST, Voice *CH, Operator *SLOT, int v) {
    SLOT->mul = (v & 0x0f) ? (v & 0x0f) * 2 : 1;
    SLOT->DT = ST->dt_tab[(v >> 4) & 7];
    CH->SLOT[SLOT1].Incr = -1;
}

/// @brief Set the 7-bit total level.
///
/// @param CH a pointer to the channel
/// @param Operator a pointer to the operator
/// @param v the value for the TL register
///
static inline void set_tl(Voice *CH, Operator *SLOT, int v) {
    SLOT->tl = (v & 0x7f) << (ENV_BITS - 7);
}

/// set attack rate & key scale
static inline void set_ar_ksr(Voice *CH, Operator *SLOT, int v) {
    uint8_t old_KSR = SLOT->KSR;
    SLOT->ar = (v & 0x1f) ? 32 + ((v & 0x1f) << 1) : 0;
    SLOT->KSR = 3 - (v >> 6);
    if (SLOT->KSR != old_KSR) CH->SLOT[SLOT1].Incr = -1;
    // refresh Attack rate
    if ((SLOT->ar + SLOT->ksr) < 32 + 62) {
        SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar + SLOT->ksr ];
        SLOT->eg_sel_ar = eg_rate_select[SLOT->ar + SLOT->ksr ];
    } else {
        SLOT->eg_sh_ar = 0;
        SLOT->eg_sel_ar = 17 * RATE_STEPS;
    }
}

/// set decay rate
static inline void set_dr(Operator *SLOT, int v) {
    SLOT->d1r = (v & 0x1f) ? 32 + ((v & 0x1f) << 1) : 0;
    SLOT->eg_sh_d1r = eg_rate_shift [SLOT->d1r + SLOT->ksr];
    SLOT->eg_sel_d1r= eg_rate_select[SLOT->d1r + SLOT->ksr];
}

/// set sustain rate
static inline void set_sr(Operator *SLOT, int v) {
    SLOT->d2r = (v & 0x1f) ? 32 + ((v & 0x1f) << 1) : 0;
    SLOT->eg_sh_d2r = eg_rate_shift [SLOT->d2r + SLOT->ksr];
    SLOT->eg_sel_d2r= eg_rate_select[SLOT->d2r + SLOT->ksr];
}

/// set release rate
static inline void set_sl_rr(Operator *SLOT, int v) {
    SLOT->sl = sl_table[v >> 4];
    SLOT->rr = 34 + ((v & 0x0f) << 2);
    SLOT->eg_sh_rr  = eg_rate_shift [SLOT->rr + SLOT->ksr];
    SLOT->eg_sel_rr = eg_rate_select[SLOT->rr + SLOT->ksr];
}

static void reset_channels(OperatorState *ST, Voice *CH, int num) {
    // normal mode
    ST->mode   = 0;
    ST->TA     = 0;
    ST->TAC    = 0;
    ST->TB     = 0;
    ST->TBC    = 0;
    for(int c = 0; c < num; c++) {
        CH[c].fc = 0;
        for(int s = 0; s < 4; s++) {
            CH[c].SLOT[s].ssg = 0;
            CH[c].SLOT[s].ssgn = 0;
            CH[c].SLOT[s].state= EG_OFF;
            CH[c].SLOT[s].volume = MAX_ATT_INDEX;
            CH[c].SLOT[s].vol_out= MAX_ATT_INDEX;
        }
    }
}

/// SSG-EG update process
/// The behavior is based upon Nemesis tests on real hardware
/// This is actually executed before each samples
static void update_ssg_eg_channel(Operator *SLOT) {
    // four operators per channel
    for (unsigned i = 4; i > 0; i--) {
        // detect SSG-EG transition. this is not required during release phase
        // as the attenuation has been forced to MAX and output invert flag is
        // not used. If an Attack Phase is programmed, inversion can occur on
        // each sample.
        if ((SLOT->ssg & 0x08) && (SLOT->volume >= 0x200) && (SLOT->state > EG_REL)) {
            if (SLOT->ssg & 0x01) {  // bit 0 = hold SSG-EG
                // set inversion flag
                if (SLOT->ssg & 0x02) SLOT->ssgn = 4;
                // force attenuation level during decay phases
                if ((SLOT->state != EG_ATT) && !(SLOT->ssgn ^ (SLOT->ssg & 0x04)))
                    SLOT->volume  = MAX_ATT_INDEX;
            } else {  // loop SSG-EG
                // toggle output inversion flag or reset Phase Generator
                if (SLOT->ssg & 0x02)
                    SLOT->ssgn ^= 4;
                else
                    SLOT->phase = 0;
                // same as Key ON
                if (SLOT->state != EG_ATT) {
                    if ((SLOT->ar + SLOT->ksr) < 32 + 62) {
                        SLOT->state = (SLOT->volume <= MIN_ATT_INDEX) ?
                            ((SLOT->sl == MIN_ATT_INDEX) ? EG_SUS : EG_DEC) : EG_ATT;
                    } else { // Attack Rate is maximal: jump to Decay or Sustain
                        SLOT->volume = MIN_ATT_INDEX;
                        SLOT->state = (SLOT->sl == MIN_ATT_INDEX) ? EG_SUS : EG_DEC;
                    }
                }
            }
            // recalculate EG output
            if (SLOT->ssgn ^ (SLOT->ssg&0x04))
                SLOT->vol_out = ((uint32_t)(0x200 - SLOT->volume) & MAX_ATT_INDEX) + SLOT->tl;
            else
                SLOT->vol_out = (uint32_t)SLOT->volume + SLOT->tl;
        }
        // next slot
        SLOT++;
    }
}

// ---------------------------------------------------------------------------
// MARK: OPN unit
// ---------------------------------------------------------------------------

/// OPN 3slot struct
struct FM_3SLOT {
    /// fnum3,blk3: calculated
    uint32_t fc[3] = {0, 0, 0};
    /// freq3 latch
    uint8_t fn_h = 0;
    /// key code
    uint8_t kcode[3] = {0, 0, 0};
    /// current fnum value for this slot (can be different between slots of
    /// one channel in 3slot mode)
    uint32_t block_fnum[3] = {0, 0, 0};
};

/// OPN/A/B common state
struct EngineState {
    /// chip type
    uint8_t type = 0;
    /// general state
    OperatorState ST;
    /// 3 slot mode state
    FM_3SLOT SL3;
    /// pointer of CH
    Voice *P_CH = nullptr;
    /// fm channels output masks (0xffffffff = enable) */
    unsigned int pan[6 * 2];

    /// global envelope generator counter
    uint32_t eg_cnt = 0;
    /// global envelope generator counter works at frequency = chipclock/144/3
    uint32_t eg_timer = 0;
    /// step of eg_timer
    uint32_t eg_timer_add = 0;
    /// envelope generator timer overflows every 3 samples (on real chip)
    uint32_t eg_timer_overflow = 0;

    /// there are 2048 FNUMs that can be generated using FNUM/BLK registers
    /// but LFO works with one more bit of a precision so we really need 4096
    /// elements. fnumber->increment counter
    uint32_t fn_table[4096];
    /// maximal phase increment (used for phase overflow)
    uint32_t fn_max = 0;

    /// current LFO phase (out of 128)
    uint8_t lfo_cnt = 0;
    /// current LFO phase runs at LFO frequency
    uint32_t lfo_timer = 0;
    /// step of lfo_timer
    uint32_t lfo_timer_add = 0;
    /// LFO timer overflows every N samples (depends on LFO frequency)
    uint32_t lfo_timer_overflow = 0;
    /// current LFO AM step
    uint32_t LFO_AM = 0;
    /// current LFO PM step
    uint32_t LFO_PM = 0;

    /// Phase Modulation input for operator 2
    int32_t m2 = 0;
    /// Phase Modulation input for operator 3
    int32_t c1 = 0;
    /// Phase Modulation input for operator 4
    int32_t c2 = 0;

    /// one sample delay memory
    int32_t mem = 0;
    /// outputs of working channels
    int32_t out_fm[8];
};

// ---------------------------------------------------------------------------
// MARK: Functional API
// ---------------------------------------------------------------------------

/// initialize time tables
static void init_timetables(EngineState *OPN, double freqbase) {
    // DeTune table
    for (int d = 0; d <= 3; d++) {
        for (int i = 0; i <= 31; i++) {
            // -10 because chip works with 10.10 fixed point, while we use 16.16
            double rate = ((double) dt_tab[d * 32 + i]) * freqbase * (1 << (FREQ_SH - 10));
            OPN->ST.dt_tab[d][i] = (int32_t) rate;
            OPN->ST.dt_tab[d + 4][i] = -OPN->ST.dt_tab[d][i];
        }
    }
    // there are 2048 FNUMs that can be generated using FNUM/BLK registers
    // but LFO works with one more bit of a precision so we really need 4096
    // elements. calculate fnumber -> increment counter table
    for (int i = 0; i < 4096; i++) {
        // freq table for octave 7
        // OPN phase increment counter = 20bit
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
        OPN->fn_table[i] = (uint32_t)((double) i * 32 * freqbase * (1 << (FREQ_SH - 10)));
    }
    // maximal frequency is required for Phase overflow calculation, register
    // size is 17 bits (Nemesis)
    OPN->fn_max = (uint32_t)((double) 0x20000 * freqbase * (1 << (FREQ_SH - 10)));
}

/// Set pre-scaler and make time tables.
///
/// @param OPN the OPN emulator to set the pre-scaler and create timetables for
///
static void set_prescaler(EngineState *OPN) {
    // frequency base
    OPN->ST.freqbase = (OPN->ST.rate) ? ((double)OPN->ST.clock / OPN->ST.rate) : 0;
    // TODO: why is it necessary to scale these increments by a factor of 1/16
    //       to get the correct timings from the EG and LFO?
    // EG timer increment (updates every 3 samples)
    OPN->eg_timer_add = (1 << EG_SH) * OPN->ST.freqbase / 16;
    OPN->eg_timer_overflow = 3 * (1 << EG_SH) / 16;
    // LFO timer increment (updates every 16 samples)
    OPN->lfo_timer_add = (1 << LFO_SH) * OPN->ST.freqbase / 16;
    // make time tables
    init_timetables(OPN, OPN->ST.freqbase);
}

/// set algorithm connection
static void setup_connection(EngineState *OPN, Voice *CH, int ch) {
    int32_t *carrier = &OPN->out_fm[ch];

    int32_t **om1 = &CH->connect1;
    int32_t **om2 = &CH->connect3;
    int32_t **oc1 = &CH->connect2;

    int32_t **memc = &CH->mem_connect;

    switch( CH->ALGO ) {
    case 0:
        // M1---C1---MEM---M2---C2---OUT
        *om1 = &OPN->c1;
        *oc1 = &OPN->mem;
        *om2 = &OPN->c2;
        *memc= &OPN->m2;
        break;
    case 1:
        // M1------+-MEM---M2---C2---OUT
        //      C1-+
        *om1 = &OPN->mem;
        *oc1 = &OPN->mem;
        *om2 = &OPN->c2;
        *memc= &OPN->m2;
        break;
    case 2:
        // M1-----------------+-C2---OUT
        //      C1---MEM---M2-+
        *om1 = &OPN->c2;
        *oc1 = &OPN->mem;
        *om2 = &OPN->c2;
        *memc= &OPN->m2;
        break;
    case 3:
        // M1---C1---MEM------+-C2---OUT
        //                 M2-+
        *om1 = &OPN->c1;
        *oc1 = &OPN->mem;
        *om2 = &OPN->c2;
        *memc= &OPN->c2;
        break;
    case 4:
        // M1---C1-+-OUT
        // M2---C2-+
        // MEM: not used
        *om1 = &OPN->c1;
        *oc1 = carrier;
        *om2 = &OPN->c2;
        *memc= &OPN->mem;  // store it anywhere where it will not be used
        break;
    case 5:
        //    +----C1----+
        // M1-+-MEM---M2-+-OUT
        //    +----C2----+
        *om1 = nullptr;  // special mark
        *oc1 = carrier;
        *om2 = carrier;
        *memc= &OPN->m2;
        break;
    case 6:
        // M1---C1-+
        //      M2-+-OUT
        //      C2-+
        // MEM: not used
        *om1 = &OPN->c1;
        *oc1 = carrier;
        *om2 = carrier;
        *memc= &OPN->mem;  // store it anywhere where it will not be used
        break;
    case 7:
        // M1-+
        // C1-+-OUT
        // M2-+
        // C2-+
        // MEM: not used
        *om1 = carrier;
        *oc1 = carrier;
        *om2 = carrier;
        *memc= &OPN->mem;  // store it anywhere where it will not be used
        break;
    }
    CH->connect4 = carrier;
}

/// advance LFO to next sample.
static inline void advance_lfo(EngineState *OPN) {
    if (OPN->lfo_timer_overflow) {  // LFO enabled ?
        // increment LFO timer
        OPN->lfo_timer +=  OPN->lfo_timer_add;
        // when LFO is enabled, one level will last for
        // 108, 77, 71, 67, 62, 44, 8 or 5 samples
        while (OPN->lfo_timer >= OPN->lfo_timer_overflow) {
            OPN->lfo_timer -= OPN->lfo_timer_overflow;
            // There are 128 LFO steps
            OPN->lfo_cnt = ( OPN->lfo_cnt + 1 ) & 127;
            // triangle (inverted)
            // AM: from 126 to 0 step -2, 0 to 126 step +2
            if (OPN->lfo_cnt<64)
                OPN->LFO_AM = (OPN->lfo_cnt ^ 63) << 1;
            else
                OPN->LFO_AM = (OPN->lfo_cnt & 63) << 1;
            // PM works with 4 times slower clock
            OPN->LFO_PM = OPN->lfo_cnt >> 2;
        }
    }
}

static inline void advance_eg_channel(EngineState *OPN, Operator *SLOT) {
    // four operators per channel
    for (unsigned i = 4; i > 0; i--) {  // reset SSG-EG swap flag
        unsigned int swap_flag = 0;
        switch(SLOT->state) {
        case EG_ATT:  // attack phase
            if (!(OPN->eg_cnt & ((1 << SLOT->eg_sh_ar) - 1))) {
                SLOT->volume += (~SLOT->volume * (eg_inc[SLOT->eg_sel_ar + ((OPN->eg_cnt>>SLOT->eg_sh_ar) & 7)])) >> 4;
                if (SLOT->volume <= MIN_ATT_INDEX) {
                    SLOT->volume = MIN_ATT_INDEX;
                    SLOT->state = EG_DEC;
                }
            }
            break;
        case EG_DEC:  // decay phase
            if (SLOT->ssg & 0x08) {  // SSG EG type envelope selected
                if (!(OPN->eg_cnt & ((1 << SLOT->eg_sh_d1r) - 1))) {
                    SLOT->volume += 4 * eg_inc[SLOT->eg_sel_d1r + ((OPN->eg_cnt>>SLOT->eg_sh_d1r) & 7)];
                    if ( SLOT->volume >= static_cast<int32_t>(SLOT->sl) )
                        SLOT->state = EG_SUS;
                }
            } else {
                if (!(OPN->eg_cnt & ((1 << SLOT->eg_sh_d1r) - 1))) {
                    SLOT->volume += eg_inc[SLOT->eg_sel_d1r + ((OPN->eg_cnt>>SLOT->eg_sh_d1r) & 7)];
                    if (SLOT->volume >= static_cast<int32_t>(SLOT->sl))
                        SLOT->state = EG_SUS;
                }
            }
            break;
        case EG_SUS:  // sustain phase
            if (SLOT->ssg & 0x08) {  // SSG EG type envelope selected
                if (!(OPN->eg_cnt & ((1 << SLOT->eg_sh_d2r) - 1))) {
                    SLOT->volume += 4 * eg_inc[SLOT->eg_sel_d2r + ((OPN->eg_cnt>>SLOT->eg_sh_d2r) & 7)];
                    if (SLOT->volume >= ENV_QUIET) {
                        SLOT->volume = MAX_ATT_INDEX;
                        if (SLOT->ssg & 0x01) {  // bit 0 = hold
                            if (SLOT->ssgn & 1) {  // have we swapped once ???
                                // yes, so do nothing, just hold current level
                            } else {  // bit 1 = alternate
                                swap_flag = (SLOT->ssg & 0x02) | 1;
                            }
                        } else {  // same as KEY-ON operation
                            // restart of the Phase Generator should be here
                            SLOT->phase = 0;
                            // phase -> Attack
                            SLOT->volume = 511;
                            SLOT->state = EG_ATT;
                            // bit 1 = alternate
                            swap_flag = (SLOT->ssg & 0x02);
                        }
                    }
                }
            } else {
                if (!(OPN->eg_cnt & ((1 << SLOT->eg_sh_d2r) - 1))) {
                    SLOT->volume += eg_inc[SLOT->eg_sel_d2r + ((OPN->eg_cnt>>SLOT->eg_sh_d2r) & 7)];
                    if (SLOT->volume >= MAX_ATT_INDEX) {
                        SLOT->volume = MAX_ATT_INDEX;
                        // do not change SLOT->state (verified on real chip)
                    }
                }
            }
            break;
        case EG_REL:  // release phase
            if (!(OPN->eg_cnt & ((1 << SLOT->eg_sh_rr) - 1))) {
                // SSG-EG affects Release phase also (Nemesis)
                SLOT->volume += eg_inc[SLOT->eg_sel_rr + ((OPN->eg_cnt>>SLOT->eg_sh_rr) & 7)];
                if (SLOT->volume >= MAX_ATT_INDEX) {
                    SLOT->volume = MAX_ATT_INDEX;
                    SLOT->state = EG_OFF;
                }
            }
            break;
        }
        // get the output volume from the slot
        unsigned int out = static_cast<uint32_t>(SLOT->volume);
        // negate output (changes come from alternate bit, init comes from
        // attack bit)
        if ((SLOT->ssg & 0x08) && (SLOT->ssgn & 2) && (SLOT->state > EG_REL))
            out ^= MAX_ATT_INDEX;
        // we need to store the result here because we are going to change
        // ssgn in next instruction
        SLOT->vol_out = out + SLOT->tl;
        // reverse SLOT inversion flag
        SLOT->ssgn ^= swap_flag;
        // increment the slot and decrement the iterator
        SLOT++;
    }
}

static inline void update_phase_lfo_channel(EngineState *OPN, Voice *CH) {
    uint32_t block_fnum = CH->block_fnum;
    uint32_t fnum_lfo  = ((block_fnum & 0x7f0) >> 4) * 32 * 8;
    int32_t  lfo_fn_table_index_offset = lfo_pm_table[fnum_lfo + CH->pms + OPN->LFO_PM];
    if (lfo_fn_table_index_offset) {  // LFO phase modulation active
        block_fnum = block_fnum * 2 + lfo_fn_table_index_offset;
        uint8_t blk = (block_fnum & 0x7000) >> 12;
        uint32_t fn = block_fnum & 0xfff;
        // key-scale code
        int kc = (blk << 2) | opn_fktable[fn >> 8];
        // phase increment counter
        int fc = (OPN->fn_table[fn]>>(7 - blk));
        // detects frequency overflow (credits to Nemesis)
        int finc = fc + CH->SLOT[SLOT1].DT[kc];
        // Operator 1
        if (finc < 0) finc += OPN->fn_max;
        CH->SLOT[SLOT1].phase += (finc * CH->SLOT[SLOT1].mul) >> 1;
        // Operator 2
        finc = fc + CH->SLOT[SLOT2].DT[kc];
        if (finc < 0) finc += OPN->fn_max;
        CH->SLOT[SLOT2].phase += (finc * CH->SLOT[SLOT2].mul) >> 1;
        // Operator 3
        finc = fc + CH->SLOT[SLOT3].DT[kc];
        if (finc < 0) finc += OPN->fn_max;
        CH->SLOT[SLOT3].phase += (finc * CH->SLOT[SLOT3].mul) >> 1;
        // Operator 4
        finc = fc + CH->SLOT[SLOT4].DT[kc];
        if (finc < 0) finc += OPN->fn_max;
        CH->SLOT[SLOT4].phase += (finc * CH->SLOT[SLOT4].mul) >> 1;
    } else {  // LFO phase modulation is 0
        CH->SLOT[SLOT1].phase += CH->SLOT[SLOT1].Incr;
        CH->SLOT[SLOT2].phase += CH->SLOT[SLOT2].Incr;
        CH->SLOT[SLOT3].phase += CH->SLOT[SLOT3].Incr;
        CH->SLOT[SLOT4].phase += CH->SLOT[SLOT4].Incr;
    }
}

static inline void chan_calc(EngineState *OPN, Voice *CH) {
#define CALCULATE_VOLUME(OP) ((OP)->vol_out + (AM & (OP)->AMmask))
    uint32_t AM = OPN->LFO_AM >> CH->ams;
    OPN->m2 = OPN->c1 = OPN->c2 = OPN->mem = 0;
    // restore delayed sample (MEM) value to m2 or c2
    *CH->mem_connect = CH->mem_value;
    // SLOT 1
    unsigned int eg_out = CALCULATE_VOLUME(&CH->SLOT[SLOT1]);
    int32_t out = CH->op1_out[0] + CH->op1_out[1];
    CH->op1_out[0] = CH->op1_out[1];
    if (!CH->connect1) {  // algorithm 5
        OPN->mem = OPN->c1 = OPN->c2 = CH->op1_out[0];
    } else {  // other algorithms
        *CH->connect1 += CH->op1_out[0];
    }
    CH->op1_out[1] = 0;
    if (eg_out < ENV_QUIET) {
        if (!CH->FB) out = 0;
        CH->op1_out[1] = op_calc1(CH->SLOT[SLOT1].phase, eg_out, (out << CH->FB) );
    }
    // SLOT 3
    eg_out = CALCULATE_VOLUME(&CH->SLOT[SLOT3]);
    if (eg_out < ENV_QUIET)
        *CH->connect3 += op_calc(CH->SLOT[SLOT3].phase, eg_out, OPN->m2);
    // SLOT 2
    eg_out = CALCULATE_VOLUME(&CH->SLOT[SLOT2]);
    if (eg_out < ENV_QUIET)
        *CH->connect2 += op_calc(CH->SLOT[SLOT2].phase, eg_out, OPN->c1);
    // SLOT 4
    eg_out = CALCULATE_VOLUME(&CH->SLOT[SLOT4]);
    if (eg_out < ENV_QUIET)
        *CH->connect4 += op_calc(CH->SLOT[SLOT4].phase, eg_out, OPN->c2);
    // store current MEM
    CH->mem_value = OPN->mem;
    // update phase counters AFTER output calculations
    if (CH->pms) {
        update_phase_lfo_channel(OPN, CH);
    } else {  // no LFO phase modulation
        CH->SLOT[SLOT1].phase += CH->SLOT[SLOT1].Incr;
        CH->SLOT[SLOT2].phase += CH->SLOT[SLOT2].Incr;
        CH->SLOT[SLOT3].phase += CH->SLOT[SLOT3].Incr;
        CH->SLOT[SLOT4].phase += CH->SLOT[SLOT4].Incr;
    }
#undef CALCULATE_VOLUME
}

/// update phase increment and envelope generator
static inline void refresh_fc_eg_slot(EngineState *OPN, Operator *SLOT, int fc, int kc) {
    int ksr = kc >> SLOT->KSR;
    fc += SLOT->DT[kc];
    // detects frequency overflow (credits to Nemesis)
    if (fc < 0) fc += OPN->fn_max;
    // (frequency) phase increment counter
    SLOT->Incr = (fc * SLOT->mul) >> 1;
    if ( SLOT->ksr != ksr ) {
        SLOT->ksr = ksr;
        // calculate envelope generator rates
        if ((SLOT->ar + SLOT->ksr) < 32+62) {
            SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar  + SLOT->ksr ];
            SLOT->eg_sel_ar = eg_rate_select[SLOT->ar  + SLOT->ksr ];
        } else {
            SLOT->eg_sh_ar  = 0;
            SLOT->eg_sel_ar = 17*RATE_STEPS;
        }

        SLOT->eg_sh_d1r = eg_rate_shift [SLOT->d1r + SLOT->ksr];
        SLOT->eg_sh_d2r = eg_rate_shift [SLOT->d2r + SLOT->ksr];
        SLOT->eg_sh_rr  = eg_rate_shift [SLOT->rr  + SLOT->ksr];

        SLOT->eg_sel_d1r= eg_rate_select[SLOT->d1r + SLOT->ksr];
        SLOT->eg_sel_d2r= eg_rate_select[SLOT->d2r + SLOT->ksr];
        SLOT->eg_sel_rr = eg_rate_select[SLOT->rr  + SLOT->ksr];
    }
}

/// update phase increment counters
static inline void refresh_fc_eg_chan(EngineState *OPN, Voice *CH) {
    if ( CH->SLOT[SLOT1].Incr==-1) {
        int fc = CH->fc;
        int kc = CH->kcode;
        refresh_fc_eg_slot(OPN, &CH->SLOT[SLOT1] , fc , kc );
        refresh_fc_eg_slot(OPN, &CH->SLOT[SLOT2] , fc , kc );
        refresh_fc_eg_slot(OPN, &CH->SLOT[SLOT3] , fc , kc );
        refresh_fc_eg_slot(OPN, &CH->SLOT[SLOT4] , fc , kc );
    }
}

/// write a OPN mode register 0x20-0x2f.
static void write_mode(EngineState *OPN, int r, int v) {
    switch (r) {
    case 0x21:  // Test
        break;
    case 0x22:  // LFO FREQ (YM2608/YM2610/YM2610B/YM2612)
        if (v & 8) {  // LFO enabled ?
            OPN->lfo_timer_overflow = lfo_samples_per_step[v&7] << LFO_SH;
        } else {
            // hold LFO waveform in reset state
            OPN->lfo_timer_overflow = 0;
            OPN->lfo_timer = 0;
            OPN->lfo_cnt   = 0;
            OPN->LFO_PM    = 0;
            OPN->LFO_AM    = 126;
        }
        break;
    case 0x24:  // timer A High 8
        OPN->ST.TA = (OPN->ST.TA & 0x0003) | (v << 2);
        break;
    case 0x25:  // timer A Low 2
        OPN->ST.TA = (OPN->ST.TA & 0x03fc) | (v & 3);
        break;
    case 0x26:  // timer B
        OPN->ST.TB = v;
        break;
    case 0x27:  // mode, timer control
        set_timers(&(OPN->ST), v);
        break;
    case 0x28:  // key on / off
        uint8_t c = v & 0x03;
        if (c == 3) break;
        if ((v & 0x04) && (OPN->type & TYPE_6CH)) c += 3;
        Voice* CH = OPN->P_CH;
        CH = &CH[c];
        if (v & 0x10) set_keyon(CH, SLOT1); else set_keyoff(CH, SLOT1);
        if (v & 0x20) set_keyon(CH, SLOT2); else set_keyoff(CH, SLOT2);
        if (v & 0x40) set_keyon(CH, SLOT3); else set_keyoff(CH, SLOT3);
        if (v & 0x80) set_keyon(CH, SLOT4); else set_keyoff(CH, SLOT4);
        break;
    }
}

/// write a OPN register (0x30-0xff).
static void write_register(EngineState *OPN, int r, int v) {
    uint8_t c = OPN_CHAN(r);
    // 0xX3, 0xX7, 0xXB, 0xXF
    if (c == 3) return;
    if (r >= 0x100) c+=3;
    // get the channel
    Voice* const CH = &OPN->P_CH[c];
    // get the operator
    Operator* const SLOT = &(CH->SLOT[OPN_SLOT(r)]);
    switch (r & 0xf0) {
    case 0x30:  // DET, MUL
        set_det_mul(&OPN->ST, CH, SLOT, v);
        break;
    case 0x40:  // TL
        set_tl(CH, SLOT, v);
        break;
    case 0x50:  // KS, AR
        set_ar_ksr(CH, SLOT, v);
        break;
    case 0x60:  // bit7 = AM ENABLE, DR
        set_dr(SLOT, v);
        if (OPN->type & TYPE_LFOPAN)  // YM2608/2610/2610B/2612
            SLOT->AMmask = (v & 0x80) ? ~0 : 0;
        break;
    case 0x70:  // SR
        set_sr(SLOT, v);
        break;
    case 0x80:  // SL, RR
        set_sl_rr(SLOT, v);
        break;
    case 0x90:  // SSG-EG
        SLOT->ssg  =  v&0x0f;
        // recalculate EG output
        if ((SLOT->ssg & 0x08) && (SLOT->ssgn ^ (SLOT->ssg & 0x04)) && (SLOT->state > EG_REL))
            SLOT->vol_out = ((uint32_t) (0x200 - SLOT->volume) & MAX_ATT_INDEX) + SLOT->tl;
        else
            SLOT->vol_out = (uint32_t) SLOT->volume + SLOT->tl;
        break;
    case 0xa0:
        switch (OPN_SLOT(r)) {
        case 0:  {  // 0xa0-0xa2 : FNUM1
            uint32_t fn = (((uint32_t)( (OPN->ST.fn_h) & 7)) << 8) + v;
            uint8_t blk = OPN->ST.fn_h >> 3;
            /* key-scale code */
            CH->kcode = (blk << 2) | opn_fktable[(fn >> 7) & 0xf];
            /* phase increment counter */
            CH->fc = OPN->fn_table[fn * 2] >> (7 - blk);
            /* store fnum in clear form for LFO PM calculations */
            CH->block_fnum = (blk << 11) | fn;
            CH->SLOT[SLOT1].Incr = -1;
            break;
        }
        case 1:  // 0xa4-0xa6 : FNUM2,BLK
            OPN->ST.fn_h = v&0x3f;
            break;
        case 2:  // 0xa8-0xaa : 3CH FNUM1
            if (r < 0x100) {
                uint32_t fn = (((uint32_t)(OPN->SL3.fn_h & 7)) << 8) + v;
                uint8_t blk = OPN->SL3.fn_h >> 3;
                /* keyscale code */
                OPN->SL3.kcode[c]= (blk << 2) | opn_fktable[(fn >> 7) & 0xf];
                /* phase increment counter */
                OPN->SL3.fc[c] = OPN->fn_table[fn * 2] >> (7 - blk);
                OPN->SL3.block_fnum[c] = (blk << 11) | fn;
                (OPN->P_CH)[2].SLOT[SLOT1].Incr = -1;
            }
            break;
        case 3:  // 0xac-0xae : 3CH FNUM2, BLK
            if (r < 0x100)
                OPN->SL3.fn_h = v&0x3f;
            break;
        }
        break;
    case 0xb0:
        switch (OPN_SLOT(r)) {
        case 0: {  // 0xb0-0xb2 : FB,ALGO
            int feedback = (v >> 3) & 7;
            CH->ALGO = v & 7;
            CH->FB = feedback ? feedback + 6 : 0;
            setup_connection(OPN, CH, c);
            break;
        }
        case 1:  // 0xb4-0xb6 : L, R, AMS, PMS (YM2612/YM2610B/YM2610/YM2608)
            if (OPN->type & TYPE_LFOPAN) {
                // b0-2 PMS
                // CH->pms = PM depth * 32 (index in lfo_pm_table)
                CH->pms = (v & 7) * 32;
                // b4-5 AMS
                CH->ams = lfo_ams_depth_shift[(v >> 4) & 0x03];
                // PAN :  b7 = L, b6 = R
                OPN->pan[c * 2    ] = (v & 0x80) ? ~0 : 0;
                OPN->pan[c * 2 + 1] = (v & 0x40) ? ~0 : 0;
            }
            break;
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// MARK: Object-Oriented API
// ---------------------------------------------------------------------------

/// Yamaha YM2612 chip emulator
class YamahaYM2612 {
 private:
    /// registers
    uint8_t registers[512];
    /// OPN state
    EngineState OPN;
    /// channel state
    Voice CH[6];
    /// address line A1
    uint8_t addr_A1;

    /// whether the emulated DAC is enabled
    bool is_DAC_enabled;
    /// the output value from the emulated DAC
    int32_t out_DAC;

    /// A structure with channel data for a YM2612 voice.
    struct Channel {
        /// the index of the active FM algorithm
        uint8_t AL = 0;
        /// the amount of feedback being applied to operator 1
        uint8_t FB = 0;
        /// the attenuator (and switch) for amplitude modulation from the LFO
        uint8_t AMS = 0;
        /// the attenuator (and switch) for frequency modulation from the LFO
        uint8_t FMS = 0;
        /// the four FM operators for the channel
        struct Operator {
            /// the attack rate
            uint8_t AR = 0;
            /// the 1st decay stage rate
            uint8_t D1 = 0;
            /// the amplitude to start the second decay stage
            uint8_t SL = 0;
            /// the 2nd decay stage rate
            uint8_t D2 = 0;
            /// the release rate
            uint8_t RR = 0;
            /// the total amplitude of the envelope
            uint8_t TL = 0;
            /// the multiplier for the FM frequency
            uint8_t MUL = 0;
            /// the amount of detuning to apply (DET * epsilon + frequency)
            uint8_t DET = 0;
            /// the Rate scale for key-tracking the envelope rates
            uint8_t RS = 0;
            /// whether amplitude modulation from the LFO enabled
            uint8_t AM = 0;
            /// the SSG mode for the operator
            uint8_t SSG = 0;
        } operators[4];
    } channels[6];

    /// the value of the global LFO parameter
    uint8_t LFO = 0;

    /// master output left
    int16_t MOL = 0;
    /// master output right
    int16_t MOR = 0;

 public:
    /// @brief Initialize a new YamahaYM2612 with given sample rate.
    ///
    /// @param clock_rate the underlying clock rate of the system
    /// @param sample_rate the rate to draw samples from the emulator at
    ///
    YamahaYM2612(double clock_rate = 768000, double sample_rate = 44100) {
        OPN.P_CH = CH;
        OPN.type = TYPE_YM2612;
        OPN.ST.clock = clock_rate;
        OPN.ST.rate = sample_rate;
        reset();
    }

    /// @brief Set the sample rate the a new value.
    ///
    /// @param clock_rate the underlying clock rate of the system
    /// @param sample_rate the rate to draw samples from the emulator at
    ///
    void setSampleRate(double clock_rate, double sample_rate) {
        OPN.ST.clock = clock_rate;
        OPN.ST.rate = sample_rate;
        set_prescaler(&OPN);
    }

    /// @brief Reset the emulator to its initial state
    void reset() {
        // clear instance variables
        memset(registers, 0, sizeof registers);
        LFO = MOL = MOR = 0;
        // set the frequency scaling parameters of the OPN emulator
        set_prescaler(&OPN);
        // mode 0 , timer reset
        write_mode(&OPN, 0x27, 0x30);
        // envelope generator
        OPN.eg_timer = 0;
        OPN.eg_cnt = 0;
        // LFO
        OPN.lfo_timer = 0;
        OPN.lfo_cnt = 0;
        OPN.LFO_AM = 126;
        OPN.LFO_PM = 0;
        // state
        OPN.ST.status = 0;
        OPN.ST.mode = 0;

        write_mode(&OPN, 0x27, 0x30);
        write_mode(&OPN, 0x26, 0x00);
        write_mode(&OPN, 0x25, 0x00);
        write_mode(&OPN, 0x24, 0x00);

        reset_channels(&(OPN.ST), &CH[0], 6);

        for (int i = 0xb6; i >= 0xb4; i--) {
            write_register(&OPN, i, 0xc0);
            write_register(&OPN, i | 0x100, 0xc0);
        }

        for (int i = 0xb2; i >= 0x30; i--) {
            write_register(&OPN, i, 0);
            write_register(&OPN, i | 0x100, 0);
        }

        // DAC mode clear
        is_DAC_enabled = 0;
        out_DAC = 0;
        for (int c = 0; c < 6; c++) setST(c, 3);
    }

    /// @brief Run a step on the emulator
    void step() {
        int lt, rt;
        // refresh PG and EG
        refresh_fc_eg_chan(&OPN, &CH[0]);
        refresh_fc_eg_chan(&OPN, &CH[1]);
        refresh_fc_eg_chan(&OPN, &CH[2]);
        refresh_fc_eg_chan(&OPN, &CH[3]);
        refresh_fc_eg_chan(&OPN, &CH[4]);
        refresh_fc_eg_chan(&OPN, &CH[5]);
        // clear outputs
        OPN.out_fm[0] = 0;
        OPN.out_fm[1] = 0;
        OPN.out_fm[2] = 0;
        OPN.out_fm[3] = 0;
        OPN.out_fm[4] = 0;
        OPN.out_fm[5] = 0;
        // update SSG-EG output
        update_ssg_eg_channel(&(CH[0].SLOT[SLOT1]));
        update_ssg_eg_channel(&(CH[1].SLOT[SLOT1]));
        update_ssg_eg_channel(&(CH[2].SLOT[SLOT1]));
        update_ssg_eg_channel(&(CH[3].SLOT[SLOT1]));
        update_ssg_eg_channel(&(CH[4].SLOT[SLOT1]));
        update_ssg_eg_channel(&(CH[5].SLOT[SLOT1]));
        // calculate FM
        chan_calc(&OPN, &CH[0]);
        chan_calc(&OPN, &CH[1]);
        chan_calc(&OPN, &CH[2]);
        chan_calc(&OPN, &CH[3]);
        chan_calc(&OPN, &CH[4]);
        if (is_DAC_enabled)
            *&CH[5].connect4 += out_DAC;
        else
            chan_calc(&OPN, &CH[5]);
        // advance LFO
        advance_lfo(&OPN);
        // advance envelope generator
        OPN.eg_timer += OPN.eg_timer_add;
        while (OPN.eg_timer >= OPN.eg_timer_overflow) {
            OPN.eg_timer -= OPN.eg_timer_overflow;
            OPN.eg_cnt++;
            advance_eg_channel(&OPN, &(CH[0].SLOT[SLOT1]));
            advance_eg_channel(&OPN, &(CH[1].SLOT[SLOT1]));
            advance_eg_channel(&OPN, &(CH[2].SLOT[SLOT1]));
            advance_eg_channel(&OPN, &(CH[3].SLOT[SLOT1]));
            advance_eg_channel(&OPN, &(CH[4].SLOT[SLOT1]));
            advance_eg_channel(&OPN, &(CH[5].SLOT[SLOT1]));
        }
        // clip outputs
        if (OPN.out_fm[0] > 8191)
            OPN.out_fm[0] = 8191;
        else if (OPN.out_fm[0] < -8192)
            OPN.out_fm[0] = -8192;
        if (OPN.out_fm[1] > 8191)
            OPN.out_fm[1] = 8191;
        else if (OPN.out_fm[1] < -8192)
            OPN.out_fm[1] = -8192;
        if (OPN.out_fm[2] > 8191)
            OPN.out_fm[2] = 8191;
        else if (OPN.out_fm[2] < -8192)
            OPN.out_fm[2] = -8192;
        if (OPN.out_fm[3] > 8191)
            OPN.out_fm[3] = 8191;
        else if (OPN.out_fm[3] < -8192)
            OPN.out_fm[3] = -8192;
        if (OPN.out_fm[4] > 8191)
            OPN.out_fm[4] = 8191;
        else if (OPN.out_fm[4] < -8192)
            OPN.out_fm[4] = -8192;
        if (OPN.out_fm[5] > 8191)
            OPN.out_fm[5] = 8191;
        else if (OPN.out_fm[5] < -8192)
            OPN.out_fm[5] = -8192;
        // 6-channels mixing
        lt  = ((OPN.out_fm[0] >> 0) & OPN.pan[0]);
        rt  = ((OPN.out_fm[0] >> 0) & OPN.pan[1]);
        lt += ((OPN.out_fm[1] >> 0) & OPN.pan[2]);
        rt += ((OPN.out_fm[1] >> 0) & OPN.pan[3]);
        lt += ((OPN.out_fm[2] >> 0) & OPN.pan[4]);
        rt += ((OPN.out_fm[2] >> 0) & OPN.pan[5]);
        lt += ((OPN.out_fm[3] >> 0) & OPN.pan[6]);
        rt += ((OPN.out_fm[3] >> 0) & OPN.pan[7]);
        lt += ((OPN.out_fm[4] >> 0) & OPN.pan[8]);
        rt += ((OPN.out_fm[4] >> 0) & OPN.pan[9]);
        lt += ((OPN.out_fm[5] >> 0) & OPN.pan[10]);
        rt += ((OPN.out_fm[5] >> 0) & OPN.pan[11]);
        // output buffering
        MOL = lt;
        MOR = rt;
        // timer A control
        if ((OPN.ST.TAC -= static_cast<int>(OPN.ST.freqbase * 4096)) <= 0)
            timer_A_over(&OPN.ST);
        // timer B control
        if ((OPN.ST.TBC -= static_cast<int>(OPN.ST.freqbase * 4096)) <= 0)
            timer_B_over(&OPN.ST);
    }

    /// @brief Write data to a register on the chip.
    ///
    /// @param address the address of the register to write data to
    /// @param data the value of the data to write to the register
    ///
    void write(uint8_t address, uint8_t data) {
        switch (address & 3) {
        case 0:  // address port 0
            OPN.ST.address = data;
            addr_A1 = 0;
            break;
        case 1:  // data port 0
            // verified on real YM2608
            if (addr_A1 != 0) break;
            // get the address from the latch and write the data
            address = OPN.ST.address;
            registers[address] = data;
            switch (address & 0xf0) {
            case 0x20:  // 0x20-0x2f Mode
                switch (address) {
                case 0x2a:  // DAC data (YM2612), level unknown
                    out_DAC = ((int) data - 0x80) << 6;
                    break;
                case 0x2b:  // DAC Sel (YM2612), b7 = dac enable
                    is_DAC_enabled = data & 0x80;
                    break;
                default:  // OPN section, write register
                    write_mode(&OPN, address, data);
                }
                break;
            default:  // 0x30-0xff OPN section, write register
                write_register(&OPN, address, data);
            }
            break;
        case 2:  // address port 1
            OPN.ST.address = data;
            addr_A1 = 1;
            break;
        case 3:  // data port 1
            // verified on real YM2608
            if (addr_A1 != 1) break;
            // get the address from the latch and right to the given register
            address = OPN.ST.address;
            registers[address | 0x100] = data;
            write_register(&OPN, address | 0x100, data);
            break;
        }
    }

    /// @brief Set part of a 16-bit register to a given 8-bit value.
    ///
    /// @param part the part of the register space to access, 0=latch, 1=data
    /// @param reg the address of the register to write data to
    /// @param data the value of the data to write to the register
    ///
    /// @details
    ///
    /// ## [Memory map](http://www.smspower.org/maxim/Documents/YM2612#reg27)
    ///
    /// | REG  | Bit 7           | Bit 6 | Bit 5            | Bit 4   | Bit 3      | Bit 2          | Bit 1        | Bit 0  |
    /// |:-----|:----------------|:------|:-----------------|:--------|:-----------|:---------------|:-------------|:-------|
    /// | 22H  |                 |       |                  |         | LFO enable | LFO frequency  |              |        |
    /// | 24H  | Timer A MSBs    |       |                  |         |            |                |              |        |
    /// | 25H  |                 |       |                  |         |            |                | Timer A LSBs |        |
    /// | 26H  | Timer B         |       |                  |         |            |                |              |        |
    /// | 27H  | Ch3 mode        |       | Reset B          | Reset A | Enable B   | Enable A       | Load B       | Load A |
    /// | 28H  | Operator        |       |                  |         |            | Channel        |              |        |
    /// | 29H  |                 |       |                  |         |            |                |              |        |
    /// | 2AH  | DAC             |       |                  |         |            |                |              |        |
    /// | 2BH  | DAC en          |       |                  |         |            |                |              |        |
    /// |      |                 |       |                  |         |            |                |              |        |
    /// | 30H+ |                 | DT1   |                  |         | MUL        |                |              |        |
    /// | 40H+ |                 | TL    |                  |         |            |                |              |        |
    /// | 50H+ | RS              |       |                  | AR      |            |                |              |        |
    /// | 60H+ | AM              |       |                  | D1R     |            |                |              |        |
    /// | 70H+ |                 |       |                  | D2R     |            |                |              |        |
    /// | 80H+ | D1L             |       |                  |         | RR         |                |              |        |
    /// | 90H+ |                 |       |                  |         | SSG-EG     |                |              |        |
    /// |      |                 |       |                  |         |            |                |              |        |
    /// | A0H+ | Freq. LSB       |       |                  |         |            |                |              |        |
    /// | A4H+ |                 |       | Block            |         |            | Freq. MSB      |              |        |
    /// | A8H+ | Ch3 suppl. freq.|       |                  |         |            |                |              |        |
    /// | ACH+ |                 |       | Ch3 suppl. block |         |            | Ch3 suppl freq |              |        |
    /// | B0H+ |                 |       | Feedback         |         |            | Algorithm      |              |        |
    /// | B4H+ | L               | R     | AMS              |         |            | FMS            |              |        |
    ///
    inline void setREG(uint8_t part, uint8_t reg, uint8_t data) {
        write(part << 1, reg);
        write((part << 1) + 1, data);
    }

    /// @brief Set the global LFO for the chip.
    ///
    /// @param value the value of the LFO register
    /// @details
    /// ## Mapping values to frequencies in Hz
    /// | value | LFO frequency (Hz)
    /// |:------|:-------------------|
    /// | 0     | 3.98
    /// | 1     | 5.56
    /// | 2     | 6.02
    /// | 3     | 6.37
    /// | 4     | 6.88
    /// | 5     | 9.63
    /// | 6     | 48.1
    /// | 7     | 72.2
    ///
    inline void setLFO(uint8_t value) {
        // don't set the value if it hasn't changed
        if (LFO == value) return;
        // update the local LFO value
        LFO = value;
        // set the LFO on the OPN emulator
        setREG(0, 0x22, ((value > 0) << 3) | (value & 7));
    }

    // -----------------------------------------------------------------------
    // MARK: Global control for each channel
    // -----------------------------------------------------------------------

    /// @brief Set the frequency for the given channel.
    ///
    /// @param channel the voice on the chip to set the frequency for
    /// @param frequency the frequency value measured in Hz
    ///
    inline void setFREQ(uint8_t channel, float frequency) {
        // shift the frequency to the base octave and calculate the octave to play.
        // the base octave is defined as a 10-bit number, i.e., in [0, 1023]
        int octave = 2;
        for (; frequency >= 1024; octave++) frequency /= 2;
        // NOTE: arbitrary shift calculated by producing note from a ground truth
        //       oscillator and comparing the output from YM2612 via division.
        //       1.458166333006277
        // TODO: why is this arbitrary shift necessary to tune to C4?
        frequency = frequency / 1.458;
        // cast the shifted frequency to a 16-bit container
        const uint16_t freq16bit = frequency;
        // write the low and high portions of the frequency to the register
        const auto freqHigh = ((freq16bit >> 8) & 0x07) | ((octave & 0x07) << 3);
        setREG(YM_CH_PART(channel), YM_CH_OFFSET(0xA4, channel), freqHigh);
        const auto freqLow = freq16bit & 0xff;
        setREG(YM_CH_PART(channel), YM_CH_OFFSET(0xA0, channel), freqLow);
    }

    /// @brief Set the gate for the given channel.
    ///
    /// @param channel the voice on the chip to set the gate for
    /// @param value the boolean value of the gate signal
    ///
    inline void setGATE(uint8_t channel, uint8_t value) {
        // set the gate register based on the value. False = x00 and True = 0xF0
        setREG(0, 0x28, (static_cast<bool>(value) * 0xF0) + ((channel / 3) * 4 + channel % 3));
    }

    /// @brief Set the algorithm (AL) register for the given channel.
    ///
    /// @param channel the channel to set the algorithm register of
    /// @param value the selected FM algorithm in [0, 7]
    ///
    inline void setAL(uint8_t channel, uint8_t value) {
        if (channels[channel].AL == value) return;
        channels[channel].AL = value;
        CH[channel].FB_ALG = (CH[channel].FB_ALG & 0x38) | (value & 7);
        setREG(YM_CH_PART(channel), YM_CH_OFFSET(0xB0, channel), CH[channel].FB_ALG);
    }

    /// @brief Set the feedback (FB) register for the given channel.
    ///
    /// @param channel the channel to set the feedback register of
    /// @param value the amount of feedback for operator 1
    ///
    inline void setFB(uint8_t channel, uint8_t value) {
        if (channels[channel].FB == value) return;
        channels[channel].FB = value;
        CH[channel].FB_ALG = (CH[channel].FB_ALG & 7)| ((value & 7) << 3);
        setREG(YM_CH_PART(channel), YM_CH_OFFSET(0xB0, channel), CH[channel].FB_ALG);
    }

    /// @brief Set the state (ST) register for the given channel.
    ///
    /// @param channel the channel to set the state register of
    /// @param value the value of the state register
    ///
    inline void setST(uint8_t channel, uint8_t value) {
        CH[channel].LR_AMS_FMS = (CH[channel].LR_AMS_FMS & 0x3F)| ((value & 3) << 6);
        setREG(YM_CH_PART(channel), YM_CH_OFFSET(0xB4, channel), CH[channel].LR_AMS_FMS);
    }

    /// @brief Set the AM sensitivity (AMS) register for the given channel.
    ///
    /// @param channel the channel to set the AM sensitivity register of
    /// @param value the amount of amplitude modulation (AM) sensitivity
    ///
    inline void setAMS(uint8_t channel, uint8_t value) {
        if (channels[channel].AMS == value) return;
        channels[channel].AMS = value;
        CH[channel].LR_AMS_FMS = (CH[channel].LR_AMS_FMS & 0xCF)| ((value & 3) << 4);
        setREG(YM_CH_PART(channel), YM_CH_OFFSET(0xB4, channel), CH[channel].LR_AMS_FMS);
    }

    /// @brief Set the FM sensitivity (FMS) register for the given channel.
    ///
    /// @param channel the channel to set the FM sensitivity register of
    /// @param value the amount of frequency modulation (FM) sensitivity
    ///
    inline void setFMS(uint8_t channel, uint8_t value) {
        if (channels[channel].FMS == value) return;
        channels[channel].FMS = value;
        CH[channel].LR_AMS_FMS = (CH[channel].LR_AMS_FMS & 0xF8)| (value & 7);
        setREG(YM_CH_PART(channel), YM_CH_OFFSET(0xB4, channel), CH[channel].LR_AMS_FMS);
    }

    // -----------------------------------------------------------------------
    // MARK: Operator control for each channel
    // -----------------------------------------------------------------------

    /// @brief Set the SSG-envelope register for the given channel and operator.
    ///
    /// @param channel the channel to set the SSG-EG register of (in [0, 6])
    /// @param slot the operator to set the SSG-EG register of (in [0, 3])
    /// @param is_on whether the looping envelope generator should be turned on
    /// @param mode the mode for the looping generator to run in (in [0, 7])
    /// @details
    /// The mode can be any of the following:
    ///
    /// Table: SSG-EG LFO Patterns
    /// +-------+-------------+
    /// | AtAlH | LFO Pattern |
    /// +=======+=============+
    /// | 0 0 0 |  \\\\       |
    /// +-------+-------------+
    /// | 0 0 1 |  \___       |
    /// +-------+-------------+
    /// | 0 1 0 |  \/\/       |
    /// +-------+-------------+
    /// |       |   ___       |
    /// | 0 1 1 |  \          |
    /// +-------+-------------+
    /// | 1 0 0 |  ////       |
    /// +-------+-------------+
    /// |       |   ___       |
    /// | 1 0 1 |  /          |
    /// +-------+-------------+
    /// | 1 1 0 |  /\/\       |
    /// +-------+-------------+
    /// | 1 1 1 |  /___       |
    /// +-------+-------------+
    ///
    /// The shapes are generated using Attack, Decay and Sustain phases.
    ///
    /// Each single character in the diagrams above represents this whole
    /// sequence:
    ///
    /// - when KEY-ON = 1, normal Attack phase is generated (*without* any
    ///   difference when compared to normal mode),
    ///
    /// - later, when envelope level reaches minimum level (max volume),
    ///   the EG switches to Decay phase (which works with bigger steps
    ///   when compared to normal mode - see below),
    ///
    /// - later when envelope level passes the SL level,
    ///   the EG swithes to Sustain phase (which works with bigger steps
    ///   when compared to normal mode - see below),
    ///
    /// - finally when envelope level reaches maximum level (min volume),
    ///   the EG switches to Attack phase again (depends on actual waveform).
    ///
    /// Important is that when switch to Attack phase occurs, the phase counter
    /// of that operator will be zeroed-out (as in normal KEY-ON) but not always.
    /// (I haven't found the rule for that - perhaps only when the output
    /// level is low)
    ///
    /// The difference (when compared to normal Envelope Generator mode) is
    /// that the resolution in Decay and Sustain phases is 4 times lower;
    /// this results in only 256 steps instead of normal 1024.
    /// In other words:
    /// when SSG-EG is disabled, the step inside of the EG is one,
    /// when SSG-EG is enabled, the step is four (in Decay and Sustain phases).
    ///
    /// Times between the level changes are the same in both modes.
    ///
    /// Important:
    /// Decay 1 Level (so called SL) is compared to actual SSG-EG output, so
    /// it is the same in both SSG and no-SSG modes, with this exception:
    ///
    /// when the SSG-EG is enabled and is generating raising levels
    /// (when the EG output is inverted) the SL will be found at wrong level!!!
    /// For example, when SL=02:
    ///     0 -6 = -6dB in non-inverted EG output
    ///     96-6 = -90dB in inverted EG output
    /// Which means that EG compares its level to SL as usual, and that the
    /// output is simply inverted after all.
    ///
    /// The Yamaha's manuals say that AR should be set to 0x1f (max speed).
    /// That is not necessary, but then EG will be generating Attack phase.
    ///
    inline void setSSG(uint8_t channel, uint8_t slot, bool is_on, uint8_t mode) {
        const uint8_t value = (is_on << 3) | (mode & 7);
        if (channels[channel].operators[slot].SSG == value) return;
        channels[channel].operators[slot].SSG = value;
        // TODO: slot here needs mapped to the order 1 3 2 4
        setREG(YM_CH_PART(channel), YM_CH_OFFSET(0x90 + (slot << 2), channel), value);
    }

    /// @brief Set the attack rate (AR) register for the given channel and operator.
    ///
    /// @param channel the channel to set the attack rate (AR) register of (in [0, 6])
    /// @param slot the operator to set the attack rate (AR) register of (in [0, 3])
    /// @param value the rate of the attack stage of the envelope generator
    ///
    inline void setAR(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].AR == value) return;
        channels[channel].operators[slot].AR = value;
        Operator *s = &CH[channel].SLOT[slots_idx[slot]];
        s->ar_ksr = (s->ar_ksr & 0xC0) | (value & 0x1f);
        set_ar_ksr(&CH[channel], s, s->ar_ksr);
    }

    /// @brief Set the 1st decay rate (D1) register for the given channel and operator.
    ///
    /// @param channel the channel to set the 1st decay rate (D1) register of (in [0, 6])
    /// @param slot the operator to set the 1st decay rate (D1) register of (in [0, 3])
    /// @param value the rate of decay for the 1st decay stage of the envelope generator
    ///
    inline void setD1(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].D1 == value) return;
        channels[channel].operators[slot].D1 = value;
        Operator *s = &CH[channel].SLOT[slots_idx[slot]];
        s->dr = (s->dr & 0x80) | (value & 0x1F);
        set_dr(s, s->dr);
    }

    /// @brief Set the sustain level (SL) register for the given channel and operator.
    ///
    /// @param channel the channel to set the sustain level (SL) register of (in [0, 6])
    /// @param slot the operator to set the sustain level (SL) register of (in [0, 3])
    /// @param value the amplitude level at which the 2nd decay stage of the envelope generator begins
    ///
    inline void setSL(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].SL == value) return;
        channels[channel].operators[slot].SL = value;
        Operator *s =  &CH[channel].SLOT[slots_idx[slot]];
        s->sl_rr = (s->sl_rr & 0x0f) | ((value & 0x0f) << 4);
        set_sl_rr(s, s->sl_rr);
    }

    /// @brief Set the 2nd decay rate (D2) register for the given channel and operator.
    ///
    /// @param channel the channel to set the 2nd decay rate (D2) register of (in [0, 6])
    /// @param slot the operator to set the 2nd decay rate (D2) register of (in [0, 3])
    /// @param value the rate of decay for the 2nd decay stage of the envelope generator
    ///
    inline void setD2(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].D2 == value) return;
        channels[channel].operators[slot].D2 = value;
        set_sr(&CH[channel].SLOT[slots_idx[slot]], value);
    }

    /// @brief Set the release rate (RR) register for the given channel and operator.
    ///
    /// @param channel the channel to set the release rate (RR) register of (in [0, 6])
    /// @param slot the operator to set the release rate (RR) register of (in [0, 3])
    /// @param value the rate of release of the envelope generator after key-off
    ///
    inline void setRR(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].RR == value) return;
        channels[channel].operators[slot].RR = value;
        Operator *s =  &CH[channel].SLOT[slots_idx[slot]];
        s->sl_rr = (s->sl_rr & 0xf0) | (value & 0x0f);
        set_sl_rr(s, s->sl_rr);
    }

    /// @brief Set the total level (TL) register for the given channel and operator.
    ///
    /// @param channel the channel to set the total level (TL) register of (in [0, 6])
    /// @param slot the operator to set the total level (TL) register of (in [0, 3])
    /// @param value the total amplitude of envelope generator
    ///
    inline void setTL(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].TL == value) return;
        channels[channel].operators[slot].TL = value;
        set_tl(&CH[channel], &CH[channel].SLOT[slots_idx[slot]], value);
    }

    /// @brief Set the multiplier (MUL) register for the given channel and operator.
    ///
    /// @param channel the channel to set the multiplier (MUL) register of (in [0, 6])
    /// @param slot the operator to set the multiplier  (MUL)register of (in [0, 3])
    /// @param value the value of the FM phase multiplier
    ///
    inline void setMUL(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].MUL == value) return;
        channels[channel].operators[slot].MUL = value;
        CH[channel].SLOT[slots_idx[slot]].mul = (value & 0x0f) ? (value & 0x0f) * 2 : 1;
        CH[channel].SLOT[SLOT1].Incr = -1;
    }

    /// @brief Set the detune (DET) register for the given channel and operator.
    ///
    /// @param channel the channel to set the detune (DET) register of (in [0, 6])
    /// @param slot the operator to set the detune (DET) register of (in [0, 3])
    /// @param value the the level of detuning for the FM operator
    ///
    inline void setDET(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].DET == value) return;
        channels[channel].operators[slot].DET = value;
        CH[channel].SLOT[slots_idx[slot]].DT  = OPN.ST.dt_tab[(value)&7];
        CH[channel].SLOT[SLOT1].Incr = -1;
    }

    /// @brief Set the rate-scale (RS) register for the given channel and operator.
    ///
    /// @param channel the channel to set the rate-scale (RS) register of (in [0, 6])
    /// @param slot the operator to set the rate-scale (RS) register of (in [0, 3])
    /// @param value the amount of rate-scale applied to the FM operator
    ///
    inline void setRS(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].RS == value) return;
        channels[channel].operators[slot].RS = value;
        Operator *s = &CH[channel].SLOT[slots_idx[slot]];
        s->ar_ksr = (s->ar_ksr & 0x1F) | ((value & 0x03) << 6);
        set_ar_ksr(&CH[channel], s, s->ar_ksr);
    }

    /// @brief Set the amplitude modulation (AM) register for the given channel and operator.
    ///
    /// @param channel the channel to set the amplitude modulation (AM) register of (in [0, 6])
    /// @param slot the operator to set the amplitude modulation (AM) register of (in [0, 3])
    /// @param value the true to enable amplitude modulation from the LFO, false to disable it
    ///
    inline void setAM(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].AM == value) return;
        channels[channel].operators[slot].AM = value;
        Operator *s = &CH[channel].SLOT[slots_idx[slot]];
        s->AMmask = (value) ? ~0 : 0;
    }

    // -----------------------------------------------------------------------
    // MARK: Emulator output
    // -----------------------------------------------------------------------

    /// @brief Return the output from the left channel of the mix output.
    ///
    /// @returns the left channel of the mix output
    ///
    inline int16_t getOutputLeft() { return MOL; }

    /// @brief Return the output from the right channel of the mix output.
    ///
    /// @returns the right channel of the mix output
    ///
    inline int16_t getOutputRight() { return MOR; }

    /// @brief Return the voltage from the left channel of the mix output.
    ///
    /// @returns the voltage of the left channel of the mix output
    ///
    inline float getVoltageLeft() {
        return static_cast<float>(MOL) / std::numeric_limits<int16_t>::max();
    }

    /// @brief Return the voltage from the right channel of the mix output.
    ///
    /// @returns the voltage of the right channel of the mix output
    ///
    inline float getVoltageRight() {
        return static_cast<float>(MOR) / std::numeric_limits<int16_t>::max();
    }
};

#endif  // DSP_YAMAHA_YM2612_HPP_
