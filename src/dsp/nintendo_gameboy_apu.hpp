// Nintendo Game Boy PAPU sound chip emulator
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

#ifndef DSP_NINTENDO_GAMEBOY_APU_HPP_
#define DSP_NINTENDO_GAMEBOY_APU_HPP_

#include "nintendo_gameboy_oscillators.hpp"
#include <iostream>

/// Registers for the Nintendo GameBoy Sound System (GBS) APU.
enum GBS_Registers {
    // Pulse 0
    PULSE0_SWEEP_PERIOD               = 0xFF10,
    PULSE0_DUTY_LENGTH_LOAD           = 0xFF11,
    PULSE0_START_VOLUME               = 0xFF12,
    PULSE0_FREQ_LO                    = 0xFF13,
    PULSE0_TRIG_LENGTH_ENABLE_HI      = 0xFF14,
    // Pulse 1
    // PULSE1_UNUSED                     = 0xFF15,
    PULSE1_DUTY_LENGTH_LOAD           = 0xFF16,
    PULSE1_START_VOLUME               = 0xFF17,
    PULSE1_FREQ_LO                    = 0xFF18,
    PULSE1_TRIG_LENGTH_ENABLE_FREQ_HI = 0xFF19,
    // Wave
    WAVE_DAC_POWER                    = 0xFF1A,
    WAVE_LENGTH_LOAD                  = 0xFF1B,
    WAVE_VOLUME_CODE                  = 0xFF1C,
    WAVE_FREQ_LO                      = 0xFF1D,
    WAVE_TRIG_LENGTH_ENABLE_FREQ_HI   = 0xFF1E,
    // Noise
    // NOISE_UNUSED                      = 0xFF1F,
    NOISE_LENGTH_LOAD                 = 0xFF20,
    NOISE_START_VOLUME                = 0xFF21,
    NOISE_CLOCK_SHIFT                 = 0xFF22,
    NOISE_TRIG_LENGTH_ENABLE          = 0xFF23,
    // Control / Status
    STEREO_VOLUME                     = 0xFF24,
    STEREO_ENABLES                    = 0xFF25,
    POWER_CONTROL_STATUS              = 0xFF26,
    // wave-table for wave channel
    WAVE_TABLE_VALUES                 = 0xFF30
};

/// the default values for the wave-table
const uint8_t sine_wave[32] = {
    0xA,0x8,0xD,0xC,0xE,0xE,0xF,0xF,0xF,0xF,0xE,0xF,0xD,0xE,0xA,0xC,
    0x5,0x8,0x2,0x3,0x1,0x1,0x0,0x0,0x0,0x0,0x1,0x0,0x2,0x1,0x5,0x3
};

/// The Nintendo GameBoy Sound System (GBS) Audio Processing Unit (APU).
class Gb_Apu {
 public:
    Gb_Apu();

    // Set overall volume of all oscillators, where 1.0 is full volume
    void volume(double);

    // Set treble equalization
    void treble_eq(const blip_eq_t&);

    // Outputs can be assigned to a single buffer for mono output, or to three
    // buffers for stereo output (using Stereo_Buffer to do the mixing).

    // Assign all oscillator outputs to specified buffer(s). If buffer
    // is NULL, silences all oscillators.
    void output(BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right);
    inline void output(BLIPBuffer* mono) {
        output(mono, mono, mono);
    }

    // Assign single oscillator output to buffer(s). Valid indicies are 0 to 3,
    // which refer to Square 1, Square 2, Wave, and Noise. If buffer is NULL,
    // silences oscillator.
    enum { OSC_COUNT = 4 };
    void osc_output(int index, BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right);
    inline void osc_output(int index, BLIPBuffer* mono) {
        osc_output(index, mono, mono, mono);
    }

    // Reset oscillators and internal state
    void reset();

    // Reads and writes at addr must satisfy ADDR_START <= addr <= ADDR_END
    enum { ADDR_START = 0xFF10 };
    enum { ADDR_END   = 0xFF3F };
    enum { REGISTER_COUNT = ADDR_END - ADDR_START + 1 };

    // Write 'data' to address at specified time
    void write_register(blip_time_t, unsigned addr, int data);

    // Read from address at specified time
    int read_register(blip_time_t, unsigned addr);

    // Run all oscillators up to specified time, end current time frame, then
    // start a new frame at time 0.
    void end_frame(blip_time_t);

    void set_tempo(double);

 private:
    // noncopyable
    Gb_Apu(const Gb_Apu&);
    Gb_Apu& operator = (const Gb_Apu&);

    Gb_Osc* oscs[OSC_COUNT];
    blip_time_t next_frame_time;
    blip_time_t last_time;
    blip_time_t frame_period;
    double volume_unit;
    int frame_count;

    Gb_Square square1;
    Gb_Square square2;
    Gb_Wave wave;
    Gb_Noise noise;
    uint8_t regs[REGISTER_COUNT];
    // used by squares
    Gb_Square::Synth square_synth;
    // used by wave and noise
    Gb_Wave::Synth other_synth;

    void update_volume();
    void run_until(blip_time_t);
    void write_osc(int index, int reg, int data);
};

// inline void Gb_Apu::output(BLIPBuffer* b)

// inline void Gb_Apu::osc_output(int i, BLIPBuffer* b) {
//     osc_output(i, b, b, b);
// }

inline void Gb_Apu::volume(double vol) {
    volume_unit = 0.60 / OSC_COUNT / 15 /*steps*/ / 2 /*?*/ / 8 /*master vol range*/ * vol;
    update_volume();
}

#endif  // DSP_NINTENDO_GAMEBOY_APU_HPP_
