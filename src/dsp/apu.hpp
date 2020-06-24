// A macro oscillator based on the NES 2A03 synthesis chip.
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

#ifndef NES_APU_HPP
#define NES_APU_HPP

#include "oscillators.hpp"
#include <cassert>

/// A macro oscillator based on the NES 2A03 synthesis chip.
class APU {
 public:
    /// the number of oscillators on the VRC6 chip
    static constexpr int OSC_COUNT = 4;
    /// the first address of the APU RAM addresses
    static constexpr int ADDR_START = 0x4000;
    /// the last address of the APU RAM addresses
    static constexpr int ADDR_END   = 0x4017;

    /// Initialize a new APU.
    APU() {
        pulse1.synth = pulse2.synth = &square_synth;
        output(NULL);
        volume(1.0);
        reset(false);
    }

    /// Reset internal frame counter, registers, and all oscillators.
    ///
    /// @param pal_timing Use PAL timing if pal_timing is true, otherwise NTSC
    ///
    inline void reset(bool pal_timing = false) {
        // TODO: time pal frame periods exactly
        frame_period = pal_timing ? 8314 : 7458;

        pulse1.reset();
        pulse2.reset();
        triangle.reset();
        noise.reset();

        last_time = 0;
        osc_enables = 0;
        frame_delay = 1;
        write_register(0, 0x4017, 0x00);
        write_register(0, 0x4015, 0x00);
        // initialize sq1, sq2, tri, and noise, not DMC
        for (cpu_addr_t addr = ADDR_START; addr <= 0x4009; addr++)
            write_register(0, addr, (addr & 3) ? 0x00 : 0x10);
    }

    /// Set the volume.
    ///
    /// @param value the global volume level of the chip
    ///
    inline void volume(double v = 1.f) {
        square_synth.volume(0.1128 * v);
        triangle.synth.volume(0.12765 * v);
        noise.synth.volume(0.0741 * v);
    }

    /// Set treble equalization.
    ///
    /// @param eq the equalizer settings to use
    ///
    inline void treble_eq(const blip_eq_t& eq) {
        square_synth.treble_eq(eq);
        triangle.synth.treble_eq(eq);
        noise.synth.treble_eq(eq);
    }

    /// Set buffer to generate all sound into, or disable sound if NULL.
    ///
    /// @param buf the buffer to write samples from the synthesizer to
    ///
    inline void output(BLIPBuffer* buf) {
        for (int i = 0; i < OSC_COUNT; i++) osc_output(i, buf);
    }

    /// Set the output buffer for an individual synthesizer voice.
    ///
    /// @param i the index of the oscillator to set the output buffer for
    /// @param buf the buffer to write samples from the synthesizer to
    /// @note If buffer is NULL, the specified oscillator is muted and
    ///       emulation accuracy is reduced.
    /// @note The oscillators are indexed as follows:
    ///       0) Pulse 1,
    ///       1) Pulse 2,
    ///       2) Triangle,
    ///       3) Noise.
    ///
    inline void osc_output(int osc, BLIPBuffer* buf) {
        assert(("APU::osc_output(): Index out of range", 0 <= osc && osc < OSC_COUNT));
        oscs[osc]->output = buf;
    }

    /// Run all oscillators up to specified time, end current time frame, then
    /// start a new time frame at time 0. Time frames have no effect on
    /// emulation and each can be whatever length is convenient.
    ///
    /// @param time the number of elapsed cycles
    ///
    inline void end_frame(cpu_time_t end_time) {
        if (end_time > last_time) run_until(end_time);
        // make times relative to new frame
        last_time -= end_time;
        assert(last_time >= 0);
    }

    /// Write to register (0x4000-0x4017, except 0x4014 and 0x4016).
    ///
    /// @param time the number of elapsed cycles
    /// @param address the address of the register to write
    /// @param data the data to write to the register
    ///
    void write_register(cpu_time_t time, cpu_addr_t addr, int data) {
        assert(addr > 0x20);  // addr must be actual address (i.e. 0x40xx)
        assert((unsigned) data <= 0xff);

        // Ignore addresses outside range
        if (addr < ADDR_START || ADDR_END < addr) return;

        run_until(time);

        if (addr < 0x4010) {  // synthesize registers
            // Write to channel
            int osc_index = (addr - ADDR_START) >> 2;
            Oscillator* osc = oscs[osc_index];

            int reg = addr & 3;
            osc->regs[reg] = data;
            osc->reg_written[reg] = true;

            if (osc_index == 4) {
                // handle DMC specially
            } else if (reg == 3) {
                // load length counter
                if ((osc_enables >> osc_index) & 1)
                    osc->length_counter = length_table[(data >> 3) & 0x1f];
                // reset square phase
                // DISABLED TO HACK SQUARE OSCILLATOR for VCV Rack
                // if (osc_index < 2)
                //  ((Nes_Square*) osc)->phase = Nes_Square::phase_range - 1;
            }
        } else if (addr == 0x4015) {
            // Channel enables
            for (int i = OSC_COUNT; i--;)
                if (!((data >> i) & 1))
                    oscs[i]->length_counter = 0;
            int old_enables = osc_enables;
            osc_enables = data;
        } else if (addr == 0x4017) {
            // Frame mode
            frame_mode = data;

            // mode 1
            frame_delay = (frame_delay & 1);
            frame = 0;

            if (!(data & 0x80)) {
                // mode 0
                frame = 1;
                frame_delay += frame_period;
            }
        }
    }

