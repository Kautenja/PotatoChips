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

/// @brief Yamaha YM2612 chip emulator engine.
struct YamahaYM2612Engine;

/// @brief Yamaha YM2612 chip emulator.
class YamahaYM2612  {
    /// the engine for the emulator
    YamahaYM2612Engine* impl;

 public:
    /// the number of channels on the chip
    enum { channel_count = 6 };

    /// the number of output channels on the chip
    enum { out_chan_count = 2 };

    /// a type for 16-bit audio samples.
    typedef short sample_t;

    /// @brief Initialize a new YM2612 emulator.
    YamahaYM2612() { impl = 0; }

    /// @brief Destroy an instance of YM2612 emulator.
    ~YamahaYM2612();

    /// @brief Set output sample rate and chip clock rates, in Hz.
    /// @returns non-zero if error.
    const char* set_rate(double sample_rate, double clock_rate);

    /// @brief Reset the emulator to power-up state.
    void reset();

    // Mute voice n if bit n (1 << n) of mask is set
    void mute_voices(int mask);

    /// @brief Write addr to register 0 then data to register 1
    void write0(int addr, int data);

    // @brief Write addr to register 2 then data to register 3
    void write1(int addr, int data);

    /// @brief Run and write pair_count samples to output.
    void run(int pair_count, sample_t* out);
};

#endif  // DSP_YM2612_HPP_
