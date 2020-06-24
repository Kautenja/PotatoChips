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

#include "apu.hpp"
#include <cassert>

APU::APU() {
    pulse1.synth = &square_synth;
    pulse2.synth = &square_synth;

    oscs[0] = &pulse1;
    oscs[1] = &pulse2;
    oscs[2] = &triangle;
    oscs[3] = &noise;

    output(NULL);
    volume(1.0);
    reset(false);
}

// frames

void APU::run_until(cpu_time_t end_time) {
    assert(end_time >= last_time);

    if (end_time == last_time)
        return;

    while (true) {
        // earlier of next frame time or end time
        cpu_time_t time = last_time + frame_delay;
        if (time > end_time)
            time = end_time;
        frame_delay -= time - last_time;

        // run oscs to present
        pulse1.run(last_time, time);
        pulse2.run(last_time, time);
        triangle.run(last_time, time);
        noise.run(last_time, time);
        last_time = time;

        if (time == end_time)
            break;  // no more frames to run

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


// registers

static const unsigned char length_table[0x20] = {
    0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06,
    0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
    0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
    0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E
};

void APU::write_register(cpu_time_t time, cpu_addr_t addr, int data) {
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

            // DISABLED TO HACK SQUARE OSCILLATOR
            // // reset square phase
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
