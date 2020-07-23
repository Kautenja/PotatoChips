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

struct TexasInstrumentsSN76489_Osc {
    /// the output buffer to write samples to
    BLIPBuffer* output = 0;
    /// TODO:
    int delay = 0;
    /// the value of the waveform amplitude at the last sample
    int last_amp = 0;
    /// the output volume from the synthesizer
    int volume = 0;

    /// Reset the oscillator to its initial state.
    inline void reset() {
        delay = 0;
        last_amp = 0;
        volume = 0;
    }
};

struct TexasInstrumentsSN76489_Square : TexasInstrumentsSN76489_Osc {
    /// the period of the oscillator
    int period = 0;
    /// the phase of the oscillator
    int phase = 0;
    /// The synthesizer for generating samples from this oscillator
    typedef BLIPSynth<blip_good_quality, 1> Synth;
    const Synth* synth;

    /// Reset the oscillator to its initial state.
    inline void reset() {
        period = 0;
        phase = 0;
        TexasInstrumentsSN76489_Osc::reset();
    }

    /// Run the oscillator from time until end_time.
    void run(blip_time_t time, blip_time_t end_time) {
        if (!volume || period <= 128) {
            // ignore 16kHz and higher
            if (last_amp) {
                synth->offset(time, -last_amp, output);
                last_amp = 0;
            }
            time += delay;
            if (!period) {
                time = end_time;
            } else if (time < end_time) {
                // keep calculating phase
                int count = (end_time - time + period - 1) / period;
                phase = (phase + count) & 1;
                time += count * period;
            }
        } else {
            int amp = phase ? volume : -volume;
            {
                int delta = amp - last_amp;
                if (delta) {
                    last_amp = amp;
                    synth->offset(time, delta, output);
                }
            }

            time += delay;
            if (time < end_time) {
                BLIPBuffer* const output = this->output;
                int delta = amp * 2;
                do {
                    delta = -delta;
                    synth->offset(time, delta, output);
                    time += period;
                    phase ^= 1;
                } while (time < end_time);
                this->last_amp = phase ? volume : -volume;
            }
        }
        delay = time - end_time;
    }
};

struct TexasInstrumentsSN76489_Noise : TexasInstrumentsSN76489_Osc {
    /// the possible noise periods
    static const int noise_periods[3];
    /// the period of the oscillator
    const int* period = &noise_periods[0];
    /// the shift value
    unsigned shifter = 0x8000;
    /// the linear feedback shift registers
    unsigned feedback = 0x9000;
    /// The synthesizer for generating samples from this oscillator
    typedef BLIPSynth<blip_med_quality, 1> Synth;
    Synth synth;

    /// Reset the oscillator to its initial state.
    inline void reset() {
        period = &noise_periods[0];
        shifter = 0x8000;
        feedback = 0x9000;
        TexasInstrumentsSN76489_Osc::reset();
    }

    /// Run the oscillator from time until end_time.
    void run(blip_time_t time, blip_time_t end_time) {
        int amp = volume;
        if (shifter & 1)
            amp = -amp;

        {
            int delta = amp - last_amp;
            if (delta) {
                last_amp = amp;
                synth.offset(time, delta, output);
            }
        }

        time += delay;
        if (!volume)
            time = end_time;

        if (time < end_time) {
            BLIPBuffer* const output = this->output;
            unsigned shifter = this->shifter;
            int delta = amp * 2;
            int period = *this->period * 2;
            if (!period)
                period = 16;

            do {
                int changed = shifter + 1;
                shifter = (feedback & -(shifter & 1)) ^ (shifter >> 1);
                if (changed & 2) // true if bits 0 and 1 differ
                {
                    delta = -delta;
                    synth.offset(time, delta, output);
                }
                time += period;
            } while (time < end_time);

            this->shifter = shifter;
            this->last_amp = delta >> 1;
        }
        delay = time - end_time;
    }
};

const int TexasInstrumentsSN76489_Noise::noise_periods[3] = {0x100, 0x200, 0x400};

#endif  // DSP_TEXAS_INSTRUMENTS_SN76489_OSCILLATORS_HPP_
