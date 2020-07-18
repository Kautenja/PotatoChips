// Turbo Grafx 16 (PC Engine) PSG sound chip emulator
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

#ifndef DSP_NEC_TURBO_GRAFX_16_APU_HPP_
#define DSP_NEC_TURBO_GRAFX_16_APU_HPP_

#include "blargg_common.h"
#include "blip_buffer.hpp"

struct Hes_Osc
{
    unsigned char wave [32];
    short volume [2];
    int last_amp [2];
    int delay;
    int period;
    unsigned char noise;
    unsigned char phase;
    unsigned char balance;
    unsigned char dac;
    blip_time_t last_time;

    BLIPBuffer* outputs [2];
    BLIPBuffer* chans [3];
    unsigned noise_lfsr;
    unsigned char control;

    enum { amp_range = 0x8000 };
    typedef BLIPSynth<blip_med_quality,1> synth_t;

    void run_until( synth_t& synth, blip_time_t );
};

class Hes_Apu {
public:
    void treble_eq( blip_eq_t const& );
    void volume( double );

    enum { osc_count = 6 };
    void osc_output( int index, BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right );

    void reset();

    enum { start_addr = 0x0800 };
    enum { end_addr   = 0x0809 };
    void write_data( blip_time_t, int addr, int data );

    void end_frame( blip_time_t );

public:
    Hes_Apu();
private:
    Hes_Osc oscs [osc_count];
    int latch;
    int balance;
    Hes_Osc::synth_t synth;

    void balance_changed( Hes_Osc& );
    void recalc_chans();
};

inline void Hes_Apu::volume( double v ) { synth.volume( 1.8 / osc_count / Hes_Osc::amp_range * v ); }

inline void Hes_Apu::treble_eq( blip_eq_t const& eq ) { synth.treble_eq( eq ); }

#endif  // DSP_NEC_TURBO_GRAFX_16_APU_HPP_
