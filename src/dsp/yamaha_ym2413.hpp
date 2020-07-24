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

/// @brief YM2413 chip emulator.
namespace YM2413 {

/// @brief YM2413 chip emulator.
class Emulator  {
    struct OPLL* opll;

 public:
    /// the number of channels on the chip
    enum { channel_count = 14 };

    /// the number of output channels on the chip
    enum { out_chan_count = 2 }; // stereo

    /// a type for 16-bit audio samples.
    typedef short sample_t;

    /// @brief Initialize a new YM2413 emulator.
    Emulator();

    /// @brief Destroy an instance of YM2413 emulator.
    ~Emulator();

    /// @brief Set output sample rate and chip clock rates, in Hz.
    /// @returns non-zero if error.
    int set_rate(double sample_rate, double clock_rate);

    /// @brief Reset the emulator to power-up state.
    void reset();

    /// @brief Mute voice n if bit n (1 << n) of mask is set.
    void mute_voices(int mask);

    /// @brief Write 'data' to 'addr'.
    void write(int addr, int data);

    /// @brief Run and write pair_count samples to output.
    void run(int pair_count, sample_t* out);
};

}  // namespace YM2413

#endif  // DSP_YM2413_HPP_

