// Common functions for Sony S-DSP classes.
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
//

#ifndef DSP_SONY_S_DSP_COMMON_HPP_
#define DSP_SONY_S_DSP_COMMON_HPP_

#include <algorithm>
#include <cstdint>
#include <limits>

enum : unsigned {
    /// the sample rate of the S-DSP in Hz
    SAMPLE_RATE = 32000
};

/// Clamp an integer to a 16-bit value.
///
/// @param n a 32-bit integer value to clip
/// @returns n clipped to a 16-bit value [-32768, 32767]
///
inline int16_t clamp_16(int n) {
    const int lower = std::numeric_limits<int16_t>::min();
    const int upper = std::numeric_limits<int16_t>::max();
    return std::max(lower, std::min(n, upper));
}

/// @brief Returns the 14-bit pitch calculated from the given frequency.
///
/// @param frequency the frequency in Hz
/// @returns the 14-bit pitch corresponding to the S-DSP 32kHz sample rate
/// @details
///
/// \f$frequency = \f$SAMPLE_RATE\f$ * \frac{pitch}{2^{12}}\f$
///
static inline uint16_t get_pitch(float frequency) {
    // calculate the pitch based on the known relationship to frequency
    const auto pitch = static_cast<float>(1 << 12) * frequency / SAMPLE_RATE;
    // mask the 16-bit pitch to 14-bit
    return 0x3FFF & static_cast<uint16_t>(pitch);
}

/// The initial value of the envelope.
static constexpr int ENVELOPE_RATE_INITIAL = 0x7800;

/// the range of the envelope generator amplitude level (i.e., max value)
static constexpr int ENVELOPE_RANGE = 0x0800;

/// Return the envelope rate for the given index in the table.
///
/// @param index the index of the envelope rate to return in the table
/// @returns the envelope rate at given index in the table
///
static inline uint16_t get_envelope_rate(unsigned index) {
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

/// @brief Return the Gaussian interpolation table value for the given index.
///
/// @param index the index of the Gaussian interpolation to return
/// @returns the Gaussian interpolation table value at given index
///
static inline const int16_t* get_gaussian(unsigned index) {
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

    /// the header byte for the block
    union __attribute__((packed)) Header {
        /// the bit-wise flag representation of the header
        Flags flags;
        /// the encoded header byte
        uint8_t byte = 0;
    } header;

    /// the 8-byte block of sample data
    uint8_t samples[NUM_SAMPLES];
};

/// @brief A stereo sample of 16-bit PCM data.
struct __attribute__((packed, aligned(4))) StereoSample {
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

#endif  // DSP_SONY_S_DSP_COMMON_HPP_
