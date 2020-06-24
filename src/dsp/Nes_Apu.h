// A macro oscillator based on the NES 2A03 synthesis chip.
// Copyright 2020 Christian Kauten
//
// Author: Christian Kauten (kautenja@auburn.edu)
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

#ifndef NES_APU_H
#define NES_APU_H

#include "Nes_Oscs.h"

/// A macro oscillator based on the NES 2A03 synthesis chip.
class Nes_Apu {
 public:
    Nes_Apu();

    // Set buffer to generate all sound into, or disable sound if NULL
    void output(Blip_Buffer*);

    // All time values are the number of CPU clock cycles relative to the
    // beginning of the current time frame. Before resetting the CPU clock
    // count, call end_frame(last_cpu_time).

    // Write to register (0x4000-0x4017, except 0x4014 and 0x4016)
    enum { start_addr = 0x4000 };
    enum { end_addr   = 0x4017 };
    void write_register(cpu_time_t, cpu_addr_t, int data);

    // Run all oscillators up to specified time, end current time frame, then
    // start a new time frame at time 0. Time frames have no effect on emulation
    // and each can be whatever length is convenient.
    void end_frame(cpu_time_t);

// Additional optional features (can be ignored without any problem)

    // Reset internal frame counter, registers, and all oscillators.
    // Use PAL timing if pal_timing is true, otherwise use NTSC timing.
    void reset(bool pal_timing = false);

    // Set overall volume (default is 1.0)
    void volume(double);

    // Reset oscillator amplitudes. Must be called when clearing buffer while
    // using non-linear sound.
    void buffer_cleared();

    // Set treble equalization (see notes.txt).
    void treble_eq(const blip_eq_t&);

    // Set sound output of specific oscillator to buffer. If buffer is NULL,
    // the specified oscillator is muted and emulation accuracy is reduced.
    // The oscillators are indexed as follows: 0) Square 1, 1) Square 2,
    // 2) Triangle, 3) Noise.
    enum { osc_count = 4 };
    void osc_output(int osc, Blip_Buffer* buf) {
        assert(("Nes_Apu::osc_output(): Index out of range", 0 <= osc && osc < osc_count));
        oscs[osc]->output = buf;
    }

    // Run APU until specified time, so that any DMC memory reads can be
    // accounted for (i.e. inserting CPU wait states).
    void run_until(cpu_time_t);

// End of public interface.
 private:
    friend class Nes_Nonlinearizer;
    void enable_nonlinear(double volume);

 private:
    /// Disable the copy constructor.
    Nes_Apu(const Nes_Apu&);
    /// Disable the assignment operator.
    Nes_Apu& operator = (const Nes_Apu&);

    /// the channel 1 square wave generator
    Nes_Square          square1;
    /// the channel 2 square wave generator
    Nes_Square          square2;
    /// the channel 2 triangle wave generator
    Nes_Noise           noise;
    /// the channel 2 noise generator
    Nes_Triangle        triangle;
    /// pointers to the oscillators
    Nes_Osc*            oscs[osc_count];

    /// has been run until this time in current frame
    cpu_time_t last_time;
    /// TODO:
    int frame_period;
    /// cycles until frame counter runs next
    int frame_delay;
    /// current frame (0-3)
    int frame;
    /// TODO:
    int osc_enables;
    /// TODO:
    int frame_mode;
    /// a synthesizer shared by squares
    Nes_Square::Synth square_synth;
};

#endif  // NES_APU_H
