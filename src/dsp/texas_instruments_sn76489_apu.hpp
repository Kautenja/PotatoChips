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
    void volume(double);

    // Set treble equalization
    void treble_eq(const blip_eq_t&);

    // Outputs can be assigned to a single buffer for mono output, or to three
    // buffers for stereo output (using Stereo_Buffer to do the mixing).

    // Assign all oscillator outputs to specified buffer(s). If buffer
    // is NULL, silences all oscillators.
    void output(BLIPBuffer* mono);
    void output(BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right);

    // Assign single oscillator output to buffer(s). Valid indicies are 0 to 3,
    // which refer to Square 1, Square 2, Square 3, and Noise. If buffer is NULL,
    // silences oscillator.
    enum { OSC_COUNT = 4 };
    void osc_output(int index, BLIPBuffer* mono);
    void osc_output(int index, BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right);

    // Reset oscillators and internal state
    void reset(unsigned noise_feedback = 0, int noise_width = 0);

    // Write to data port
    void write_data(blip_time_t, int);

    // Run all oscillators up to specified time, end current frame, then
    // start a new frame at time 0.
    void end_frame(blip_time_t);

 public:
    TexasInstrumentsSN76489();
    ~TexasInstrumentsSN76489();

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

    void run_until(blip_time_t);
};

struct sms_apu_state_t {
    unsigned char regs [8] [2];
    unsigned char latch;
};

inline void TexasInstrumentsSN76489::output(BLIPBuffer* b) { output(b, b, b); }

inline void TexasInstrumentsSN76489::osc_output(int i, BLIPBuffer* b) { osc_output(i, b, b, b); }

#endif  // DSP_TEXAS_INSTRUMENTS_SN76489_APU_HPP_
