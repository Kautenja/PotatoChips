// Sony SPC700 emulator.
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

#include "snes_echo.hpp"
#include <algorithm>
#include <cstddef>
#include <limits>

Sony_S_DSP_Echo::Sony_S_DSP_Echo(uint8_t* ram_) : ram(ram_) {
    // validate that the structures are of expected size
    // TODO: move to unit testing code and remove from here
    assert(NUM_REGISTERS == sizeof(GlobalData));
    assert(NUM_REGISTERS == sizeof(voices));
    assert(9 == sizeof(BitRateReductionBlock));
    assert(4 == sizeof(SourceDirectoryEntry));
    assert(4 == sizeof(EchoBufferSample));
}

/// This table is for envelope timing global.  It represents the number of
/// counts that should be subtracted from the counter each sample period
/// (32kHz). The counter starts at 30720 (0x7800). Each count divides exactly
/// into 0x7800 without remainder.
const int env_rate_init = 0x7800;
static const short env_rates[0x20] = {
    0x0000, 0x000F, 0x0014, 0x0018, 0x001E, 0x0028, 0x0030, 0x003C,
    0x0050, 0x0060, 0x0078, 0x00A0, 0x00C0, 0x00F0, 0x0140, 0x0180,
    0x01E0, 0x0280, 0x0300, 0x03C0, 0x0500, 0x0600, 0x0780, 0x0A00,
    0x0C00, 0x0F00, 0x1400, 0x1800, 0x1E00, 0x2800, 0x3C00, 0x7800
};

/// the range of the envelope generator amplitude level (i.e., max value)
const int ENVELOPE_RANGE = 0x800;

inline int Sony_S_DSP_Echo::clock_envelope(unsigned voice_idx) {
    RawVoice& raw_voice = this->voices[voice_idx];
    VoiceState& voice = voice_states[voice_idx];

    int envx = voice.envx;

    if (voice.envelope_stage == EnvelopeStage::Release) {
        envx = 0;
        voice.envx = envx;
        raw_voice.envx = envx >> 8;
        return envx;
    }

    int cnt = voice.envcnt;

    // Note: if the game switches between ADSR and GAIN modes
    // partway through, should the count be reset, or should it
    // continue from where it was? Does the DSP actually watch for
    // that bit to change, or does it just go along with whatever
    // it sees when it performs the update? I'm going to assume
    // the latter and not update the count, unless I see a game
    // that obviously wants the other behavior.  The effect would
    // be pretty subtle, in any case.
    int t = raw_voice.gain;
    if (t < 0x80) {
        envx = voice.envx = t << 4;
    }
    else switch (t >> 5) {
    case 4:         /* Docs: "Decrease (linear): Subtraction
                         * of the fixed value 1/64." */
        cnt -= env_rates[t & 0x1F];
        if (cnt > 0)
            break;
        cnt = env_rate_init;
        envx -= ENVELOPE_RANGE / 64;
        if (envx < 0) {
            envx = 0;
            if (voice.envelope_stage == EnvelopeStage::Attack)
                voice.envelope_stage = EnvelopeStage::Decay;
        }
        voice.envx = envx;
        break;
    case 5:         /* Docs: "Decrease <sic> (exponential):
                         * Multiplication by the fixed value
                         * 1-1/256." */
        cnt -= env_rates[t & 0x1F];
        if (cnt > 0)
            break;
        cnt = env_rate_init;
        envx -= ((envx - 1) >> 8) + 1;
        if (envx < 0) {
            envx = 0;
            if (voice.envelope_stage == EnvelopeStage::Attack)
                voice.envelope_stage = EnvelopeStage::Decay;
        }
        voice.envx = envx;
        break;
    case 6:         /* Docs: "Increase (linear): Addition of
                         * the fixed value 1/64." */
        cnt -= env_rates[t & 0x1F];
        if (cnt > 0)
            break;
        cnt = env_rate_init;
        envx += ENVELOPE_RANGE / 64;
        if (envx >= ENVELOPE_RANGE)
            envx = ENVELOPE_RANGE - 1;
        voice.envx = envx;
        break;
    case 7:         /* Docs: "Increase (bent line): Addition
                         * of the constant 1/64 up to .75 of the
                         * constant <sic> 1/256 from .75 to 1." */
        cnt -= env_rates[t & 0x1F];
        if (cnt > 0)
            break;
        cnt = env_rate_init;
        if (envx < ENVELOPE_RANGE * 3 / 4)
            envx += ENVELOPE_RANGE / 64;
        else
            envx += ENVELOPE_RANGE / 256;
        if (envx >= ENVELOPE_RANGE)
            envx = ENVELOPE_RANGE - 1;
        voice.envx = envx;
        break;
    }

    voice.envcnt = cnt;
    raw_voice.envx = envx >> 4;
    return envx;
}

