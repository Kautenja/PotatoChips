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

#define MAX_ATT_INDEX (ENV_LEN - 1) /* 1023 */
#define MIN_ATT_INDEX (0)           /* 0 */

#define EG_ATT 4
#define EG_DEC 3
#define EG_SUS 2
#define EG_REL 1
#define EG_OFF 0

#define SIN_BITS 10
#define SIN_LEN (1 << SIN_BITS)
#define SIN_MASK (SIN_LEN - 1)

#define TL_RES_LEN (256) /* 8 bits addressing (real chip) */

#define FINAL_SH (0)
#define MAXOUT (+32767)
#define MINOUT (-32768)

/* register number to channel number , slot offset */
#define OPN_CHAN(N) (N & 3)
#define OPN_SLOT(N) ((N >> 2) & 3)

/* slot number */
#define SLOT1 0
#define SLOT2 2
#define SLOT3 1
#define SLOT4 3

/* bit0 = Right enable , bit1 = Left enable */
#define OUTD_RIGHT 1
#define OUTD_LEFT 2
#define OUTD_CENTER 3

/*  TL_TAB_LEN is calculated as:
*   13 - sinus amplitude bits     (Y axis)
*   2  - sinus sign bit           (Y axis)
*   TL_RES_LEN - sinus resolution (X axis)
*/
#define TL_TAB_LEN (13 * 2 * TL_RES_LEN)
static signed int tl_tab[TL_TAB_LEN];

#define ENV_QUIET (TL_TAB_LEN >> 3)

/* sin waveform table in 'decibel' scale */
static unsigned int sin_tab[SIN_LEN];

/* sustain level table (3dB per step) */
/* bit0, bit1, bit2, bit3, bit4, bit5, bit6 */
/* 1,    2,    4,    8,    16,   32,   64   (value)*/
/* 0.75, 1.5,  3,    6,    12,   24,   48   (dB)*/

/* 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)*/
#define SC(db) (uint32_t)(db * (4.0 / ENV_STEP))
static const uint32_t sl_table[16] = {
    SC(0), SC(1), SC(2), SC(3), SC(4), SC(5), SC(6), SC(7),
    SC(8), SC(9), SC(10), SC(11), SC(12), SC(13), SC(14), SC(31)};
#undef SC

static const uint8_t slots_idx[4] = {0,2,1,3};

