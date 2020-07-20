// Atari POKEY sound chip emulator
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

#ifndef DSP_ATARI_POKEY_HPP_
#define DSP_ATARI_POKEY_HPP_

#include "blargg_common.h"
#include "blip_buffer.hpp"

typedef unsigned char byte;

class Sap_Apu_Impl;

class Sap_Apu {
public:
    enum { osc_count = 4 };
    void osc_output(int index, BLIPBuffer*);

    void reset(Sap_Apu_Impl*);

    enum { start_addr = 0xD200 };
    enum { end_addr   = 0xD209 };
    void write_data(blip_time_t, unsigned addr, int data);

    void end_frame(blip_time_t);

public:
    Sap_Apu();
private:
    struct osc_t
    {
        unsigned char regs [2];
        unsigned char phase;
        unsigned char invert;
        int last_amp;
        blip_time_t delay;
        blip_time_t period; // always recalculated before use; here for convenience
        BLIPBuffer* output;
    };
    osc_t oscs [osc_count];
    Sap_Apu_Impl* impl;
    blip_time_t last_time;
    int poly5_pos;
    int poly4_pos;
    int polym_pos;
    int control;

    void calc_periods();
    void run_until(blip_time_t);

    enum { poly4_len  = (1L <<  4) - 1 };
    enum { poly9_len  = (1L <<  9) - 1 };
    enum { poly17_len = (1L << 17) - 1 };
    friend class Sap_Apu_Impl;
};

// Common tables and BLIPSynth that can be shared among multiple Sap_Apu objects
class Sap_Apu_Impl {
public:
    BLIPSynth<blip_good_quality, 1> synth;

    Sap_Apu_Impl();
    void volume(double d) { synth.volume(1.0 / Sap_Apu::osc_count / 30 * d); }

private:
    typedef unsigned char byte;
    byte poly4  [Sap_Apu::poly4_len  / 8 + 1];
    byte poly9  [Sap_Apu::poly9_len  / 8 + 1];
    byte poly17 [Sap_Apu::poly17_len / 8 + 1];
    friend class Sap_Apu;
};

inline void Sap_Apu::osc_output(int i, BLIPBuffer* b)
{
    assert((unsigned) i < osc_count);
    oscs [i].output = b;
}

#endif  // DSP_ATARI_POKEY_HPP_