/// Clamp an integer to a 16-bit value.
///
/// @param n a 32-bit integer value to clip
/// @returns n clipped to a 16-bit value [-32768, 32767]
///
inline int clamp_16(int n) {
    const int lower = std::numeric_limits<int16_t>::min();
    const int upper = std::numeric_limits<int16_t>::max();
    return std::max(lower, std::min(n, upper));
}

void Sony_S_DSP_Echo::run(int32_t count, int left, int right, int16_t* output_buffer) {
    while (--count >= 0) {
        // buffer the inputs for applying feedback
        int echol = left;
        int echor = right;

        // get the current feedback sample in the echo buffer
        EchoBufferSample* const echo_sample =
            reinterpret_cast<EchoBufferSample*>(&ram[(global.echo_page * 0x100 + echo_ptr) & 0xFFFF]);
        // increment the echo pointer by the size of the echo buffer sample (4)
        echo_ptr += sizeof(EchoBufferSample);
        // check if for the end of the ring buffer and wrap the pointer around
        // the echo delay is clamped in [0, 15] and each delay index requires
        // 2KB of RAM (0x800)
        if (echo_ptr >= (global.echo_delay & 15) * 0x800) echo_ptr = 0;
        // cache the feedback value (sign-extended to 32-bit)
        int fb_left = echo_sample->samples[EchoBufferSample::LEFT];
        int fb_right = echo_sample->samples[EchoBufferSample::RIGHT];

        // put samples in history ring buffer
        const int fir_offset = this->fir_offset;
        short (*fir_pos)[2] = &fir_buf[fir_offset];
        this->fir_offset = (fir_offset + 7) & 7;  // move backwards one step
        fir_pos[0][0] = (short) fb_left;
        fir_pos[0][1] = (short) fb_right;
        // duplicate at +8 eliminates wrap checking below
        fir_pos[8][0] = (short) fb_left;
        fir_pos[8][1] = (short) fb_right;

        // FIR
        fb_left =     fb_left * fir_coeff[7] +
                fir_pos[1][0] * fir_coeff[6] +
                fir_pos[2][0] * fir_coeff[5] +
                fir_pos[3][0] * fir_coeff[4] +
                fir_pos[4][0] * fir_coeff[3] +
                fir_pos[5][0] * fir_coeff[2] +
                fir_pos[6][0] * fir_coeff[1] +
                fir_pos[7][0] * fir_coeff[0];

        fb_right =   fb_right * fir_coeff[7] +
                fir_pos[1][1] * fir_coeff[6] +
                fir_pos[2][1] * fir_coeff[5] +
                fir_pos[3][1] * fir_coeff[4] +
                fir_pos[4][1] * fir_coeff[3] +
                fir_pos[5][1] * fir_coeff[2] +
                fir_pos[6][1] * fir_coeff[1] +
                fir_pos[7][1] * fir_coeff[0];
        // add the echo to the samples for the left and right channel
        left  += (fb_left  * global.left_echo_volume) >> 14;
        right += (fb_right * global.right_echo_volume) >> 14;

        // if (!(global.flags & FLAG_MASK_ECHO_WRITE)) {  // echo buffer feedback
            // add feedback to the echo samples
            echol += (fb_left  * global.echo_feedback) >> 14;
            echor += (fb_right * global.echo_feedback) >> 14;
            // put the echo samples into the buffer
            echo_sample->samples[EchoBufferSample::LEFT] = clamp_16(echol);
            echo_sample->samples[EchoBufferSample::RIGHT] = clamp_16(echor);
        // }

        // -------------------------------------------------------------------
        // MARK: Output
        // -------------------------------------------------------------------
        if (output_buffer) {  // write final samples
            // clamp the left and right samples and place them into the buffer
            output_buffer[0] = left  = clamp_16(left);
            output_buffer[1] = right = clamp_16(right);
            // increment the buffer to the position of the next stereo sample
            output_buffer += 2;
        }
    }
}

// Base normal_gauss table is almost exactly (with an error of 0 or -1 for each entry):
// int normal_gauss[512];
// normal_gauss[i] = exp((i-511)*(i-511)*-9.975e-6)*pow(sin(0.00307096*i),1.7358)*1304.45

// Interleved gauss table (to improve cache coherency).
// gauss[i * 2 + j] = normal_gauss[(1 - j) * 256 + i]
const int16_t Sony_S_DSP_Echo::gauss[512] = {
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