 private:
    /// The length table to lookup length values from registers
    static constexpr unsigned char length_table[0x20] = {
        0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06,
        0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
        0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
        0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E
    };

    /// Disable the public copy constructor.
    APU(const APU&);

    /// Disable the public assignment operator.
    APU& operator=(const APU&);

    /// the channel 0 pulse wave generator
    Pulse pulse1;
    /// the channel 1 pulse wave generator
    Pulse pulse2;
    /// the channel 2 triangle wave generator
    Noise noise;
    /// the channel 3 noise generator
    Triangle triangle;
    /// pointers to the oscillators
    Oscillator* oscs[OSC_COUNT] = {
        &pulse1, &pulse2, &triangle, &noise
    };

    /// has been run until this time in current frame
    cpu_time_t last_time;
    /// TODO: document
    int frame_period;
    /// cycles until frame counter runs next
    int frame_delay;
    /// current frame (0-3)
    int frame;
    /// the channel enabled register
    int osc_enables;
    /// TODO: document
    int frame_mode;
    /// a synthesizer shared by squares
    Pulse::Synth square_synth;

    // TODO: remove these two nonlinear functions?
    // /// Enable nonlinear volume.
    // ///
    // /// @param v the volume level
    // ///
    // inline void enable_nonlinear(double v) {
    //     square_synth.volume(1.3 * 0.25751258 / 0.742467605 * 0.25 * v);
    //     const double tnd = 0.75 / 202 * 0.48;
    //     triangle.synth.volume_unit(3 * tnd);
    //     noise.synth.volume_unit(2 * tnd);
    //     buffer_cleared();
    // }
    // /// Reset oscillator amplitudes. Must be called when clearing buffer while
    // /// using non-linear sound.
    // inline void buffer_cleared() {
    //     pulse1.last_amp = 0;
    //     pulse2.last_amp = 0;
    //     triangle.last_amp = 0;
    //     noise.last_amp = 0;
    // }

    /// Run APU until specified time, so that any DMC memory reads can be
    /// accounted for (i.e. inserting CPU wait states).
    ///
    /// @param time the number of elapsed cycles
    ///
    void run_until(cpu_time_t end_time) {
        assert(end_time >= last_time);
        if (end_time == last_time) return;

        while (true) {
            // earlier of next frame time or end time
            cpu_time_t time = last_time + frame_delay;
            if (time > end_time) time = end_time;
            frame_delay -= time - last_time;

            // run oscs to present
            pulse1.run(last_time, time);
            pulse2.run(last_time, time);
            triangle.run(last_time, time);
            noise.run(last_time, time);
            last_time = time;

            // no more frames to run
            if (time == end_time) break;

            // take frame-specific actions
            frame_delay = frame_period;
            switch (frame++) {
                case 0:
                    // fall through
                case 2:
                    // clock length and sweep on frames 0 and 2
                    pulse1.clock_length(0x20);
                    pulse2.clock_length(0x20);
                    noise.clock_length(0x20);
                    // different bit for halt flag on triangle
                    triangle.clock_length(0x80);

                    pulse1.clock_sweep(-1);
                    pulse2.clock_sweep(0);
                    break;
                case 1:
                    // frame 1 is slightly shorter
                    frame_delay -= 2;
                    break;
                case 3:
                    frame = 0;
                    // frame 3 is almost twice as long in mode 1
                    if (frame_mode & 0x80)
                        frame_delay += frame_period - 6;
                    break;
            }
            // clock envelopes and linear counter every frame
            triangle.clock_linear_counter();
            pulse1.clock_envelope();
            pulse2.clock_envelope();
            noise.clock_envelope();
        }
    }
};

#endif  // NES_APU_HPP
