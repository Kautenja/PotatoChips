// Konami SCC sound chip emulator.
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

#ifndef DSP_KONAMI_SCC_HPP_
#define DSP_KONAMI_SCC_HPP_

#include "blip_buffer.hpp"
#include "exceptions.hpp"
#include <cstring>

// TODO: c-cast to static_cast

/// @brief Konami SCC sound chip emulator.
class KonamiSCC {
 public:
    /// the number of oscillators on the chip
    enum { OSC_COUNT = 5 };
    /// the number of registers on the chip
    enum { REGISTER_COUNT = 0x90 };

    /// the size of the wave-tables on the chip in bytes
    int static constexpr WAVE_SIZE = 32;

    /// the registers on the Konami SCC
    enum Registers {
        WAVEFORM_CH_1,
        WAVEFORM_CH_2     = 1 * WAVE_SIZE,
        WAVEFORM_CH_3     = 2 * WAVE_SIZE,
        WAVEFORM_CH_4     = 3 * WAVE_SIZE,
        FREQUENCY_CH_1_LO = 4 * WAVE_SIZE,
        FREQUENCY_CH_1_HI,
        FREQUENCY_CH_2_LO,
        FREQUENCY_CH_2_HI,
        FREQUENCY_CH_3_LO,
        FREQUENCY_CH_3_HI,
        FREQUENCY_CH_4_LO,
        FREQUENCY_CH_4_HI,
        FREQUENCY_CH_5_LO,
        FREQUENCY_CH_5_HI,
        VOLUME_CH_1,
        VOLUME_CH_2,
        VOLUME_CH_3,
        VOLUME_CH_4,
        VOLUME_CH_5,
        POWER,
    };

    /// a flag to indicate that the volume is on
    static constexpr uint8_t VOLUME_ON    = 0b00010000;

    /// a flag for the power register to indicate that all 5 channels are on
    static constexpr uint8_t POWER_ALL_ON = 0b00011111;

 private:
    /// the range of the amplifier on the chip
    enum { AMP_RANGE = 0x8000 };
    /// Tones above this frequency are treated as disabled tone at half volume.
    /// Power of two is more efficient (avoids division).
    unsigned static constexpr INAUDIBLE_FREQ = AMP_RANGE / 2;

    /// An oscillators on the chip.
    struct osc_t {
        /// TODO:
        int delay = 0;
        /// TODO:
        int phase = 0;
        /// TODO:
        int last_amp = 0;
        /// TODO:
        BLIPBuffer* output = NULL;
    };

    /// the oscillators on the chip
    osc_t oscs[OSC_COUNT];
    /// the last time the oscillators were updated
    blip_time_t last_time = 0;
    /// the registers on the chip
    unsigned char regs[REGISTER_COUNT];
    /// the synthesizer for the oscillators on the chip
    BLIPSynthesizer<blip_med_quality, 1> synth;

    /// Run the oscillators until the given end time.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    void run_until(blip_time_t end_time) {
        for (int index = 0; index < OSC_COUNT; index++) {
            osc_t& osc = oscs[index];
            // get the output buffer for this oscillator, continue if it's null
            BLIPBuffer* const output = osc.output;
            if (!output) continue;
            // get the period of the oscillator
            blip_time_t period = (regs[0x80 + index * 2 + 1] & 0x0F) * 0x100 + regs[0x80 + index * 2] + 1;
            int volume = 0;
            if (regs[0x8F] & (1 << index)) {
                blip_time_t inaudible_period = (output->get_clock_rate() + INAUDIBLE_FREQ * 32) / (INAUDIBLE_FREQ * 16);
                if (period > inaudible_period)
                    volume = (regs[0x8A + index] & 0x0F) * (AMP_RANGE / 256 / 15);
            }
            // get the wave for the oscillator
            int8_t const* wave = (int8_t*) regs + index * WAVE_SIZE;
            // the last two oscillators share a wave
            if (index == OSC_COUNT - 1) wave -= WAVE_SIZE;
            {  // confine scope of `amp` and `delta`
                int amp = wave[osc.phase] * volume;
                int delta = amp - osc.last_amp;
                if (delta) {
                    osc.last_amp = amp;
                    synth.offset(last_time, delta, output);
                }
            }
            // get the time to advance to
            blip_time_t time = last_time + osc.delay;
            if (time < end_time) {
                if (!volume) {  // maintain phase
                    blip_time_t count = (end_time - time + period - 1) / period;
                    osc.phase = (osc.phase + count) & (WAVE_SIZE - 1);
                    time += count * period;
                } else {
                    int phase = osc.phase;
                    int last_wave = wave[phase];
                    // pre-advance for optimal inner loop
                    phase = (phase + 1) & (WAVE_SIZE - 1);
                    do {
                        int amp = wave[phase];
                        phase = (phase + 1) & (WAVE_SIZE - 1);
                        int delta = amp - last_wave;
                        if (delta) {
                            last_wave = amp;
                            synth.offset(time, delta * volume, output);
                        }
                        time += period;
                    } while (time < end_time);
                    // undo pre-advance
                    osc.phase = phase = (phase - 1) & (WAVE_SIZE - 1);
                    osc.last_amp = wave[phase] * volume;
                }
            }
            osc.delay = time - end_time;
        }
        last_time = end_time;
    }

 public:
    /// Initialize a new Konami SCC.
    KonamiSCC() {
        memset(regs, 0, sizeof regs);
    }

    /// Set overall volume of all oscillators, where 1.0 is full volume
    ///
    /// @param level the value to set the volume to
    ///
    inline void set_volume(double v) {
        synth.volume(0.43 / OSC_COUNT / AMP_RANGE * v);
    }

    /// Set treble equalization for the synthesizers.
    ///
    /// @param equalizer the equalization parameter for the synthesizers
    ///
    inline void treble_eq(BLIPEqualizer const& equalizer) {
        synth.treble_eq(equalizer);
    }

    /// Set sound output of specific oscillator to buffer, where index is
    /// 0 to 4. If buffer is NULL, the specified oscillator is muted.
    inline void set_output(int index, BLIPBuffer* buffer) {
        assert((unsigned) index < OSC_COUNT);
        oscs[index].output = buffer;
    }

    /// Set buffer to generate all sound into, or disable sound if NULL
    inline void set_output(BLIPBuffer* buffer) {
        for (int i = 0; i < OSC_COUNT; i++)
            set_output(i, buffer);
    }

    /// Reset oscillators and internal state.
    inline void reset() {
        last_time = 0;
        for (int i = 0; i < OSC_COUNT; i++)
            memset(&oscs[i], 0, offsetof(osc_t, output));
        memset(regs, 0, sizeof regs);
    }

    /// Write to the data port.
    ///
    /// @param addr the register to write the data to
    /// @param data the byte to write to the register at given address
    ///
    inline void write(int addr, int data) {
        static constexpr blip_time_t time = 0;
        assert((unsigned) addr < REGISTER_COUNT);
        run_until(time);
        regs[addr] = data;
    }

    /// Run all oscillators up to specified time, end current frame, then
    /// start a new frame at time 0.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    inline void end_frame(blip_time_t end_time) {
        if (end_time > last_time)
            run_until(end_time);
        last_time -= end_time;
        assert(last_time >= 0);
    }
};

#endif  // DSP_KONAMI_SCC_HPP_
