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
    APU();

    /// Reset internal frame counter, registers, and all oscillators.
    ///
    /// @param pal_timing Use PAL timing if pal_timing is true, otherwise NTSC
    ///
    void reset(bool pal_timing = false);

    /// Set the volume.
    ///
    /// @param value the global volume level of the chip
    ///
    void volume(double = 1.f);

    // Set treble equalization (see notes.txt).
    void treble_eq(const blip_eq_t&);

    /// Set buffer to generate all sound into, or disable sound if NULL.
    ///
    /// @param buf the buffer to write samples from the synthesizer to
    ///
    void output(BLIPBuffer*);

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
    void osc_output(int osc, BLIPBuffer* buf) {
        assert(("APU::osc_output(): Index out of range", 0 <= osc && osc < OSC_COUNT));
        oscs[osc]->output = buf;
    }

    /// Run all oscillators up to specified time, end current time frame, then
    /// start a new time frame at time 0. Time frames have no effect on
    /// emulation and each can be whatever length is convenient.
    ///
    /// @param time the number of elapsed cycles
    ///
    void end_frame(cpu_time_t);

    /// Write to register (0x4000-0x4017, except 0x4014 and 0x4016)
    void write_register(cpu_time_t, cpu_addr_t, int data);

 private:
    friend class Nes_Nonlinearizer;
    void enable_nonlinear(double volume);

    /// Reset oscillator amplitudes. Must be called when clearing buffer while
    /// using non-linear sound.
    void buffer_cleared();

    /// Run APU until specified time, so that any DMC memory reads can be
    /// accounted for (i.e. inserting CPU wait states).
    ///
    /// @param time the number of elapsed cycles
    ///
    void run_until(cpu_time_t);

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
    Oscillator* oscs[OSC_COUNT];

    /// has been run until this time in current frame
    cpu_time_t last_time;
    /// TODO:
    int frame_period;
    /// cycles until frame counter runs next
    int frame_delay;
    /// current frame (0-3)
    int frame;
    /// the channel enabled register
    int osc_enables;
    /// TODO:
    int frame_mode;
    /// a synthesizer shared by squares
    Pulse::Synth square_synth;
};

#endif  // NES_APU_HPP
