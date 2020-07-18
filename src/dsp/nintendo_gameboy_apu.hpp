// Nintendo Game Boy PAPU sound chip emulator
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

#ifndef DSP_NINTENDO_GAMEBOY_APU_HPP_
#define DSP_NINTENDO_GAMEBOY_APU_HPP_

#include "nintendo_gameboy_oscillators.hpp"

class Gb_Apu {
public:

    // Set overall volume of all oscillators, where 1.0 is full volume
    void volume( double );

    // Set treble equalization
    void treble_eq( const blip_eq_t& );

    // Outputs can be assigned to a single buffer for mono output, or to three
    // buffers for stereo output (using Stereo_Buffer to do the mixing).

    // Assign all oscillator outputs to specified buffer(s). If buffer
    // is NULL, silences all oscillators.
    void output( BLIPBuffer* mono );
    void output( BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right );

    // Assign single oscillator output to buffer(s). Valid indicies are 0 to 3,
    // which refer to Square 1, Square 2, Wave, and Noise. If buffer is NULL,
    // silences oscillator.
    enum { osc_count = 4 };
    void osc_output( int index, BLIPBuffer* mono );
    void osc_output( int index, BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right );

    // Reset oscillators and internal state
    void reset();

    // Reads and writes at addr must satisfy start_addr <= addr <= end_addr
    enum { start_addr = 0xFF10 };
    enum { end_addr   = 0xFF3F };
    enum { register_count = end_addr - start_addr + 1 };

    // Write 'data' to address at specified time
    void write_register( blip_time_t, unsigned addr, int data );

    // Read from address at specified time
    int read_register( blip_time_t, unsigned addr );

    // Run all oscillators up to specified time, end current time frame, then
    // start a new frame at time 0.
    void end_frame( blip_time_t );

    void set_tempo( double );

public:
    Gb_Apu();
private:
    // noncopyable
    Gb_Apu( const Gb_Apu& );
    Gb_Apu& operator = ( const Gb_Apu& );

    Gb_Osc*     oscs [osc_count];
    blip_time_t   next_frame_time;
    blip_time_t   last_time;
    blip_time_t frame_period;
    double      volume_unit;
    int         frame_count;

    Gb_Square   square1;
    Gb_Square   square2;
    Gb_Wave     wave;
    Gb_Noise    noise;
    uint8_t regs [register_count];
    Gb_Square::Synth square_synth; // used by squares
    Gb_Wave::Synth   other_synth;  // used by wave and noise

    void update_volume();
    void run_until( blip_time_t );
    void write_osc( int index, int reg, int data );
};

inline void Gb_Apu::output( BLIPBuffer* b ) { output( b, b, b ); }

inline void Gb_Apu::osc_output( int i, BLIPBuffer* b ) { osc_output( i, b, b, b ); }

inline void Gb_Apu::volume( double vol )
{
    volume_unit = 0.60 / osc_count / 15 /*steps*/ / 2 /*?*/ / 8 /*master vol range*/ * vol;
    update_volume();
}

#endif  // DSP_NINTENDO_GAMEBOY_APU_HPP_
