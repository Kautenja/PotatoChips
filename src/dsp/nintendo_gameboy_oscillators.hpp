// Private oscillators used by NintendoGBS
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

#ifndef DSP_NINTENDO_GAMEBOY_OSCILLATORS_HPP_
#define DSP_NINTENDO_GAMEBOY_OSCILLATORS_HPP_

#include "blip_buffer.hpp"

struct NintendoGBS_Oscillator {
    enum { trigger = 0x80 };
    enum { len_enabled_mask = 0x40 };

    BLIPBuffer* outputs[4];  // NULL, right, left, center
    BLIPBuffer* output;
    int output_select;
    uint8_t* regs;  // osc's 5 registers

    int delay;
    int last_amp;
    int volume;
    int length;
    int enabled;

    void reset();

    void clock_length();

    inline int frequency() const {
        return (regs[4] & 7) * 0x100 + regs[3];
    }
};

struct NintendoGBS_Envelope : NintendoGBS_Oscillator {
    int env_delay;

    void reset();
    void clock_envelope();
    bool write_register(int, int);
};

struct NintendoGBS_Pulse : NintendoGBS_Envelope {
    enum { period_mask = 0x70 };
    enum { shift_mask  = 0x07 };

    typedef BLIPSynth<blip_good_quality, 1> Synth;
    Synth const* synth;
    int sweep_delay;
    int sweep_freq;
    int phase;

    void reset();
    void clock_sweep();
    void run(blip_time_t, blip_time_t, int playing);
};

struct NintendoGBS_Noise : NintendoGBS_Envelope {
    typedef BLIPSynth<blip_med_quality, 1> Synth;
    Synth const* synth;
    unsigned bits;

    void run(blip_time_t, blip_time_t, int playing);
};

struct NintendoGBS_Wave : NintendoGBS_Oscillator {
    typedef BLIPSynth<blip_med_quality, 1> Synth;
    Synth const* synth;
    int wave_pos;
    enum { wave_size = 32 };
    uint8_t wave[wave_size];

    void write_register(int, int);
    void run(blip_time_t, blip_time_t, int playing);
};

inline void NintendoGBS_Envelope::reset() {
    env_delay = 0;
    NintendoGBS_Oscillator::reset();
}

#endif  // DSP_NINTENDO_GAMEBOY_OSCILLATORS_HPP_
