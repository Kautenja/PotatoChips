// Private oscillators used by TexasInstrumentsSN76489.
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

#ifndef DSP_TEXAS_INSTRUMENTS_SN76489_OSCILLATORS_HPP_
#define DSP_TEXAS_INSTRUMENTS_SN76489_OSCILLATORS_HPP_

#include "blip_buffer.hpp"

struct TexasInstrumentsSN76489_Osc
{
    BLIPBuffer* outputs [4]; // NULL, right, left, center
    BLIPBuffer* output;
    int output_select;

    int delay;
    int last_amp;
    int volume;

    TexasInstrumentsSN76489_Osc();
    void reset();
};

struct TexasInstrumentsSN76489_Square : TexasInstrumentsSN76489_Osc
{
    int period;
    int phase;

    typedef BLIPSynth<blip_good_quality, 1> Synth;
    const Synth* synth;

    void reset();
    void run(blip_time_t, blip_time_t);
};

struct TexasInstrumentsSN76489_Noise : TexasInstrumentsSN76489_Osc
{
    const int* period;
    unsigned shifter;
    unsigned feedback;

    typedef BLIPSynth<blip_med_quality, 1> Synth;
    Synth synth;

    void reset();
    void run(blip_time_t, blip_time_t);
};

#endif  // DSP_TEXAS_INSTRUMENTS_SN76489_OSCILLATORS_HPP_
