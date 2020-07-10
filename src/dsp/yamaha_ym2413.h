// YM2413 emulator written by Mitsutaka Okazaki 2001
// Copyright 2020 Christian Kauten
// Copyright 2006 Shay Green
// Copyright 2001 Mitsutaka Okazaki
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
// Version 0.61
//

#ifndef DSP_YM2413_HPP_
#define DSP_YM2413_HPP_

class Ym2413_Emu  {
	struct OPLL* opll;

 public:
	Ym2413_Emu();
	~Ym2413_Emu();

	// Set output sample rate and chip clock rates, in Hz. Returns non-zero
	// if error.
	int set_rate( double sample_rate, double clock_rate );

	// Reset to power-up state
	void reset();

	// Mute voice n if bit n (1 << n) of mask is set
	enum { channel_count = 14 };
	void mute_voices( int mask );

	// Write 'data' to 'addr'
	void write( int addr, int data );

	// Run and write pair_count samples to output
	typedef short sample_t;
	enum { out_chan_count = 2 }; // stereo
	void run( int pair_count, sample_t* out );
};

#endif  // DSP_YM2413_HPP_

