// An emulation of the Gaussian filter from the Sony S-DSP.
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

#include "sony_s_dsp_gaussian.hpp"
#include <algorithm>
#include <cstddef>
#include <limits>

Sony_S_DSP_Gaussian::Sony_S_DSP_Gaussian(uint8_t* ram_) : ram(ram_) {
    // validate that the structures are of expected size
    // TODO: move to unit testing code and remove from here
    assert(NUM_REGISTERS == sizeof(GlobalData));
    assert(NUM_REGISTERS == sizeof(voices));
    assert(9 == sizeof(BitRateReductionBlock));
    assert(4 == sizeof(SourceDirectoryEntry));
    assert(4 == sizeof(EchoBufferSample));
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

void Sony_S_DSP_Gaussian::run(int32_t count, int delta, int16_t* output_buffer) {
    // render samples at 32kHz for the given count of samples
    while (--count >= 0) {
        RawVoice& raw_voice = voices[0];
        VoiceState& voice = voice_states[0];

        // int delta = input;
        // One, two and three point IIR filters
        int smp1 = voice.interp0;
        int smp2 = voice.interp1;
        // if (voice.block_header & 8) {
            delta += smp1;
            delta -= smp2 >> 1;
            if (!(voice.block_header & 4)) {
                delta += (-smp1 - (smp1 >> 1)) >> 5;
                delta += smp2 >> 5;
            } else {
                delta += (-smp1 * 13) >> 7;
                delta += (smp2 + (smp2 >> 1)) >> 4;
            }
        // } else if (voice.block_header & 4) {
            // delta += smp1 >> 1;
            // delta += (-smp1) >> 5;
        // }

        voice.interp3 = voice.interp2;
        voice.interp2 = smp2;
        voice.interp1 = smp1;
        // sign-extend
        voice.interp0 = int16_t (clamp_16(delta) * 2);

        // get the 14-bit frequency value
        int rate = 0x3FFF & ((raw_voice.rate[1] << 8) | raw_voice.rate[0]);

        // Gaussian interpolation using most recent 4 samples
        int index = voice.fraction >> 2 & 0x3FC;
        voice.fraction = (voice.fraction & 0x0FFF) + rate;
        const int16_t* table  = (int16_t const*) ((char const*) gauss + index);
        const int16_t* table2 = (int16_t const*) ((char const*) gauss + (255 * 4 - index));
        int s = ((table[0] * voice.interp3) >> 12) +
                ((table[1] * voice.interp2) >> 12) +
                ((table2[1] * voice.interp1) >> 12);
        s = (int16_t) (s * 2);
        s += (table2[0] * voice.interp0) >> 11 & ~1;
        int output = clamp_16(s);
        // scale output and set outx values
        int left = (voice.volume[0] * output) >> 7;
        int right = (voice.volume[1] * output) >> 7;

        if (output_buffer) {  // write final samples
            // clamp the left and right samples and place them into the buffer
            output_buffer[0] = left  = clamp_16(left);
            output_buffer[1] = right = clamp_16(right);
            // increment the buffer to the position of the next stereo sample
            output_buffer += 2;
            if (global.flags & FLAG_MASK_MUTE)  // muting
                output_buffer[-2] = output_buffer[-1] = 0;
        }
    }
}

// Base normal_gauss table is almost exactly (with an error of 0 or -1 for each entry):
// int normal_gauss[512];
// normal_gauss[i] = exp((i-511)*(i-511)*-9.975e-6)*pow(sin(0.00307096*i),1.7358)*1304.45

// Interleved gauss table (to improve cache coherency).
// gauss[i * 2 + j] = normal_gauss[(1 - j) * 256 + i]
const int16_t Sony_S_DSP_Gaussian::gauss[512] = {
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
