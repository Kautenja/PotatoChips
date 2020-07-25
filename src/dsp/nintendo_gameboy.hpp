// The Nintendo GameBoy Sound System (GBS) chip emulator.
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

#include "blip_buffer.hpp"
#include "exceptions.hpp"
#include <algorithm>
#include <cstring>

// TODO: remove (use global wavetable header)

/// the default values for the wave-table
const uint8_t sine_wave[32] = {
    0xA,0x8,0xD,0xC,0xE,0xE,0xF,0xF,0xF,0xF,0xE,0xF,0xD,0xE,0xA,0xC,
    0x5,0x8,0x2,0x3,0x1,0x1,0x0,0x0,0x0,0x0,0x1,0x0,0x2,0x1,0x5,0x3
};

/// @brief The Nintendo GameBoy Sound System (GBS) chip emulator.
class NintendoGBS {
 public:
    /// the number of oscillators on the chip
    enum { OSC_COUNT = 4 };
    /// the first address of the APU in memory space
    enum { ADDR_START = 0xFF10 };
    /// the last address of the APU in memory space
    enum { ADDR_END   = 0xFF3F };
    /// the total number of registers available on the chip
    enum { REGISTER_COUNT = ADDR_END - ADDR_START + 1 };

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

 private:
    struct Oscillator {
        /// TODO:
        enum { trigger = 0x80 };
        /// TODO:
        enum { len_enabled_mask = 0x40 };
        /// TODO:
        BLIPBuffer* outputs[4];  // NULL, right, left, center
        /// TODO:
        BLIPBuffer* output;
        /// TODO:
        int output_select;
        /// the 5 registers for the oscillator
        uint8_t* regs;  // osc's 5 registers

        /// TODO:
        int delay;
        /// TODO:
        int last_amp;
        /// TODO:
        int volume;
        /// TODO:
        int length;
        /// TODO:
        int enabled;

        /// Reset the oscillator to its default state.
        inline void reset() {
            delay = 0;
            last_amp = 0;
            length = 0;
            output_select = 3;
            output = outputs[output_select];
        }

        /// TODO:
        inline void clock_length() {
            if ((regs[4] & len_enabled_mask) && length)
                length--;
        }

        /// Return the 11-bit frequency of the oscillator.
        inline int frequency() const {
            return (regs[4] & 7) * 0x100 + regs[3];
        }
    };

    struct Envelope : Oscillator {
        int env_delay;

        inline void reset() {
            env_delay = 0;
            Oscillator::reset();
        }

        void clock_envelope() {
            if (env_delay && !--env_delay) {
                env_delay = regs[2] & 7;
                int v = volume - 1 + (regs[2] >> 2 & 2);
                if ((unsigned) v < 15)
                    volume = v;
            }
        }

        bool write_register(int reg, int data) {
            switch (reg) {
            case 1:
                length = 64 - (regs[1] & 0x3F);
                break;
            case 2:
                if (!(data >> 4))
                    enabled = false;
                break;
            case 4:
                if (data & trigger) {
                    env_delay = regs[2] & 7;
                    volume = regs[2] >> 4;
                    enabled = true;
                    if (length == 0)
                        length = 64;
                    return true;
                }
            }
            return false;
        }
    };

    struct Pulse : Envelope {
        enum { period_mask = 0x70 };
        enum { shift_mask  = 0x07 };

        typedef BLIPSynth<blip_good_quality, 1> Synth;
        Synth const* synth;
        int sweep_delay;
        int sweep_freq;
        int phase;

        inline void reset() {
            phase = 0;
            sweep_freq = 0;
            sweep_delay = 0;
            Envelope::reset();
        }

        void clock_sweep() {
            int sweep_period = (regs[0] & period_mask) >> 4;
            if (sweep_period && sweep_delay && !--sweep_delay) {
                sweep_delay = sweep_period;
                regs[3] = sweep_freq & 0xFF;
                regs[4] = (regs[4] & ~0x07) | (sweep_freq >> 8 & 0x07);

                int offset = sweep_freq >> (regs[0] & shift_mask);
                if (regs[0] & 0x08)
                    offset = -offset;
                sweep_freq += offset;

                if (sweep_freq < 0) {
                    sweep_freq = 0;
                } else if (sweep_freq >= 2048) {
                    sweep_delay = 0;  // don't modify channel frequency any further
                    sweep_freq = 2048;  // silence sound immediately
                }
            }
        }

