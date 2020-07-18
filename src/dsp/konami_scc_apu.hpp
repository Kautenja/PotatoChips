// Konami SCC sound chip emulator
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

#ifndef DSP_KONAMI_SCC_APU_HPP_
#define DSP_KONAMI_SCC_APU_HPP_

#include "blargg_common.h"
#include "blip_buffer.hpp"
#include <cstring>

class Scc_Apu {
public:
    // Set buffer to generate all sound into, or disable sound if NULL
    void output(BLIPBuffer* );

    // Reset sound chip
    void reset();

    // Write to register at specified time
    enum { reg_count = 0x90 };
    void write(blip_time_t time, int reg, int data );

    // Run sound to specified time, end current time frame, then start a new
    // time frame at time 0. Time frames have no effect on emulation and each
    // can be whatever length is convenient.
    void end_frame(blip_time_t length );

// Additional features

    // Set sound output of specific oscillator to buffer, where index is
    // 0 to 4. If buffer is NULL, the specified oscillator is muted.
    enum { osc_count = 5 };
    void osc_output(int index, BLIPBuffer* );

    // Set overall volume (default is 1.0)
    void volume(double );

    // Set treble equalization (see documentation)
    void treble_eq(blip_eq_t const& );

public:
    Scc_Apu();
private:
    enum { amp_range = 0x8000 };
    struct osc_t
    {
        int delay;
        int phase;
        int last_amp;
        BLIPBuffer* output;
    };
    osc_t oscs [osc_count];
    blip_time_t last_time;
    unsigned char regs [reg_count];
    BLIPSynth<blip_med_quality, 1> synth;

    void run_until(blip_time_t );
};

inline void Scc_Apu::volume(double v ) { synth.volume(0.43 / osc_count / amp_range * v ); }

inline void Scc_Apu::treble_eq(blip_eq_t const& eq ) { synth.treble_eq(eq ); }

inline void Scc_Apu::osc_output(int index, BLIPBuffer* b )
{
    assert((unsigned) index < osc_count );
    oscs [index].output = b;
}

inline void Scc_Apu::write(blip_time_t time, int addr, int data )
{
    assert((unsigned) addr < reg_count );
    run_until(time );
    regs [addr] = data;
}

inline void Scc_Apu::end_frame(blip_time_t end_time )
{
    if (end_time > last_time )
        run_until(end_time );
    last_time -= end_time;
    assert(last_time >= 0 );
}

inline void Scc_Apu::output(BLIPBuffer* buf )
{
    for (int i = 0; i < osc_count; i++ )
        oscs [i].output = buf;
}

inline Scc_Apu::Scc_Apu()
{
    output(0 );
}

inline void Scc_Apu::reset()
{
    last_time = 0;

    for (int i = 0; i < osc_count; i++ )
        memset(&oscs [i], 0, offsetof (osc_t,output) );

    memset(regs, 0, sizeof regs );
}

#endif  // DSP_KONAMI_SCC_APU_HPP_
