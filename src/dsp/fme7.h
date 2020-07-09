// Sunsoft FME-7 sound emulator

// Game_Music_Emu 0.5.2
#ifndef NES_FME7_APU_H
#define NES_FME7_APU_H

#include "blip_buffer/blip_buffer.hpp"
#include "blip_buffer/blip_synth.hpp"
#include <cstdint>

class FME7 {
 private:
	enum { reg_count = 14 };
	uint8_t regs [reg_count];
	uint8_t phases [3]; // 0 or 1
	uint8_t latch;
	uint16_t delays [3]; // a, b, c

 public:
	// See Nes_Apu.h for reference
	void reset();
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

	static unsigned char const amp_table [16];

	struct {
		BLIPBuffer* output;
		int last_amp;
	} oscs [OSC_COUNT];
	blip_time_t last_time;

	enum { amp_range = 192 }; // can be any value; this gives best error/quality tradeoff
	BLIPSynth<BLIPQuality::Good, 1> synth;

	void run_until( blip_time_t );
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