        void run(blip_time_t time, blip_time_t end_time, int playing) {
            if (sweep_freq == 2048)
                playing = false;

            static unsigned char const table[4] = { 1, 2, 4, 6 };
            int const duty = table[regs[1] >> 6];
            int amp = volume & playing;
            if (phase >= duty)
                amp = -amp;

            int frequency = this->frequency();
            if (unsigned (frequency - 1) > 2040) {  // frequency < 1 || frequency > 2041
                // really high frequency results in DC at half volume
                amp = volume >> 1;
                playing = false;
            }

            {
                int delta = amp - last_amp;
                if (delta) {
                    last_amp = amp;
                    synth->offset(time, delta, output);
                }
            }

            time += delay;
            if (!playing)
                time = end_time;

            if (time < end_time) {
                int const period = (2048 - frequency) * 4;
                BLIPBuffer* const output = this->output;
                int phase = this->phase;
                int delta = amp * 2;
                do {
                    phase = (phase + 1) & 7;
                    if (phase == 0 || phase == duty) {
                        delta = -delta;
                        synth->offset(time, delta, output);
                    }
                    time += period;
                } while (time < end_time);
                this->phase = phase;
                last_amp = delta >> 1;
            }
            delay = time - end_time;
        }
    };

    struct Noise : Envelope {
        typedef BLIPSynth<blip_med_quality, 1> Synth;
        Synth const* synth;
        unsigned bits;

        void run(blip_time_t time, blip_time_t end_time, int playing) {
            int amp = volume & playing;
            int tap = 13 - (regs[3] & 8);
            if (bits >> tap & 2)
                amp = -amp;

            {
                int delta = amp - last_amp;
                if (delta) {
                    last_amp = amp;
                    synth->offset(time, delta, output);
                }
            }

            time += delay;
            if (!playing)
                time = end_time;

            if (time < end_time) {
                static unsigned char const table[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };
                int period = table[regs[3] & 7] << (regs[3] >> 4);

                // keep parallel resampled time to eliminate time conversion in the loop
                BLIPBuffer* const output = this->output;
                const auto resampled_period = output->resampled_duration(period);
                auto resampled_time = output->resampled_time(time);
                unsigned bits = this->bits;
                int delta = amp * 2;

                do {
                    unsigned changed = (bits >> tap) + 1;
                    time += period;
                    bits <<= 1;
                    if (changed & 2) {
                        delta = -delta;
                        bits |= 1;
                        synth->offset_resampled(resampled_time, delta, output);
                    }
                    resampled_time += resampled_period;
                } while (time < end_time);

                this->bits = bits;
                last_amp = delta >> 1;
            }
            delay = time - end_time;
        }
    };

    struct Wave : Oscillator {
        typedef BLIPSynth<blip_med_quality, 1> Synth;
        Synth const* synth;
        int wave_pos;
        enum { wave_size = 32 };
        uint8_t wave[wave_size];

        inline void write_register(int reg, int data) {
            switch (reg) {
            case 0:
                if (!(data & 0x80))
                    enabled = false;
                break;
            case 1:
                length = 256 - regs[1];
                break;
            case 2:
                volume = data >> 5 & 3;
                break;
            case 4:
                if (data & trigger & regs[0]) {
                    wave_pos = 0;
                    enabled = true;
                    if (length == 0)
                        length = 256;
                }
            }
        }

        void run(blip_time_t time, blip_time_t end_time, int playing) {
            int volume_shift = (volume - 1) & 7;  // volume = 0 causes shift = 7
            int frequency;
            {
                int amp = (wave[wave_pos] >> volume_shift & playing) * 2;
                frequency = this->frequency();
                if (unsigned (frequency - 1) > 2044) {  // frequency < 1 || frequency > 2045
                    amp = 30 >> volume_shift & playing;
                    playing = false;
                }

                int delta = amp - last_amp;
                if (delta) {
                    last_amp = amp;
                    synth->offset(time, delta, output);
                }
            }

            time += delay;
            if (!playing)
                time = end_time;

            if (time < end_time) {
                BLIPBuffer* const output = this->output;
                int const period = (2048 - frequency) * 2;
                int wave_pos = (this->wave_pos + 1) & (wave_size - 1);

                do {
                    int amp = (wave[wave_pos] >> volume_shift) * 2;
                    wave_pos = (wave_pos + 1) & (wave_size - 1);
                    int delta = amp - last_amp;
                    if (delta) {
                        last_amp = amp;
                        synth->offset(time, delta, output);
                    }
                    time += period;
                } while (time < end_time);

                this->wave_pos = (wave_pos - 1) & (wave_size - 1);
            }
            delay = time - end_time;
        }
    };

