// A Ricoh 2A03 sound chip emulator.
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
// derived from: Nes_Snd_Emu 0.1.7
//

#ifndef DSP_2A03_HPP_
#define DSP_2A03_HPP_

#include "ricoh_2a03_oscillators.hpp"
#include "exceptions.hpp"

/// @brief A Ricoh 2A03 sound chip emulator.
class Ricoh2A03 {
 public:
    /// the number of oscillators on the chip
    static constexpr unsigned OSC_COUNT = 4;
    /// the first address of the RAM space
    static constexpr uint16_t ADDR_START = 0x4000;
    /// the last address of the RAM space
    static constexpr uint16_t ADDR_END   = 0x4017;
    /// the number of registers on the chip
    static constexpr uint16_t NUM_REGISTERS = ADDR_END - ADDR_START;

    /// the indexes of the channels on the chip
    enum Channel {
        PULSE0,
        PULSE1,
        TRIANGLE,
        NOISE
    };

    /// the IO registers on the chip
    enum Register : uint16_t {
        /// the duty & 4-bit volume register for pulse waveform generator 0
        PULSE0_VOL =      0x4000,
        /// the sweep register for pulse waveform generator 0
        PULSE0_SWEEP =    0x4001,
        /// the frequency (low 8-bits) for pulse waveform generator 0
        PULSE0_LO =       0x4002,
        /// the frequency (high 3-bits) for pulse waveform generator 0
        PULSE0_HI =       0x4003,
        /// the duty & 4-bit volume register for pulse waveform generator 1
        PULSE1_VOL =      0x4004,
        /// the sweep register for pulse waveform generator 1
        PULSE1_SWEEP =    0x4005,
        /// the frequency (low 8-bits) for pulse waveform generator 1
        PULSE1_LO =       0x4006,
        /// the frequency (high 3-bits) for pulse waveform generator 1
        PULSE1_HI =       0x4007,
        /// the linear counter for the triangle waveform generator
        TRIANGLE_LINEAR = 0x4008,
        /// an unnecessary register that may be used for memory clearing loops
        /// by application code (NES ROMs)
        // APU_UNUSED1 =     0x4009,
        /// the frequency (low 8-bits) for triangle waveform generator
        TRIANGLE_LO =     0x400A,
        /// the frequency (high 3-bits) for triangle waveform generator
        TRIANGLE_HI =     0x400B,
        /// the volume register for the noise generator
        NOISE_VOL =       0x400C,
        /// an unnecessary register that may be used for memory clearing loops
        /// by application code (NES ROMs)
        // APU_UNUSED2 =     0x400D,
        /// period and waveform shape for the noise generator
        NOISE_LO =        0x400E,
        /// length counter value for the noise generator
        NOISE_HI =        0x400F,
        /// play mode and frequency for DMC samples
        // DMC_FREQ =        0x4010,
        /// 7-bit DAC
        // DMC_RAW =         0x4011,
        /// start of the DMC waveform
        // DMC_START =       0x4012,
        /// length of the DMC waveform
        // DMC_LEN =         0x4013,
        /// channel enables and status
        SND_CHN =         0x4015,
        // JOY1 =            0x4016,
        /// the status register
        STATUS =          0x4017,
    };

 private:
    /// the channel 0 pulse wave generator
    Pulse pulse0;
    /// the channel 1 pulse wave generator
    Pulse pulse1;
    /// the channel 2 triangle wave generator
    Noise noise;
    /// the channel 3 noise generator
    Triangle triangle;
    /// pointers to the oscillators
    Oscillator* oscs[OSC_COUNT] = { &pulse0, &pulse1, &triangle, &noise };

    /// has been run until this time in current frame
    blip_time_t last_time;
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

