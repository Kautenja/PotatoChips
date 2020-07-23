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

#ifndef DSP_NINTENDO_GAMEBOY_HPP_
#define DSP_NINTENDO_GAMEBOY_HPP_

#include "nintendo_gameboy_oscillators.hpp"
#include <algorithm>
#include <cstring>

// TODO: remove (use global wavetable header)

/// the default values for the wave-table
const uint8_t sine_wave[32] = {
    0xA,0x8,0xD,0xC,0xE,0xE,0xF,0xF,0xF,0xF,0xE,0xF,0xD,0xE,0xA,0xC,
    0x5,0x8,0x2,0x3,0x1,0x1,0x0,0x0,0x0,0x0,0x1,0x0,0x2,0x1,0x5,0x3
};

/// The Nintendo GameBoy Sound System (GBS) Audio Processing Unit (APU).
class NintendoGBS {
 public:
    /// Registers for the Nintendo GameBoy Sound System (GBS) APU.
    enum Registers {
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

    NintendoGBS() {
        square1.synth = &square_synth;
        square2.synth = &square_synth;
        wave.synth  = &other_synth;
        noise.synth = &other_synth;

        oscs[0] = &square1;
        oscs[1] = &square2;
        oscs[2] = &wave;
        oscs[3] = &noise;

        for (int i = 0; i < OSC_COUNT; i++) {
            NintendoGBS_Oscillator& osc = *oscs[i];
            osc.regs = &regs[i * 5];
            osc.output = 0;
            osc.outputs[0] = 0;
            osc.outputs[1] = 0;
            osc.outputs[2] = 0;
            osc.outputs[3] = 0;
        }

        set_tempo(1.0);
        volume(1.0);
        reset();
    }

    // Set overall volume of all oscillators, where 1.0 is full volume
    void volume(double vol) {
        // 15: steps
        // 2: ?
        // 8: master volume range
        volume_unit = 0.60 / OSC_COUNT / 15 / 2 / 8 * vol;
        update_volume();
    }

    // Set treble equalization
    void treble_eq(const blip_eq_t& eq) {
        square_synth.treble_eq(eq);
        other_synth.treble_eq(eq);
    }

    // Outputs can be assigned to a single buffer for mono output, or to three
    // buffers for stereo output (using Stereo_Buffer to do the mixing).

    // Assign all oscillator outputs to specified buffer(s). If buffer
    // is NULL, silences all oscillators.
    inline void output(BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right) {
        for (int i = 0; i < OSC_COUNT; i++) osc_output(i, center, left, right);
    }

    inline void output(BLIPBuffer* mono) {
        output(mono, mono, mono);
    }

    // Assign single oscillator output to buffer(s). Valid indicies are 0 to 3,
    // which refer to Square 1, Square 2, Wave, and Noise. If buffer is NULL,
    // silences oscillator.
    enum { OSC_COUNT = 4 };
    inline void osc_output(int index, BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right) {
        assert((unsigned) index < OSC_COUNT);
        assert((center && left && right) || (!center && !left && !right));
        NintendoGBS_Oscillator& osc = *oscs[index];
        osc.outputs[1] = right;
        osc.outputs[2] = left;
        osc.outputs[3] = center;
        osc.output = osc.outputs[osc.output_select];
    }

    inline void osc_output(int index, BLIPBuffer* mono) {
        osc_output(index, mono, mono, mono);
    }

    // Reset oscillators and internal state
    void reset() {
        next_frame_time = 0;
        last_time       = 0;
        frame_count     = 0;

        square1.reset();
        square2.reset();
        wave.reset();
        noise.reset();
        noise.bits = 1;
        wave.wave_pos = 0;

        // avoid click at beginning
        regs[STEREO_VOLUME - ADDR_START] = 0x77;
        update_volume();

        regs[POWER_CONTROL_STATUS - ADDR_START] = 0x01;  // force power
        write(POWER_CONTROL_STATUS, 0x00);

        static constexpr uint8_t initial_wave[] = {
            0x84,0x40,0x43,0xAA,0x2D,0x78,0x92,0x3C, // wave table
            0x60,0x59,0x59,0xB0,0x34,0xB8,0x2E,0xDA
        };
        memcpy(wave.wave, initial_wave, sizeof wave.wave);
    }

