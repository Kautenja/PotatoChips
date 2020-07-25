// Sony SPC700 emulator.
// Copyright 2020 Christian Kauten
// Copyright 2006 Shay Green
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
//

#ifndef DSP_SONY_SPC700_HPP_
#define DSP_SONY_SPC700_HPP_

#include "exceptions.hpp"
#include <cassert>

/// @brief Sony SonySPC700 chip emulator.
class SonySPC700 {
 public:
    enum { VOICE_COUNT = 8 };
    enum { REGISTER_COUNT = 128 };

 private:
    struct raw_voice_t {
        int8_t  left_vol;
        int8_t  right_vol;
        uint8_t rate[2];
        uint8_t waveform;
        uint8_t adsr[2];   // envelope rates for attack, decay, and sustain
        uint8_t gain;       // envelope gain (if not using ADSR)
        int8_t  envx;       // current envelope level
        int8_t  outx;       // current sample
        int8_t  unused[6];
    };

    struct globals_t {
        int8_t  unused1[12];
        int8_t  left_volume;        // 0C   Main Volume Left (-.7)
        int8_t  echo_feedback;      // 0D   Echo Feedback (-.7)
        int8_t  unused2[14];
        int8_t  right_volume;       // 1C   Main Volume Right (-.7)
        int8_t  unused3[15];
        int8_t  left_echo_volume;   // 2C   Echo Volume Left (-.7)
        uint8_t pitch_mods;         // 2D   Pitch Modulation on/off for each voice
        int8_t  unused4[14];
        int8_t  right_echo_volume;  // 3C   Echo Volume Right (-.7)
        uint8_t noise_enables;      // 3D   Noise output on/off for each voice
        int8_t  unused5[14];
        uint8_t key_ons;            // 4C   Key On for each voice
        uint8_t echo_ons;           // 4D   Echo on/off for each voice
        int8_t  unused6[14];
        uint8_t key_offs;           // 5C   key off for each voice (instantiates release mode)
        uint8_t wave_page;          // 5D   source directory (wave table offsets)
        int8_t  unused7[14];
        uint8_t flags;              // 6C   flags and noise freq
        uint8_t echo_page;          // 6D
        int8_t  unused8[14];
        uint8_t wave_ended;         // 7C
        uint8_t echo_delay;         // 7D   ms >> 4
        char    unused9[2];
    };

    union {
        raw_voice_t voice[VOICE_COUNT];
        uint8_t reg[REGISTER_COUNT];
        globals_t g;
    };

    uint8_t* const ram;

    // Cache of echo FIR values for faster access
    short fir_coeff[VOICE_COUNT];

    // fir_buf[i + 8] == fir_buf[i], to avoid wrap checking in FIR code
    short fir_buf[16][2];
    int fir_offset;  // (0 to 7)

    static constexpr int EMU_GAIN_BITS = 8;
    int emu_gain;

    int keys;

    int echo_ptr;
    int noise_amp;
    int noise;
    int noise_count;

    int surround_threshold;

    static int16_t const gauss[];

    enum state_t {
        state_attack,
        state_decay,
        state_sustain,
        state_release
    };

    struct voice_t {
        short volume[2];
        /// 12-bit fractional position
        short fraction;
        /// most recent four decoded samples
        short interp3;
        short interp2;
        short interp1;
        short interp0;
        /// number of nybbles remaining in current block
        short block_remain;
        unsigned short addr;
        /// header byte from current block
        short block_header;
        short envcnt;
        short envx;
        short on_cnt;
        /// 7 if enabled, 31 if disabled
        short enabled;
        short envstate;
        /// pad to power of 2
        short unused;
    };

    voice_t voice_state[VOICE_COUNT];

    int clock_envelope(int);

 public:
    // Keeps pointer to 64K ram
    SonySPC700(uint8_t* ram);

    // Mute voice n if bit n (1 << n) of mask is clear.
    void mute_voices(int mask);

    // Clear state and silence everything.
    void reset();

    /// Set gain, where 1.0 is normal. When greater than 1.0, output is
    /// clamped to the 16-bit sample range.
    inline void set_gain(double v) {
        emu_gain = static_cast<int>(v * (1 << EMU_GAIN_BITS));
    }

    // If true, prevent channels and global volumes from being phase-negated
    inline void disable_surround(bool disable) {
        surround_threshold = disable ? 0 : -0x7FFF;
    }

    // Read/write register 'n', where n ranges from 0 to REGISTER_COUNT - 1.
    inline int read(int i) {
        assert((unsigned) i < REGISTER_COUNT);
        return reg[i];
    }

    void write(int n, int);

    /// Run DSP for 'count' samples. Write resulting samples to 'buf' if not
    /// NULL.
    void run(long count, short* buf = NULL);
};

#endif  // DSP_SONY_SPC700_HPP_
