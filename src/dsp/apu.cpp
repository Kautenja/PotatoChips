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
