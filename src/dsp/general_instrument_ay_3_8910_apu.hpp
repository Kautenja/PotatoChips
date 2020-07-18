// General Instrument AY-3-8910 sound chip emulator.
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

#ifndef GENERAL_INSTRUMENT_AY_3_8910_APU_HPP_
#define GENERAL_INSTRUMENT_AY_3_8910_APU_HPP_

#include "blargg_common.h"
#include "blip_buffer.hpp"

typedef unsigned char byte;

class Ay_Apu {
 public:
    // Set buffer to generate all sound into, or disable sound if NULL
    void output( BLIPBuffer* );

    // Reset sound chip
    void reset();

    // Write to register at specified time
    enum { reg_count = 16 };
    void write( blip_time_t time, int addr, int data );

    // Run sound to specified time, end current time frame, then start a new
    // time frame at time 0. Time frames have no effect on emulation and each
    // can be whatever length is convenient.
    void end_frame( blip_time_t length );

// Additional features

    // Set sound output of specific oscillator to buffer, where index is
    // 0, 1, or 2. If buffer is NULL, the specified oscillator is muted.
    enum { osc_count = 3 };
    void osc_output( int index, BLIPBuffer* );

    // Set overall volume (default is 1.0)
    void volume( double );

    // Set treble equalization (see documentation)
    void treble_eq( blip_eq_t const& );

public:
    Ay_Apu();
    typedef unsigned char byte;
private:
    struct osc_t
    {
        blip_time_t period;
        blip_time_t delay;
        short last_amp;
        short phase;
        BLIPBuffer* output;
    } oscs [osc_count];
    blip_time_t last_time;
    byte latch;
    byte regs [reg_count];

    struct {
        blip_time_t delay;
        blargg_ulong lfsr;
    } noise;

    struct {
        blip_time_t delay;
        byte const* wave;
        int pos;
        byte modes [8] [48]; // values already passed through volume table
    } env;

    void run_until( blip_time_t );
    void write_data_( int addr, int data );
public:
    enum { amp_range = 255 };
    BLIPSynth<blip_good_quality, 1> synth_;
};

inline void Ay_Apu::volume( double v ) { synth_.volume( 0.7 / osc_count / amp_range * v ); }

inline void Ay_Apu::treble_eq( blip_eq_t const& eq ) { synth_.treble_eq( eq ); }

inline void Ay_Apu::write( blip_time_t time, int addr, int data )
{
    run_until( time );
    write_data_( addr, data );
}

inline void Ay_Apu::osc_output( int i, BLIPBuffer* buf )
{
    assert( (unsigned) i < osc_count );
    oscs [i].output = buf;
}

inline void Ay_Apu::output( BLIPBuffer* buf )
{
    osc_output( 0, buf );
    osc_output( 1, buf );
    osc_output( 2, buf );
}

inline void Ay_Apu::end_frame( blip_time_t time )
{
    if ( time > last_time )
        run_until( time );

    assert( last_time >= time );
    last_time -= time;
}

#endif  // GENERAL_INSTRUMENT_AY_3_8910_APU_HPP_
