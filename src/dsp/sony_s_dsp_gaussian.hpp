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

#ifndef DSP_SONY_S_DSP_GAUSSIAN_HPP_
#define DSP_SONY_S_DSP_GAUSSIAN_HPP_

#include "sony_s_dsp_common.hpp"

/// @brief An emulation of the Gaussian filter from the Sony S-DSP.
/// @details
/// The emulator consumes 16 bytes of RAM and is aligned to 16-byte addresses.
class __attribute__((packed, aligned(16))) Sony_S_DSP_Gaussian {
 private:
    // -----------------------------------------------------------------------
    // Byte 1,2, 3,4, 5,6, 7,8
    // -----------------------------------------------------------------------
    /// a history of the four most recent samples
    int16_t samples[4] = {0, 0, 0, 0};
    // -----------------------------------------------------------------------
    // Byte 9,10
    // -----------------------------------------------------------------------
    /// 12-bit fractional position in the Gaussian table
    int16_t fraction = 0x3FFF;
    // -----------------------------------------------------------------------
    // Byte 11,12
    // -----------------------------------------------------------------------
    /// the volume level after the Gaussian filter
    int16_t volume = 0;
    // -----------------------------------------------------------------------
    // Byte 13,14
    // -----------------------------------------------------------------------
    /// the 14-bit frequency value
    uint16_t rate = 0;
    // -----------------------------------------------------------------------
    // Byte 15
    // -----------------------------------------------------------------------
    /// the discrete filter mode (i.e., the set of coefficients to use)
    uint8_t filter = 0;
    // -----------------------------------------------------------------------
    // Byte 16
    // -----------------------------------------------------------------------
    /// a dummy byte for byte alignment to 16-bytes
    const uint8_t unused_spacer_for_byte_alignment;

 public:
    /// the sample rate of the S-DSP in Hz
    static constexpr unsigned SAMPLE_RATE = 32000;

    /// @brief Initialize a new Sony_S_DSP_Gaussian.
    Sony_S_DSP_Gaussian() : unused_spacer_for_byte_alignment(0) { }

    /// @brief Set the filter coefficients to a discrete mode.
    ///
    /// @param filter the new mode for the filter
    ///
    inline void setFilter(uint8_t filter) { this->filter = filter & 0x3; }

    /// @brief Set the volume level of the low-pass gate to a new value.
    ///
    /// @param volume the volume level after the Gaussian low-pass filter
    ///
    inline void setVolume(int8_t volume) { this->volume = volume; }

    /// @brief Set the frequency of the low-pass gate to a new value.
    ///
    /// @param freq the frequency to set the low-pass gate to
    ///
    inline void setFrequency(float freq) {
        // calculate the pitch based on the known relationship to frequency
        const auto pitch = static_cast<float>(1 << 12) * freq / SAMPLE_RATE;
        rate = 0x3FFF & static_cast<uint16_t>(pitch);
    }

    /// @brief Run the Gaussian filter for the given input sample.
    ///
    /// @param input the 16-bit PCM sample to pass through the Gaussian filter
    /// @returns the output from the Gaussian filter system for given input
    ///
    int16_t run(int16_t input) {
        // -------------------------------------------------------------------
        // MARK: Filter
        // -------------------------------------------------------------------
        // VoiceState& voice = voice_state;
        // cast the input to 32-bit to do maths
        int delta = input;
        // One, two and three point IIR filters
        int smp1 = samples[0];
        int smp2 = samples[1];
        switch (filter) {
        case 0:  // !filter1 !filter2
            break;
        case 1:  // !filter1 filter2
            delta += smp1 >> 1;
            delta += (-smp1) >> 5;
            break;
        case 2:  // filter1 !filter2
            delta += smp1;
            delta -= smp2 >> 1;
            delta += (-smp1 - (smp1 >> 1)) >> 5;
            delta += smp2 >> 5;
            break;
        case 3:  // filter1 filter2
            delta += smp1;
            delta -= smp2 >> 1;
            delta += (-smp1 * 13) >> 7;
            delta += (smp2 + (smp2 >> 1)) >> 4;
            break;
        }
        // update sample history
        samples[3] = samples[2];
        samples[2] = smp2;
        samples[1] = smp1;
        samples[0] = 2 * clamp_16(delta);
        // Gaussian interpolation using most recent 4 samples. update the
        // fractional increment based on the 14-bit frequency rate of the voice
        const int index = fraction >> 2 & 0x3FC;
        fraction = (fraction & 0x0FFF) + rate;
        // -------------------------------------------------------------------
        // MARK: Gaussian table
        // -------------------------------------------------------------------
        // Base normal_gauss table is almost exactly (with an error of 0 or -1
        // for each entry):
        // int normal_gauss[512];
        // normal_gauss[i] = exp((i-511)*(i-511)*-9.975e-6)*pow(sin(0.00307096*i),1.7358)*1304.45
        // Interleved gauss table (to improve cache coherency).
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
        // get a pointer to the Gaussian interpolation table as bytes
        const auto gauss8 = reinterpret_cast<uint8_t const*>(gauss);
        // lookup the interpolation values in the table based on the index and
        // inverted index value
        const auto table1 = reinterpret_cast<int16_t const*>(gauss8 + index);
        const auto table2 = reinterpret_cast<int16_t const*>(gauss8 + (255 * 4 - index));
        // -------------------------------------------------------------------
        // MARK: Interpolation
        // -------------------------------------------------------------------
        // apply the Gaussian interpolation to the incoming sample
        int sample = ((table1[0] * samples[3]) >> 12) +
                     ((table1[1] * samples[2]) >> 12) +
                     ((table2[1] * samples[1]) >> 12);
        sample = static_cast<int16_t>(2 * sample);
        sample +=    ((table2[0] * samples[0]) >> 11) & ~1;
        // apply the volume/amplitude level
        sample = (sample  * volume) >> 7;
        // return the sample clipped to 16-bit PCM
        return clamp_16(sample);
    }
};

#endif  // DSP_SONY_S_DSP_GAUSSIAN_HPP_
