// Sega Master System SN76489 PSG sound chip emulator
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

#ifndef SMS_APU_H
#define SMS_APU_H

#include "texas_instruments_sn76489_oscillators.hpp"

class Sms_Apu {
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
	// which refer to Square 1, Square 2, Square 3, and Noise. If buffer is NULL,
	// silences oscillator.
	enum { osc_count = 4 };
	void osc_output( int index, BLIPBuffer* mono );
	void osc_output( int index, BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right );

	// Reset oscillators and internal state
	void reset( unsigned noise_feedback = 0, int noise_width = 0 );

	// Write GameGear left/right assignment byte
	void write_ggstereo( blip_time_t, int );

	// Write to data port
	void write_data( blip_time_t, int );

	// Run all oscillators up to specified time, end current frame, then
	// start a new frame at time 0.
	void end_frame( blip_time_t );

public:
	Sms_Apu();
	~Sms_Apu();
private:
	// noncopyable
	Sms_Apu( const Sms_Apu& );
	Sms_Apu& operator = ( const Sms_Apu& );

	Sms_Osc*    oscs [osc_count];
	Sms_Square  squares [3];
	Sms_Square::Synth square_synth; // used by squares
	blip_time_t last_time;
	int         latch;
	Sms_Noise   noise;
	unsigned    noise_feedback;
	unsigned    looped_feedback;

	void run_until( blip_time_t );
};

struct sms_apu_state_t
{
	unsigned char regs [8] [2];
	unsigned char latch;
};

inline void Sms_Apu::output( BLIPBuffer* b ) { output( b, b, b ); }

inline void Sms_Apu::osc_output( int i, BLIPBuffer* b ) { osc_output( i, b, b, b ); }

#endif