    /// TODO:
    blip_time_t next_frame_time;
    /// TODO:
    blip_time_t last_time;
    /// TODO:
    blip_time_t frame_period;
    /// TODO:
    double volume_unit;
    /// TODO:
    int frame_count;

    /// the pulse waveform generator for channel 1
    Pulse pulse1;
    /// the pulse waveform generator for channel 2
    Pulse pulse2;
    /// the DPCM waveform generator for channel 3
    Wave wave;
    /// the noise generator for channel 4
    Noise noise;
    /// the general set of oscillators on the chip
    Oscillator* const oscs[OSC_COUNT] = {
        &pulse1,
        &pulse2,
        &wave,
        &noise
    };
    /// the registers on the chip
    uint8_t regs[REGISTER_COUNT];

    /// the synthesizer used by pulse waves
    Pulse::Synth pulse_synth;
    /// the synthesizer used by DPCM wave and noise
    Wave::Synth other_synth;

    void update_volume() {
        // TODO: doesn't handle differing left/right global volume (support would
        // require modification to all oscillator code)
        int data = regs[STEREO_VOLUME - ADDR_START];
        double vol = (std::max(data & 7, data >> 4 & 7) + 1) * volume_unit;
        pulse_synth.volume(vol);
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
                Oscillator& osc = *oscs[i];
                if (osc.output) {
                    int playing = false;
                    if (osc.enabled && osc.volume &&
                            (!(osc.regs[4] & osc.len_enabled_mask) || osc.length))
                        playing = -1;
                    switch (i) {
                    case 0: pulse1.run(last_time, time, playing); break;
                    case 1: pulse2.run(last_time, time, playing); break;
                    case 2: wave.run(  last_time, time, playing); break;
                    case 3: noise.run( last_time, time, playing); break;
                    }
                }
            }
            last_time = time;

            if (time == end_time)
                break;

            next_frame_time += frame_period;

            // 256 Hz actions
            pulse1.clock_length();
            pulse2.clock_length();
            wave.clock_length();
            noise.clock_length();

            frame_count = (frame_count + 1) & 3;
            if (frame_count == 0) {
                // 64 Hz actions
                pulse1.clock_envelope();
                pulse2.clock_envelope();
                noise.clock_envelope();
            }

            if (frame_count & 1)
                pulse1.clock_sweep(); // 128 Hz action
        }
    }

    void write_osc(int index, int reg, int data) {
        reg -= index * 5;
        Pulse* sq = &pulse2;
        switch (index) {
        case 0:
            sq = &pulse1;
        case 1:
            if (sq->write_register(reg, data) && index == 0) {
                pulse1.sweep_freq = pulse1.frequency();
                if ((regs[0] & sq->period_mask) && (regs[0] & sq->shift_mask)) {
                    pulse1.sweep_delay = 1;  // cause sweep to recalculate now
                    pulse1.clock_sweep();
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

    /// Disable the copy constructor
    NintendoGBS(const NintendoGBS&);

    /// Disable the assignment operator
    NintendoGBS& operator=(const NintendoGBS&);

 public:
    /// Initialize a new Nintendo GBS chip emulator.
    NintendoGBS() {
        pulse1.synth = &pulse_synth;
        pulse2.synth = &pulse_synth;
        wave.synth  = &other_synth;
        noise.synth = &other_synth;

        for (int i = 0; i < OSC_COUNT; i++) {
            Oscillator& osc = *oscs[i];
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
    void treble_eq(const BLIPEqualizer& equalizer) {
        pulse_synth.treble_eq(equalizer);
        other_synth.treble_eq(equalizer);
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
    inline void osc_output(int index, BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right) {
        assert((unsigned) index < OSC_COUNT);
        assert((center && left && right) || (!center && !left && !right));
        Oscillator& osc = *oscs[index];
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

        pulse1.reset();
        pulse2.reset();
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
                Oscillator& osc = *oscs[i];
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
                Oscillator& osc = *oscs[i];
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
                const Oscillator& osc = *oscs[i];
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
};

#endif  // DSP_NINTENDO_GAMEBOY_HPP_
