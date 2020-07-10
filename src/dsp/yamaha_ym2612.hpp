// YM2612 FM sound chip emulator interface
// Copyright 2020 Christian Kauten
// Copyright 2006 Shay Green
// Copyright 2001 Jarek Burczynski
// Copyright 1998 Tatsuyuki Satoh
// Copyright 1997 Nicola Salmoria and the MAME team
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
// Version 1.4 (final beta)
//

#ifndef DSP_YM2612_HPP_
#define DSP_YM2612_HPP_

struct Ym2612_Impl;

class Ym2612_Emu  {
    Ym2612_Impl* impl;

 public:
    Ym2612_Emu() { impl = 0; }
    ~Ym2612_Emu();

    // Set output sample rate and chip clock rates, in Hz. Returns non-zero
    // if error.
    const char* set_rate( double sample_rate, double clock_rate );

    // Reset to power-up state
    void reset();

    // Mute voice n if bit n (1 << n) of mask is set
    enum { channel_count = 6 };
    void mute_voices( int mask );

    // Write addr to register 0 then data to register 1
    void write0( int addr, int data );

    // Write addr to register 2 then data to register 3
    void write1( int addr, int data );

    // Run and add pair_count samples into current output buffer contents
    typedef short sample_t;
    enum { out_chan_count = 2 }; // stereo
    void run( int pair_count, sample_t* out );
};

#endif  // DSP_YM2612_HPP_
