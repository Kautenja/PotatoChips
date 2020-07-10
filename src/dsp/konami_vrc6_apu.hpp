// An oscillator based on the Konami VRC6 synthesis chip.
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

#ifndef NES_VRC6_APU_HPP_
#define NES_VRC6_APU_HPP_

#include "blip_buffer/blip_synth.hpp"

/// An oscillator based on the Konami VRC6 synthesis chip.
class VRC6 {
 public:
    /// CPU clock cycle count
    typedef int32_t cpu_time_t;
    /// 16-bit memory address
    typedef int16_t cpu_addr_t;
    /// the number of oscillators on the VRC6 chip
    static constexpr int OSC_COUNT = 3;
    /// the number of registers per oscillator
    static constexpr int REG_COUNT = 3;

    /// Initialize a new VRC6 chip emulator.
    VRC6() { output(NULL); volume(1.0); reset(); }

    /// Reset internal frame counter, registers, and all oscillators.
    inline void reset() {
        last_time = 0;
        for (int i = 0; i < OSC_COUNT; i++) {
            VRC6_Oscillator& osc = oscs[i];
            for (int j = 0; j < REG_COUNT; j++) osc.regs[j] = 0;
            osc.delay = 0;
            osc.last_amp = 0;
            osc.phase = 1;
            osc.amp = 0;
        }
    }

    /// Set the volume.
    ///
    /// @param value the global volume level of the chip
    ///
    inline void volume(double value = 1.f) {
        value *= 0.0967 * 2;
        saw_synth.volume(value);
        square_synth.volume(value * 0.5);
    }

    /// Set treble equalization.
    ///
    /// @param eq the equalizer settings to use
    ///
    inline void treble_eq(blip_eq_t const& equalizer) {
        saw_synth.treble_eq(equalizer);
        square_synth.treble_eq(equalizer);
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
    ///       2) Saw.
    ///
    inline void osc_output(int i, BLIPBuffer* buf) {
        assert((unsigned) i < OSC_COUNT);
        oscs[i].output = buf;
    }

    /// Run all oscillators up to specified time, end current time frame, then
    /// start a new time frame at time 0. Time frames have no effect on
    /// emulation and each can be whatever length is convenient.
    ///
    /// @param time the number of elapsed cycles
    ///
    inline void end_frame(cpu_time_t time) {
        if (time > last_time) run_until(time);
        last_time -= time;
        assert(last_time >= 0);
    }

    /// Write a value to the given oscillator's register.
    ///
    /// @param time the number of elapsed cycles
    /// @param osc_index the index of the oscillator
    /// @param reg the index of the synthesizer's register
    /// @param data the data to write to the register value
    ///
    inline void write_osc(cpu_time_t time, int osc_index, int reg, int data) {
        assert((unsigned) osc_index < OSC_COUNT);
        assert((unsigned) reg < REG_COUNT);
        run_until(time);
        oscs[osc_index].regs[reg] = data;
    }

 private:
    /// Disable the public copy constructor.
    VRC6(const VRC6&);

    /// Disable the public assignment operator.
    VRC6& operator = (const VRC6&);

    /// An oscillator on the VRC6 chip.
    struct VRC6_Oscillator {
        /// the internal registers for the oscillator
        uint8_t regs[3];
        /// the output buffer to write samples to
        BLIPBuffer* output;
        /// TODO: document
        int delay;
        /// TODO: document
        int last_amp;
        /// the phase of the waveform
        int phase;
        /// the amplitude of the waveform, only used by the saw waveform
        int amp;

        /// Return the period of the waveform.
        inline int period() const {
            return (regs[2] & 0x0f) * 0x100L + regs[1] + 1;
        }
    };

    /// the oscillators on the chip
    VRC6_Oscillator oscs[OSC_COUNT];
    /// the time after the last run_until call
    cpu_time_t last_time = 0;

    /// a BLIP synthesizer for the saw waveform
    BLIPSynth<BLIPQuality::Medium, 31> saw_synth;
    /// a BLIP synthesizer for the square waveform
    BLIPSynth<BLIPQuality::Good, 15> square_synth;

    /// Run VRC6 until specified time.
    ///
    /// @param time the number of elapsed cycles
    ///
    void run_until(cpu_time_t time) {
        assert(time >= last_time);
        run_square(oscs[0], time);
        run_square(oscs[1], time);
        run_saw(time);
        last_time = time;
    }

    /// Run a square waveform until specified time.
    ///
    /// @param osc the oscillator to run
    /// @param time the number of elapsed cycles
    ///
    void run_square(VRC6_Oscillator& osc, cpu_time_t end_time) {
        BLIPBuffer* output = osc.output;
        if (!output) return;

        int volume = osc.regs[0] & 15;
        if (!(osc.regs[2] & 0x80)) volume = 0;

        int gate = osc.regs[0] & 0x80;
        int duty = ((osc.regs[0] >> 4) & 7) + 1;
        int delta = ((gate || osc.phase < duty) ? volume : 0) - osc.last_amp;
        cpu_time_t time = last_time;
        if (delta) {
            osc.last_amp += delta;
            square_synth.offset(time, delta, output);
        }

        time += osc.delay;
        osc.delay = 0;
        int period = osc.period();
        if (volume && !gate && period > 4) {
            if (time < end_time) {
                int phase = osc.phase;
                do {
                    phase++;
                    if (phase == 16) {
                        phase = 0;
                        osc.last_amp = volume;
                        square_synth.offset(time, volume, output);
                    }
                    if (phase == duty) {
                        osc.last_amp = 0;
                        square_synth.offset(time, -volume, output);
                    }
                    time += period;
                } while (time < end_time);
                osc.phase = phase;
            }
            osc.delay = time - end_time;
        }
    }

    /// Run a saw waveform until specified time.
    ///
    /// @param time the number of elapsed cycles
    ///
    void run_saw(cpu_time_t end_time) {
        VRC6_Oscillator& osc = oscs[2];
        BLIPBuffer* output = osc.output;
        if (!output) return;

        int amp = osc.amp;
        int amp_step = osc.regs[0] & 0x3F;
        cpu_time_t time = last_time;
        int last_amp = osc.last_amp;

        if (!(osc.regs[2] & 0x80) || !(amp_step | amp)) {
            osc.delay = 0;
            int delta = (amp >> 3) - last_amp;
            last_amp = amp >> 3;
            saw_synth.offset(time, delta, output);
        } else {
            time += osc.delay;
            if (time < end_time) {
                int period = osc.period() * 2;
                int phase = osc.phase;
                do {
                    if (--phase == 0) {
                        phase = 7;
                        amp = 0;
                    }
                    int delta = (amp >> 3) - last_amp;
                    if (delta) {
                        last_amp = amp >> 3;
                        saw_synth.offset(time, delta, output);
                    }
                    time += period;
                    amp = (amp + amp_step) & 0xFF;
                } while (time < end_time);
                osc.phase = phase;
                osc.amp = amp;
            }
            osc.delay = time - end_time;
        }
        osc.last_amp = last_amp;
    }
};

#endif  // NES_VRC6_APU_HPP_
