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

#include "apu.h"
#include "blip_buffer/blargg_source.h"

APU::APU() {
    square1.synth = &square_synth;
    square2.synth = &square_synth;

    oscs[0] = &square1;
    oscs[1] = &square2;
    oscs[2] = &triangle;
    oscs[3] = &noise;

    output(NULL);
    volume(1.0);
    reset(false);
}

void APU::treble_eq(const blip_eq_t& eq) {
    square_synth.treble_eq(eq);
    triangle.synth.treble_eq(eq);
    noise.synth.treble_eq(eq);
}

void APU::buffer_cleared() {
    square1.last_amp = 0;
    square2.last_amp = 0;
    triangle.last_amp = 0;
    noise.last_amp = 0;
}

void APU::enable_nonlinear(double v) {
    square_synth.volume(1.3 * 0.25751258 / 0.742467605 * 0.25 * v);
    const double tnd = 0.75 / 202 * 0.48;
    triangle.synth.volume_unit(3 * tnd);
    noise.synth.volume_unit(2 * tnd);
    buffer_cleared();
}

void APU::volume(double v) {
    square_synth.volume(0.1128 * v);
    triangle.synth.volume(0.12765 * v);
    noise.synth.volume(0.0741 * v);
}

void APU::output(Blip_Buffer* buffer) {
    for (int i = 0; i < OSC_COUNT; i++)
        osc_output(i, buffer);
}

void APU::reset(bool pal_mode) {
    // to do: time pal frame periods exactly
    frame_period = pal_mode ? 8314 : 7458;

    square1.reset();
    square2.reset();
    triangle.reset();
    noise.reset();

    last_time = 0;
    osc_enables = 0;
    frame_delay = 1;
    write_register(0, 0x4017, 0x00);
    write_register(0, 0x4015, 0x00);
    // initialize sq1, sq2, tri, and noise, not DMC
    for (cpu_addr_t addr = start_addr; addr <= 0x4009; addr++)
        write_register(0, addr, (addr & 3) ? 0x00 : 0x10);
}

// frames

void APU::run_until(cpu_time_t end_time) {
    require(end_time >= last_time);

    if (end_time == last_time)
        return;

    while (true) {
        // earlier of next frame time or end time
        cpu_time_t time = last_time + frame_delay;
        if (time > end_time)
            time = end_time;
        frame_delay -= time - last_time;

        // run oscs to present
        square1.run(last_time, time);
        square2.run(last_time, time);
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
                square1.clock_length(0x20);
                square2.clock_length(0x20);
                noise.clock_length(0x20);
                // different bit for halt flag on triangle
                triangle.clock_length(0x80);

                square1.clock_sweep(-1);
                square2.clock_sweep(0);
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
        square1.clock_envelope();
        square2.clock_envelope();
        noise.clock_envelope();
    }
}

void APU::end_frame(cpu_time_t end_time) {
    if (end_time > last_time)
        run_until(end_time);

    // make times relative to new frame
    last_time -= end_time;
    require(last_time >= 0);
}

// registers

static const unsigned char length_table[0x20] = {
    0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06,
    0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
    0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
    0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E
};

void APU::write_register(cpu_time_t time, cpu_addr_t addr, int data) {
    require(addr > 0x20);  // addr must be actual address (i.e. 0x40xx)
    require((unsigned) data <= 0xff);

    // Ignore addresses outside range
    if (addr < start_addr || end_addr < addr) return;

    run_until(time);

    if (addr < 0x4010) {  // synthesize registers
        // Write to channel
        int osc_index = (addr - start_addr) >> 2;
        Nes_Osc* osc = oscs[osc_index];

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