    // Reads and writes at addr must satisfy ADDR_START <= addr <= ADDR_END
    enum { ADDR_START = 0xFF10 };
    enum { ADDR_END   = 0xFF3F };
    enum { REGISTER_COUNT = ADDR_END - ADDR_START + 1 };

    // Write 'data' to address at specified time
    void write(unsigned addr, int data) {
        static constexpr int time = 0;
        static constexpr unsigned char powerup_regs[0x20] = {
            0x80,0x3F,0x00,0xFF,0xBF,                     // square 1
            0xFF,0x3F,0x00,0xFF,0xBF,                     // square 2
            0x7F,0xFF,0x9F,0xFF,0xBF,                     // wave
            0xFF,0xFF,0x00,0x00,0xBF,                     // noise
            0x00,                                         // left/right enables
            0x77,                                         // master volume
            0x80,                                         // power
            0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF  // wave-table
        };

        assert((unsigned) data < 0x100);

        int reg = addr - ADDR_START;
        if ((unsigned) reg >= REGISTER_COUNT)
            return;

        run_until(time);

        int old_reg = regs[reg];
        regs[reg] = data;

        if (addr < STEREO_VOLUME) {
            write_osc(reg / 5, reg, data);
        } else if (addr == STEREO_VOLUME && data != old_reg) {  // global volume
            // return all oscs to 0
            for (int i = 0; i < OSC_COUNT; i++) {
                NintendoGBS_Oscillator& osc = *oscs[i];
                int amp = osc.last_amp;
                osc.last_amp = 0;
                if (amp && osc.enabled && osc.output)
                    other_synth.offset(time, -amp, osc.output);
            }

            if (wave.outputs[3])
                other_synth.offset(time, 30, wave.outputs[3]);

            update_volume();

            if (wave.outputs[3])
                other_synth.offset(time, -30, wave.outputs[3]);

            // oscs will update with new amplitude when next run
        } else if (addr == 0xFF25 || addr == POWER_CONTROL_STATUS) {
            int mask = (regs[POWER_CONTROL_STATUS - ADDR_START] & 0x80) ? ~0 : 0;
            int flags = regs[0xFF25 - ADDR_START] & mask;

            // left/right assignments
            for (int i = 0; i < OSC_COUNT; i++) {
                NintendoGBS_Oscillator& osc = *oscs[i];
                osc.enabled &= mask;
                int bits = flags >> i;
                BLIPBuffer* old_output = osc.output;
                osc.output_select = (bits >> 3 & 2) | (bits & 1);
                osc.output = osc.outputs[osc.output_select];
                if (osc.output != old_output) {
                    int amp = osc.last_amp;
                    osc.last_amp = 0;
                    if (amp && old_output)
                        other_synth.offset(time, -amp, old_output);
                }
            }

            if (addr == POWER_CONTROL_STATUS && data != old_reg) {
                if (!(data & 0x80)) {
                    for (unsigned i = 0; i < sizeof powerup_regs; i++) {
                        if (i != POWER_CONTROL_STATUS - ADDR_START)
                            write(i + ADDR_START, powerup_regs[i]);
                    }
                }
                // else {
                //     std::cout << "APU powered on\n" << std::endl;
                // }
            }
        } else if (addr >= 0xFF30) {
            int index = (addr & 0x0F) * 2;
            wave.wave[index] = data >> 4;
            wave.wave[index + 1] = data & 0x0F;
        }
    }

    // Read from address at specified time
    int read_register(blip_time_t time, unsigned addr) {
        run_until(time);

        int index = addr - ADDR_START;
        assert((unsigned) index < REGISTER_COUNT);
        int data = regs[index];

        if (addr == POWER_CONTROL_STATUS) {
            data = (data & 0x80) | 0x70;
            for (int i = 0; i < OSC_COUNT; i++) {
                const NintendoGBS_Oscillator& osc = *oscs[i];
                if (osc.enabled && (osc.length || !(osc.regs[4] & osc.len_enabled_mask)))
                    data |= 1 << i;
            }
        }

        return data;
    }