    /// @brief Run emulator until specified time, so that any DMC memory reads
    /// can be accounted for (i.e. inserting CPU wait states).
    ///
    /// @param time the number of elapsed cycles
    ///
    void run_until(blip_time_t end_time) {
        if (end_time < last_time)
            throw Exception("end_time must be >= last_time");
        else if (end_time == last_time)
            return;

        while (true) {
            // earlier of next frame time or end time
            blip_time_t time = last_time + frame_delay;
            if (time > end_time) time = end_time;
            frame_delay -= time - last_time;

            // run oscs to present
            pulse0.run(last_time, time);
            pulse1.run(last_time, time);
            triangle.run(last_time, time);
            noise.run(last_time, time);
            last_time = time;

            // no more frames to run
            if (time == end_time) break;

            // take frame-specific actions
            frame_delay = frame_period;
            switch (frame++) {
                case 0:  // fall through
                case 2:
                    // clock length and sweep on frames 0 and 2
                    pulse0.clock_length(0x20);
                    pulse1.clock_length(0x20);
                    noise.clock_length(0x20);
                    // different bit for halt flag on triangle
                    triangle.clock_length(0x80);

                    pulse0.clock_sweep(-1);
                    pulse1.clock_sweep(0);
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
            pulse0.clock_envelope();
            pulse1.clock_envelope();
            noise.clock_envelope();
        }
    }

    /// Disable the public copy constructor.
    Ricoh2A03(const Ricoh2A03&);

    /// Disable the public assignment operator.
    Ricoh2A03& operator=(const Ricoh2A03&);

 public:
    /// @brief Initialize a new Ricoh 2A03 emulator.
    Ricoh2A03() {
        // pulse 1 and 2 share the same synthesizer
        pulse0.synth = pulse1.synth = &square_synth;
        set_output();
        set_volume();
        reset();
    }

    /// @brief Assign single oscillator output to buffer. If buffer is NULL,
    /// silences the given oscillator.
    ///
    /// @param channel the index of the oscillator to set the output for
    /// @param buffer the BLIPBuffer to output the given voice to
    /// @returns 0 if the output was set successfully, 1 if the index is invalid
    /// @details
    /// If buffer is NULL, the specified oscillator is muted and emulation
    /// accuracy is reduced.
    ///
    inline void set_output(unsigned channel, BLIPBuffer* buffer = NULL) {
        if (channel >= OSC_COUNT)  // make sure the channel is within bounds
            throw ChannelOutOfBoundsException(channel, OSC_COUNT);
        oscs[channel]->output = buffer;
    }

    /// @brief Assign all oscillator outputs to specified buffer. If buffer
    /// is NULL, silences all oscillators.
    ///
    /// @param buffer the single buffer to output the all the voices to
    ///
    inline void set_output(BLIPBuffer* buffer = NULL) {
        for (unsigned channel = 0; channel < OSC_COUNT; channel++)
            set_output(channel, buffer);
    }

    /// @brief Set the volume level of all oscillators.
    ///
    /// @param level the value to set the volume level to, where \f$1.0\f$ is
    /// full volume. Can be overdriven past \f$1.0\f$.
    ///
    inline void set_volume(double level = 1.0) {
        square_synth.volume(0.1128 * level);
        triangle.synth.volume(0.12765 * level);
        noise.synth.volume(0.0741 * level);
    }

    /// @brief Set treble equalization for the synthesizers.
    ///
    /// @param equalizer the equalization parameter for the synthesizers
    ///
    inline void set_treble_eq(const blip_eq_t& equalizer) {
        square_synth.treble_eq(equalizer);
        triangle.synth.treble_eq(equalizer);
        noise.synth.treble_eq(equalizer);
    }

    /// @brief Reset internal frame counter, registers, and all oscillators.
    ///
    /// @param pal_timing Use PAL timing if pal_timing is true, otherwise NTSC
    ///
    inline void reset(bool pal_timing = false) {
        // TODO: time pal frame periods exactly
        frame_period = pal_timing ? 8314 : 7458;

        pulse0.reset();
        pulse1.reset();
        triangle.reset();
        noise.reset();

        last_time = 0;
        osc_enables = 0;
        frame_delay = 1;
        write(0x4017, 0x00);
        write(0x4015, 0x00);
        // initialize sq1, sq2, tri, and noise, not DMC
        for (uint16_t addr = ADDR_START; addr <= 0x4009; addr++)
            write(addr, (addr & 3) ? 0x00 : 0x10);
    }

    /// @brief Write to data to a register.
    ///
    /// @param address the address of the register to write in
    /// `[0x4000, 0x4017]`, except `0x4014` and `0x4016`. See enum Register
    /// for more details.
    /// @param data the data to write to the register
    ///
    void write(uint16_t address, uint8_t data) {
        /// the number of elapsed CPU cycles to this write operation
        static constexpr blip_time_t time = 0;
        /// The length table to lookup length values from registers
        static constexpr unsigned char length_table[0x20] = {
            0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06,
            0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
            0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
            0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E
        };
        /// make sure the given address is legal
        if (address < PULSE0_VOL or address > STATUS)
            throw AddressSpaceException<uint16_t>(address, ADDR_START, ADDR_END);
        /// run the emulator up to the given time
        run_until(time);
        if (address < 0x4010) {  // synthesize registers
            // Write to channel
            int osc_index = (address - ADDR_START) >> 2;
            Oscillator* osc = oscs[osc_index];
            int reg = address & 3;
            osc->regs[reg] = data;
            osc->reg_written[reg] = true;
            /*if (osc_index == 4) {
                // handle DMC specially
            } else */if (reg == 3) {
                // load length counter
                if ((osc_enables >> osc_index) & 1)
                    osc->length_counter = length_table[(data >> 3) & 0x1f];
                // reset square phase
                // DISABLED TO HACK SQUARE OSCILLATOR for VCV Rack
                // if (osc_index < 2)
                //  ((Nes_Square*) osc)->phase = Nes_Square::phase_range - 1;
            }
        } else if (address == 0x4015) {
            // Channel enables
            for (int i = OSC_COUNT; i--;) {
                if (!((data >> i) & 1))
                    oscs[i]->length_counter = 0;
            }
            osc_enables = data;
        } else if (address == 0x4017) {
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

    /// @brief Run all oscillators up to specified time, end current frame,
    /// then start a new frame at time 0.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    inline void end_frame(blip_time_t end_time) {
        if (end_time > last_time) run_until(end_time);
        last_time -= end_time;
    }
};

#endif  // DSP_2A03_HPP_
