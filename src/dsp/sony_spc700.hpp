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

/// @brief Sony SPC700 chip emulator.
class SonySPC700 {
 public:
    /// the number of oscillators on the chip
    static constexpr unsigned VOICE_COUNT = 8;
    /// the number of RAM registers on the chip
    static constexpr unsigned REGISTER_COUNT = 128;

 private:
    /// TODO
    struct raw_voice_t {
        /// TODO
        int8_t left_vol;
        /// TODO
        int8_t right_vol;
        /// TODO
        uint8_t rate[2];
        /// TODO
        uint8_t waveform;
        /// envelope rates for attack, decay, and sustain
        uint8_t adsr[2];
        /// envelope gain (if not using ADSR)
        uint8_t gain;
        /// current envelope level
        int8_t envx;
        /// current sample
        int8_t outx;
        /// TODO
        int8_t unused[6];
    };

    /// TODO
    struct globals_t {
        /// TODO
        int8_t unused1[12];
        /// 0C   Main Volume Left (-.7)
        int8_t left_volume;
        /// 0D   Echo Feedback (-.7)
        int8_t echo_feedback;
        /// TODO
        int8_t unused2[14];
        /// 1C   Main Volume Right (-.7)
        int8_t right_volume;
        /// TODO
        int8_t unused3[15];
        /// 2C   Echo Volume Left (-.7)
        int8_t left_echo_volume;
        /// 2D   Pitch Modulation on/off for each voice
        uint8_t pitch_mods;
        /// TODO
        int8_t unused4[14];
        /// 3C   Echo Volume Right (-.7)
        int8_t right_echo_volume;
        /// 3D   Noise output on/off for each voice
        uint8_t noise_enables;
        /// TODO
        int8_t unused5[14];
        /// 4C   Key On for each voice
        uint8_t key_ons;
        /// 4D   Echo on/off for each voice
        uint8_t echo_ons;
        /// TODO
        int8_t unused6[14];
        /// 5C   key off for each voice (instantiates release mode)
        uint8_t key_offs;
        /// 5D   source directory (wave table offsets)
        uint8_t wave_page;
        /// TODO
        int8_t unused7[14];
        /// 6C   flags and noise freq
        uint8_t flags;
        /// 6D
        uint8_t echo_page;
        /// TODO
        int8_t unused8[14];
        /// 7C
        uint8_t wave_ended;
        /// 7D   ms >> 4
        uint8_t echo_delay;
        /// TODO
        char unused9[2];
    };

    union {
        /// TODO
        raw_voice_t voice[VOICE_COUNT];
        /// TODO
        uint8_t reg[REGISTER_COUNT];
        /// TODO
        globals_t g;
    };

    /// TODO
    uint8_t* const ram;

    /// Cache of echo FIR values for faster access
    short fir_coeff[VOICE_COUNT];

    /// fir_buf[i + 8] == fir_buf[i], to avoid wrap checking in FIR code
    short fir_buf[16][2];
    /// (0 to 7)
    int fir_offset;

    /// TODO
    static constexpr int EMU_GAIN_BITS = 8;
    /// TODO
    int emu_gain;

    /// TODO
    int keys;

    /// TODO
    int echo_ptr;
    /// TODO
    int noise_amp;
    /// TODO
    int noise;
    /// TODO
    int noise_count;

    /// TODO
    int surround_threshold;

    /// TODO
    static int16_t const gauss[];

    /// the states of the ADSR envelope generator
    enum state_t {
        /// the attack stage of the envelope generator
        state_attack,
        /// the decay stage of the envelope generator
        state_decay,
        /// the sustain stage of the envelope generator
        state_sustain,
        /// the release stage of the envelope generator
        state_release
    };

    /// TODO
    struct voice_t {
        /// TODO
        short volume[2];
        /// 12-bit fractional position
        short fraction;
        /// most recent four decoded samples
        short interp3;
        /// TODO
        short interp2;
        /// TODO
        short interp1;
        /// TODO
        short interp0;
        /// number of nybbles remaining in current block
        short block_remain;
        /// TODO
        unsigned short addr;
        /// header byte from current block
        short block_header;
        /// TODO
        short envcnt;
        /// TODO
        short envx;
        /// TODO
        short on_cnt;
        /// 7 if enabled, 31 if disabled
        short enabled;
        /// TODO
        short envstate;
        /// pad to power of 2
        short unused;
    };

    /// TODO
    voice_t voice_state[VOICE_COUNT];

    /// TODO
    int clock_envelope(int);

 public:
    /// Initialize a new SonySPC700.
    ///
    /// @param ram TODO
    ///
    /// @details
    /// Keeps pointer to 64K ram
    SonySPC700(uint8_t* ram);

    /// Mute voice n if bit n (1 << n) of mask is clear.
    void mute_voices(int mask);

    /// Clear state and silence everything.
    void reset();

    /// Set gain, where 1.0 is normal. When greater than 1.0, output is
    /// clamped to the 16-bit sample range.
    ///
    /// @param v TODO
    ///
    inline void set_gain(double v) {
        emu_gain = static_cast<int>(v * (1 << EMU_GAIN_BITS));
    }

    /// If true, prevent channels and global volumes from being phase-negated.
    ///
    /// @param disable TODO
    ///
    inline void disable_surround(bool disable) {
        surround_threshold = disable ? 0 : -0x7FFF;
    }

    /// Read/write register 'n', where n ranges from 0 to REGISTER_COUNT - 1.
    ///
    /// @param i TODO
    ///
    inline int read(int i) {
        assert((unsigned) i < REGISTER_COUNT);
        return reg[i];
    }

    /// TODO.
    ///
    /// @param n TODO
    /// @param ? TODO
    ///
    void write(int n, int);

    /// Run DSP for 'count' samples. Write resulting samples to 'buf' if not
    /// NULL.
    ///
    /// @param count TODO
    /// @param buf TODO
    ///
    void run(long count, short* buf = NULL);
};

#endif  // DSP_SONY_SPC700_HPP_