    // Run all oscillators up to specified time, end current time frame, then
    // start a new frame at time 0.
    void end_frame(blip_time_t end_time) {
        if (end_time > last_time)
            run_until(end_time);

        assert(next_frame_time >= end_time);
        next_frame_time -= end_time;

        assert(last_time >= end_time);
        last_time -= end_time;
    }

    void set_tempo(double tempo_division) {
        frame_period = 4194304 / 256; // 256 Hz
        if (tempo_division != 1.0)
            frame_period = blip_time_t (frame_period / tempo_division);
    }

 private:
    // noncopyable
    NintendoGBS(const NintendoGBS&);
    NintendoGBS& operator = (const NintendoGBS&);

    NintendoGBS_Oscillator* oscs[OSC_COUNT];
    blip_time_t next_frame_time;
    blip_time_t last_time;
    blip_time_t frame_period;
    double volume_unit;
    int frame_count;

    NintendoGBS_Pulse square1;
    NintendoGBS_Pulse square2;
    NintendoGBS_Wave wave;
    NintendoGBS_Noise noise;
    uint8_t regs[REGISTER_COUNT];
    // used by squares
    NintendoGBS_Pulse::Synth square_synth;
    // used by wave and noise
    NintendoGBS_Wave::Synth other_synth;

    void update_volume() {
        // TODO: doesn't handle differing left/right global volume (support would
        // require modification to all oscillator code)
        int data = regs[STEREO_VOLUME - ADDR_START];
        double vol = (std::max(data & 7, data >> 4 & 7) + 1) * volume_unit;
        square_synth.volume(vol);
        other_synth.volume(vol);
    }

    void run_until(blip_time_t end_time) {
        assert(end_time >= last_time); // end_time must not be before previous time
        if (end_time == last_time)
            return;

        while (true) {
            blip_time_t time = next_frame_time;
            if (time > end_time)
                time = end_time;

            // run oscillators
            for (int i = 0; i < OSC_COUNT; ++i) {
                NintendoGBS_Oscillator& osc = *oscs[i];
                if (osc.output) {
                    int playing = false;
                    if (osc.enabled && osc.volume &&
                            (!(osc.regs[4] & osc.len_enabled_mask) || osc.length))
                        playing = -1;
                    switch (i) {
                    case 0: square1.run(last_time, time, playing); break;
                    case 1: square2.run(last_time, time, playing); break;
                    case 2: wave   .run(last_time, time, playing); break;
                    case 3: noise  .run(last_time, time, playing); break;
                    }
                }
            }
            last_time = time;

            if (time == end_time)
                break;

            next_frame_time += frame_period;

            // 256 Hz actions
            square1.clock_length();
            square2.clock_length();
            wave.clock_length();
            noise.clock_length();

            frame_count = (frame_count + 1) & 3;
            if (frame_count == 0) {
                // 64 Hz actions
                square1.clock_envelope();
                square2.clock_envelope();
                noise.clock_envelope();
            }

            if (frame_count & 1)
                square1.clock_sweep(); // 128 Hz action
        }
    }

    void write_osc(int index, int reg, int data);
};

#include "nintendo_gameboy_oscillators.hpp"

void NintendoGBS::write_osc(int index, int reg, int data) {
    reg -= index * 5;
    NintendoGBS_Pulse* sq = &square2;
    switch (index) {
    case 0:
        sq = &square1;
    case 1:
        if (sq->write_register(reg, data) && index == 0) {
            square1.sweep_freq = square1.frequency();
            if ((regs[0] & sq->period_mask) && (regs[0] & sq->shift_mask)) {
                square1.sweep_delay = 1;  // cause sweep to recalculate now
                square1.clock_sweep();
            }
        }
        break;

    case 2:
        wave.write_register(reg, data);
        break;

    case 3:
        if (noise.write_register(reg, data))
            noise.bits = 0x7FFF;
    }
}

#endif  // DSP_NINTENDO_GAMEBOY_HPP_
