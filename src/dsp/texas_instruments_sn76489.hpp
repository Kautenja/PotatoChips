// Sega Master System SN76489 programmable sound generator sound chip emulator.
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

#ifndef DSP_TEXAS_INSTRUMENTS_SN76489_APU_HPP_
#define DSP_TEXAS_INSTRUMENTS_SN76489_APU_HPP_

#include "texas_instruments_sn76489_oscillators.hpp"

/// the registers on the SN76489
enum TexasInstrumentsSN76489_Registers {
    TONE_1_FREQUENCY   = 0b10000000,
    TONE_1_ATTENUATION = 0b10010000,
    TONE_2_FREQUENCY   = 0b10100000,
    TONE_2_ATTENUATION = 0b10110000,
    TONE_3_FREQUENCY   = 0b11000000,
    TONE_3_ATTENUATION = 0b11010000,
    NOISE_CONTROL      = 0b11100000,
    NOISE_ATTENUATION  = 0b11110000
};

/// the values for the linear feedback shift register to take.
enum TexasInstrumentsSN76489_LFSR_Values {
    N_512    = 0b00,  // N / 512
    N_1024   = 0b01,  // N / 1024
    N_2048   = 0b10,  // N / 2048
    N_TONE_3 = 0b11   // Tone Generator # Output
};

/// the FB bit in the Noise control register
static constexpr uint8_t NOISE_FEEDBACK = 0b00000100;

/// Sega Master System SN76489 programmable sound generator sound chip emulator.
class TexasInstrumentsSN76489 {
public:
    // Set overall volume of all oscillators, where 1.0 is full volume
    void volume(double vol) {
        vol *= 0.85 / (OSC_COUNT * 64 * 2);
        square_synth.volume(vol);
        noise.synth.volume(vol);
    }

    // Set treble equalization
    void treble_eq(const blip_eq_t& eq) {
        square_synth.treble_eq(eq);
        noise.synth.treble_eq(eq);
    }

    // Assign all oscillator outputs to specified buffer. If buffer
    // is NULL, silences all oscillators.
    void output(BLIPBuffer* output) {
        for (int i = 0; i < OSC_COUNT; i++)
            osc_output(i, output);
    }

    // Assign single oscillator output to buffer. If buffer
    // is NULL, silences the given oscillator.
    enum { OSC_COUNT = 4 };
    void osc_output(int index, BLIPBuffer* output) {
        assert((unsigned) index < OSC_COUNT);
        TexasInstrumentsSN76489_Osc& osc = *oscs[index];
        osc.output = output;
    }

    // Reset oscillators and internal state
    void reset(unsigned feedback = 0, int noise_width = 0) {
        last_time = 0;
        latch = 0;

        if (!feedback || !noise_width) {
            feedback = 0x0009;
            noise_width = 16;
        }
        // convert to "Galios configuration"
        looped_feedback = 1 << (noise_width - 1);
        noise_feedback  = 0;
        while (noise_width--) {
            noise_feedback = (noise_feedback << 1) | (feedback & 1);
            feedback >>= 1;
        }

        squares[0].reset();
        squares[1].reset();
        squares[2].reset();
        noise.reset();
    }

    // Write to data port
    void write_data(blip_time_t time, int data) {
        static constexpr unsigned char volumes[16] = {
            64, 50, 39, 31, 24, 19, 15, 12, 9, 7, 5, 4, 3, 2, 1, 0
        };

        assert((unsigned) data <= 0xFF);

        run_until(time);

        if (data & 0x80)
            latch = data;

        int index = (latch >> 5) & 3;
        if (latch & 0x10) {
            oscs[index]->volume = volumes[data & 15];
        }
        else if (index < 3) {
            TexasInstrumentsSN76489_Square& sq = squares[index];
            if (data & 0x80)
                sq.period = (sq.period & 0xFF00) | (data << 4 & 0x00FF);
            else
                sq.period = (sq.period & 0x00FF) | (data << 8 & 0x3F00);
        }
        else
        {
            int select = data & 3;
            if (select < 3)
                noise.period = &noise_periods[select];
            else
                noise.period = &squares[2].period;

            noise.feedback = (data & 0x04) ? noise_feedback : looped_feedback;
            noise.shifter = 0x8000;
        }
    }

    // Run all oscillators up to specified time, end current frame, then
    // start a new frame at time 0.
    inline void end_frame(blip_time_t end_time) {
        if (end_time > last_time) run_until(end_time);
        assert(last_time >= end_time);
        last_time -= end_time;
    }

 public:
    /// Create a new instance of TexasInstrumentsSN76489.
    TexasInstrumentsSN76489() {
        for (int i = 0; i < 3; i++) {
            squares[i].synth = &square_synth;
            oscs[i] = &squares[i];
        }
        oscs[3] = &noise;
        volume(1.0);
        reset();
    }

    /// Destroy this instance of TexasInstrumentsSN76489.
    ~TexasInstrumentsSN76489() { }

 private:
    /// Disable the copy constructor.
    TexasInstrumentsSN76489(const TexasInstrumentsSN76489&);
    /// Disable the assignment operator
    TexasInstrumentsSN76489& operator = (const TexasInstrumentsSN76489&);

    TexasInstrumentsSN76489_Osc*    oscs [OSC_COUNT];
    TexasInstrumentsSN76489_Square  squares [3];
    TexasInstrumentsSN76489_Square::Synth square_synth; // used by squares
    blip_time_t last_time;
    int         latch;
    TexasInstrumentsSN76489_Noise   noise;
    unsigned    noise_feedback;
    unsigned    looped_feedback;

    void run_until(blip_time_t end_time) {
        assert(end_time >= last_time); // end_time must not be before previous time
        if (end_time > last_time) {
            // run oscillators
            for (int i = 0; i < OSC_COUNT; ++i) {
                TexasInstrumentsSN76489_Osc& osc = *oscs[i];
                if (osc.output) {
                    if (i < 3)
                        squares[i].run(last_time, end_time);
                    else
                        noise.run(last_time, end_time);
                }
            }
            last_time = end_time;
        }
    }
};

struct sms_apu_state_t {
    unsigned char regs [8] [2];
    unsigned char latch;
};

#endif  // DSP_TEXAS_INSTRUMENTS_SN76489_APU_HPP_