#define RATE_STEPS (8)
static const uint8_t eg_inc[19 * RATE_STEPS] = {
    /*cycle:0 1  2 3  4 5  6 7*/

    /* 0 */ 0, 1, 0, 1, 0, 1, 0, 1, /* rates 00..11 0 (increment by 0 or 1) */
    /* 1 */ 0, 1, 0, 1, 1, 1, 0, 1, /* rates 00..11 1 */
    /* 2 */ 0, 1, 1, 1, 0, 1, 1, 1, /* rates 00..11 2 */
    /* 3 */ 0, 1, 1, 1, 1, 1, 1, 1, /* rates 00..11 3 */

    /* 4 */ 1, 1, 1, 1, 1, 1, 1, 1, /* rate 12 0 (increment by 1) */
    /* 5 */ 1, 1, 1, 2, 1, 1, 1, 2, /* rate 12 1 */
    /* 6 */ 1, 2, 1, 2, 1, 2, 1, 2, /* rate 12 2 */
    /* 7 */ 1, 2, 2, 2, 1, 2, 2, 2, /* rate 12 3 */

    /* 8 */ 2, 2, 2, 2, 2, 2, 2, 2, /* rate 13 0 (increment by 2) */
    /* 9 */ 2, 2, 2, 4, 2, 2, 2, 4, /* rate 13 1 */
    /*10 */ 2, 4, 2, 4, 2, 4, 2, 4, /* rate 13 2 */
    /*11 */ 2, 4, 4, 4, 2, 4, 4, 4, /* rate 13 3 */

    /*12 */ 4, 4, 4, 4, 4, 4, 4, 4, /* rate 14 0 (increment by 4) */
    /*13 */ 4, 4, 4, 8, 4, 4, 4, 8, /* rate 14 1 */
    /*14 */ 4, 8, 4, 8, 4, 8, 4, 8, /* rate 14 2 */
    /*15 */ 4, 8, 8, 8, 4, 8, 8, 8, /* rate 14 3 */

    /*16 */ 8, 8, 8, 8, 8, 8, 8, 8,         /* rates 15 0, 15 1, 15 2, 15 3 (increment by 8) */
    /*17 */ 16, 16, 16, 16, 16, 16, 16, 16, /* rates 15 2, 15 3 for attack */
    /*18 */ 0, 0, 0, 0, 0, 0, 0, 0,         /* infinity rates for attack and decay(s) */
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

/*rate  0,    1,    2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15*/
/*shift 11,  10,  9,  8,  7,  6,  5,  4,  3,  2, 1,  0,  0,  0,  0,  0 */
/*mask  2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3, 1,  0,  0,  0,  0,  0 */

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

static const uint8_t dt_tab[4 * 32] = {
// this is YM2151 and YM2612 phase increment data (in 10.10 fixed point format)
    /* FD=0 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* FD=1 */
    0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
    2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8,
    /* FD=2 */
    1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5,
    5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 16, 16, 16, 16,
    /* FD=3 */
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

#define YM_CH_PART(ch) (ch/3)
#define YM_CH_OFFSET(reg, ch) (reg + (ch % 3))

/// A single FM operator (SLOT)
struct FM_SLOT {
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

    ///  (attack state)
    uint8_t eg_sh_ar = 0;
    ///  (attack state)
    uint8_t eg_sel_ar = 0;
    ///  (decay state)
    uint8_t eg_sh_d1r = 0;
    ///  (decay state)
    uint8_t eg_sel_d1r = 0;
    ///  (sustain state)
    uint8_t eg_sh_d2r = 0;
    ///  (sustain state)
    uint8_t eg_sel_d2r = 0;
    ///  (release state)
    uint8_t eg_sh_rr = 0;
    ///  (release state)
    uint8_t eg_sel_rr = 0;

    /// SSG-EG waveform
    uint8_t ssg = 0;
    /// SSG-EG negated output
    uint8_t ssgn = 0;

    /// 0=last key was KEY OFF, 1=KEY ON
    uint32_t key = 0;

    /// AM enable flag
    uint32_t AMmask = 0;

    /// TODO
    uint8_t det_mul = 0;
    /// TODO
    uint8_t ar_ksr = 0;
    /// TODO
    uint8_t sl_rr = 0;
    /// TODO
    uint8_t dr = 0;
};

/// A single FM voice (4-operator)
struct FM_CH {
    /// four SLOTs (operators)
    FM_SLOT SLOT[4];

    /// algorithm
    uint8_t ALGO = 0;
    /// feedback shift
    uint8_t FB = 0;
    /// op1 output for feedback
    int32_t op1_out[2] = {0, 0};

    /// SLOT1 output pointer
    int32_t *connect1 = nullptr;
    /// SLOT3 output pointer
    int32_t *connect3 = nullptr;
    /// SLOT2 output pointer
    int32_t *connect2 = nullptr;
    /// SLOT4 output pointer
    int32_t *connect4 = nullptr;

    /* where to put the delayed sample (MEM) */
    int32_t *mem_connect = nullptr;
    /* delayed sample (MEM) value */
    int32_t mem_value = 0;

    /// channel phase modulation sensitivity (PMS)
    int32_t pms = 0;
    /// channel amplitude modulation sensitivity (AMS)
    uint8_t ams = 0;

    /// fnum, blk : adjusted to sample rate
    uint32_t fc = 0;
    /// key code:
    uint8_t kcode = 0;
    /// current blk/fnum value for this slot (can be different betweeen slots
    /// of one channel in 3slot mode)
    uint32_t block_fnum = 0;

    /// TODO
    uint8_t FB_ALG = 0;
    /// TODO
    uint8_t LR_AMS_FMS = 0;
};

/// The state of an FM synthesis operator.
struct FM_ST {
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
struct FM_OPN {
    /// chip type
    uint8_t type = 0;
    /// general state
    FM_ST ST;
    /// 3 slot mode state
    FM_3SLOT SL3;
    /// pointer of CH
    FM_CH *P_CH = nullptr;
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
// MARK: public interface
// ---------------------------------------------------------------------------

/// Yamaha YM2612 chip emulator
class YamahaYM2612 {
 private:
    /// registers
    uint8_t registers[512];
    /// OPN state
    FM_OPN OPN;
    /// channel state
    FM_CH CH[6];
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
    YamahaYM2612(double clock_rate = 768000, double sample_rate = 44100);

    /// @brief Set the sample rate the a new value.
    ///
    /// @param clock_rate the underlying clock rate of the system
    /// @param sample_rate the rate to draw samples from the emulator at
    ///
    void setSampleRate(double clock_rate, double sample_rate);

    /// @brief Reset the emulator to its initial state
    void reset();

    /// @brief Run a step on the emulator
    void step();

    /// @brief Write data to a register on the chip.
    ///
    /// @param address the address of the register to write data to
    /// @param data the value of the data to write to the register
    ///
    void write(uint8_t address, uint8_t data);

    /// @brief Set part of a 16-bit register to a given 8-bit value.
    ///
    /// @param part TODO
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
    void setSSG(uint8_t channel, uint8_t slot, bool is_on, uint8_t mode);

    /// @brief Set the attack rate (AR) register for the given channel and operator.
    ///
    /// @param channel the channel to set the attack rate (AR) register of (in [0, 6])
    /// @param slot the operator to set the attack rate (AR) register of (in [0, 3])
    /// @param value the rate of the attack stage of the envelope generator
    ///
    void setAR(uint8_t channel, uint8_t slot, uint8_t value);

    /// @brief Set the 1st decay rate (D1) register for the given channel and operator.
    ///
    /// @param channel the channel to set the 1st decay rate (D1) register of (in [0, 6])
    /// @param slot the operator to set the 1st decay rate (D1) register of (in [0, 3])
    /// @param value the rate of decay for the 1st decay stage of the envelope generator
    ///
    void setD1(uint8_t channel, uint8_t slot, uint8_t value);

    /// @brief Set the sustain level (SL) register for the given channel and operator.
    ///
    /// @param channel the channel to set the sustain level (SL) register of (in [0, 6])
    /// @param slot the operator to set the sustain level (SL) register of (in [0, 3])
    /// @param value the amplitude level at which the 2nd decay stage of the envelope generator begins
    ///
    void setSL(uint8_t channel, uint8_t slot, uint8_t value);

    /// @brief Set the 2nd decay rate (D2) register for the given channel and operator.
    ///
    /// @param channel the channel to set the 2nd decay rate (D2) register of (in [0, 6])
    /// @param slot the operator to set the 2nd decay rate (D2) register of (in [0, 3])
    /// @param value the rate of decay for the 2nd decay stage of the envelope generator
    ///
    void setD2(uint8_t channel, uint8_t slot, uint8_t value);

    /// @brief Set the release rate (RR) register for the given channel and operator.
    ///
    /// @param channel the channel to set the release rate (RR) register of (in [0, 6])
    /// @param slot the operator to set the release rate (RR) register of (in [0, 3])
    /// @param value the rate of release of the envelope generator after key-off
    ///
    void setRR(uint8_t channel, uint8_t slot, uint8_t value);

    /// @brief Set the total level (TL) register for the given channel and operator.
    ///
    /// @param channel the channel to set the total level (TL) register of (in [0, 6])
    /// @param slot the operator to set the total level (TL) register of (in [0, 3])
    /// @param value the total amplitude of envelope generator
    ///
    void setTL(uint8_t channel, uint8_t slot, uint8_t value);

    /// @brief Set the multiplier (MUL) register for the given channel and operator.
    ///
    /// @param channel the channel to set the multiplier (MUL) register of (in [0, 6])
    /// @param slot the operator to set the multiplier  (MUL)register of (in [0, 3])
    /// @param value the value of the FM phase multiplier
    ///
    void setMUL(uint8_t channel, uint8_t slot, uint8_t value);

    /// @brief Set the detune (DET) register for the given channel and operator.
    ///
    /// @param channel the channel to set the detune (DET) register of (in [0, 6])
    /// @param slot the operator to set the detune (DET) register of (in [0, 3])
    /// @param value the the level of detuning for the FM operator
    ///
    void setDET(uint8_t channel, uint8_t slot, uint8_t value);

    /// @brief Set the rate-scale (RS) register for the given channel and operator.
    ///
    /// @param channel the channel to set the rate-scale (RS) register of (in [0, 6])
    /// @param slot the operator to set the rate-scale (RS) register of (in [0, 3])
    /// @param value the amount of rate-scale applied to the FM operator
    ///
    void setRS(uint8_t channel, uint8_t slot, uint8_t value);

    /// @brief Set the amplitude modulation (AM) register for the given channel and operator.
    ///
    /// @param channel the channel to set the amplitude modulation (AM) register of (in [0, 6])
    /// @param slot the operator to set the amplitude modulation (AM) register of (in [0, 3])
    /// @param value the true to enable amplitude modulation from the LFO, false to disable it
    ///
    void setAM(uint8_t channel, uint8_t slot, uint8_t value);

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
