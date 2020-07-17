// An oscillator based on the Sunsoft FME7 synthesis chip.
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

#ifndef NES_FME7_APU_HPP_
#define NES_FME7_APU_HPP_

#include "blip_buffer/blip_buffer.hpp"

/// the IO registers on the FME7.
enum IORegisters {
    PULSE_A_LO   = 0x00,
    PULSE_A_HI   = 0x01,
    PULSE_B_LO   = 0x02,
    PULSE_B_HI   = 0x03,
    PULSE_C_LO   = 0x04,
    PULSE_C_HI   = 0x05,
    NOISE_PERIOD = 0x06,
    NOISE_TONE   = 0x07,
    PULSE_A_ENV  = 0x08,
    PULSE_B_ENV  = 0x09,
    PULSE_C_ENV  = 0x0A,
    ENV_LO       = 0x0B,
    ENV_HI       = 0x0C,
    ENV_RESET    = 0x0D,
    IO_PORT_A    = 0x0E,  // unused
    IO_PORT_B    = 0x0F   // unused
};

// can be any value; this gives best error/quality tradeoff
enum { amp_range = 192 };

// static unsigned char const amp_table [16];
static constexpr unsigned char amp_table[16] =
{
	#define ENTRY( n ) (unsigned char) (n * amp_range + 0.5)
	ENTRY(0.0000), ENTRY(0.0078), ENTRY(0.0110), ENTRY(0.0156),
	ENTRY(0.0221), ENTRY(0.0312), ENTRY(0.0441), ENTRY(0.0624),
	ENTRY(0.0883), ENTRY(0.1249), ENTRY(0.1766), ENTRY(0.2498),
	ENTRY(0.3534), ENTRY(0.4998), ENTRY(0.7070), ENTRY(1.0000)
	#undef ENTRY
};

class FME7 {
 private:
	enum { reg_count = 14 };
	uint8_t regs [reg_count];
	uint8_t phases [3]; // 0 or 1
	uint8_t latch;
	uint16_t delays [3]; // a, b, c

 public:
	// See Nes_Apu.h for reference
	void reset() {
		last_time = 0;
		for (int i = 0; i < OSC_COUNT; i++) oscs[i].last_amp = 0;
	}

	void volume( double );
	void treble_eq( blip_eq_t const& );
	void output( BLIPBuffer* );
	enum { OSC_COUNT = 3 };
	void osc_output( int index, BLIPBuffer* );
	void end_frame( blip_time_t );

	// Mask and addresses of registers
	enum { addr_mask = 0xE000 };
	enum { data_addr = 0xE000 };
	enum { latch_addr = 0xC000 };

	// (addr & addr_mask) == latch_addr
	void write_latch( int );

	// (addr & addr_mask) == data_addr
	void write_data( blip_time_t, int data );

 public:
	FME7();

 private:
	// noncopyable
	FME7( const FME7& );
	FME7& operator = ( const FME7& );

	struct {
		BLIPBuffer* output;
		int last_amp;
	} oscs [OSC_COUNT];
	blip_time_t last_time;

	BLIPSynth<blip_good_quality, 1> synth;

	void run_until(blip_time_t end_time) {
		// require( end_time >= last_time );

		for ( int index = 0; index < OSC_COUNT; index++ )
		{
			// int mode = regs [7] >> index;
			int vol_mode = regs [010 + index];
			int volume = amp_table [vol_mode & 0x0F];

			BLIPBuffer* const osc_output = oscs [index].output;
			if ( !osc_output )
				continue;

			// period
			int const period_factor = 16;
			unsigned period = (regs [index * 2 + 1] & 0x0F) * 0x100 * period_factor +
					regs [index * 2] * period_factor;
			if ( period < 50 ) // around 22 kHz
			{
				volume = 0;
				if ( !period ) // on my AY-3-8910A, period doesn't have extra one added
					period = period_factor;
			}

			// current amplitude
			int amp = volume;
			if ( !phases [index] )
				amp = 0;
			{
				int delta = amp - oscs [index].last_amp;
				if ( delta )
				{
					oscs [index].last_amp = amp;
					synth.offset( last_time, delta, osc_output );
				}
			}

			blip_time_t time = last_time + delays [index];
			if ( time < end_time )
			{
				int delta = amp * 2 - volume;
				if ( volume )
				{
					do
					{
						delta = -delta;
						synth.offset( time, delta, osc_output );
						time += period;
					}
					while ( time < end_time );

					oscs [index].last_amp = (delta + volume) >> 1;
					phases [index] = (delta > 0);
				}
				else
				{
					// maintain phase when silent
					int count = (end_time - time + period - 1) / period;
					phases [index] ^= count & 1;
					time += (long) count * period;
				}
			}

			delays [index] = time - end_time;
		}

		last_time = end_time;
	}
};

inline void FME7::volume( double v )
{
	synth.volume( 0.38 / amp_range * v ); // to do: fine-tune
}

inline void FME7::treble_eq( blip_eq_t const& eq )
{
	synth.treble_eq( eq );
}

inline void FME7::osc_output( int i, BLIPBuffer* buf )
{
	assert( (unsigned) i < OSC_COUNT );
	oscs [i].output = buf;
}

inline void FME7::output( BLIPBuffer* buf )
{
	for ( int i = 0; i < OSC_COUNT; i++ )
		osc_output( i, buf );
}

inline FME7::FME7()
{
	output( NULL );
	volume( 1.0 );
	reset();
}

inline void FME7::write_latch( int data ) { latch = data; }

inline void FME7::write_data( blip_time_t time, int data )
{
	if ( (unsigned) latch >= reg_count )
	{
		#ifdef dprintf
			dprintf( "FME7 write to %02X (past end of sound registers)\n", (int) latch );
		#endif
		return;
	}

	run_until( time );
	regs [latch] = data;
}

inline void FME7::end_frame( blip_time_t time )
{
	if ( time > last_time )
		run_until( time );

	assert( last_time >= time );
	last_time -= time;
}

#endif
