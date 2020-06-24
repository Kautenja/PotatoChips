// A macro oscillator based on the Namco 106 synthesis chip.
// Copyright 2020 Christian Kauten
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

#ifndef NES_NAMCO106_HPP
#define NES_NAMCO106_HPP

#include "apu.hpp"

/// A macro oscillator based on the Namco 106 synthesis chip.
class Namco106 {
 public:
    Namco106() {
        output(NULL);
        volume(1.0);
        reset();
    }

    // See APU.h for reference.
    inline void volume(double v) { synth.volume(0.10 / OSC_COUNT * v); }

    inline void treble_eq(const blip_eq_t& eq) { synth.treble_eq(eq); }

    void output(BLIPBuffer* buf) {
        for (int i = 0; i < OSC_COUNT; i++) osc_output(i, buf);
    }

    static constexpr int OSC_COUNT = 8;
    inline void osc_output(int i, BLIPBuffer* buf) {
        assert((unsigned) i < OSC_COUNT);
        oscs[i].output = buf;
    }

    void reset() {
        addr_reg = 0;

        int i;
        for (i = 0; i < REG_COUNT; i++)
            reg[i] = 0;

        for (i = 0; i < OSC_COUNT; i++) {
            Namco_Osc& osc = oscs[i];
            osc.delay = 0;
            osc.last_amp = 0;
            osc.wave_pos = 0;
        }
    }

    void end_frame(cpu_time_t time) {
        if (time > last_time) run_until(time);
        last_time -= time;
        assert(last_time >= 0);
    }

    // Read/write data register is at 0x4800
    enum { data_reg_addr = 0x4800 };
    inline void write_data(cpu_time_t time, int data) {
        run_until(time);
        access() = data;
    }

    inline int read_data() { return access(); }

    // Write-only address register is at 0xF800
    enum { addr_reg_addr = 0xF800 };
    inline void write_addr(int v) { addr_reg = v; }

 private:
    // noncopyable
    Namco106(const Namco106&);
    Namco106& operator = (const Namco106&);

    struct Namco_Osc {
        int32_t delay;
        BLIPBuffer* output;
        int16_t last_amp;
        int16_t wave_pos;
    };

    Namco_Osc oscs[OSC_COUNT];

    cpu_time_t last_time;
    int addr_reg;

    static constexpr int REG_COUNT = 0x80;
    uint8_t reg[REG_COUNT];
    BLIPSynth<BLIPQuality::Good, 15> synth;

    uint8_t& access() {
        int addr = addr_reg & 0x7f;
        if (addr_reg & 0x80) addr_reg = (addr + 1) | 0x80;
        return reg[addr];
    }

    void run_until(cpu_time_t nes_end_time) {
        int active_oscs = ((reg[0x7f] >> 4) & 7) + 1;
        for (int i = OSC_COUNT - active_oscs; i < OSC_COUNT; i++) {
            Namco_Osc& osc = oscs[i];
            BLIPBuffer* output = osc.output;
            if (!output)
                continue;

            auto time = output->resampled_time(last_time) + osc.delay;
            auto end_time = output->resampled_time(nes_end_time);
            osc.delay = 0;
            if (time < end_time) {
                const uint8_t* osc_reg = &reg[i * 8 + 0x40];
                if (!(osc_reg[4] & 0xe0))
                    continue;

                int volume = osc_reg[7] & 15;
                if (!volume)
                    continue;

                int32_t freq = (osc_reg[4] & 3) * 0x10000 + osc_reg[2] * 0x100L + osc_reg[0];
                if (!freq)
                    continue;
                BLIPBuffer::resampled_time_t period =
                        output->resampled_duration(983040) / freq * active_oscs;

                int wave_size = (8 - ((osc_reg[4] >> 2) & 7)) * 4;
                if (!wave_size)
                    continue;

                int last_amp = osc.last_amp;
                int wave_pos = osc.wave_pos;

                do {
                    // read wave sample
                    int addr = wave_pos + osc_reg[6];
                    int sample = reg[addr >> 1];
                    wave_pos++;
                    if (addr & 1)
                        sample >>= 4;
                    sample = (sample & 15) * volume;

                    // output impulse if amplitude changed
                    int delta = sample - last_amp;
                    if (delta) {
                        last_amp = sample;
                        synth.offset_resampled(time, delta, output);
                    }

                    // next sample
                    time += period;
                    if (wave_pos >= wave_size)
                        wave_pos = 0;
                } while (time < end_time);

                osc.wave_pos = wave_pos;
                osc.last_amp = last_amp;
            }
            osc.delay = time - end_time;
        }
        last_time = nes_end_time;
    }
};

#endif  // NES_NAMCO106_HPP
