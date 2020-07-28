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

/// @brief Konami SCC sound chip emulator.
class KonamiSCC {
 public:
    /// the number of oscillators on the chip
    static constexpr unsigned OSC_COUNT = 5;
    /// the first address of the RAM space
    static constexpr uint16_t ADDR_START = 0x0000;
    /// the last address of the RAM space
    static constexpr uint16_t ADDR_END   = 0x0090;
    /// the number of registers on the chip
    static constexpr uint16_t NUM_REGISTERS = ADDR_END - ADDR_START;

    /// the size of the wave-tables on the chip in bytes
    static constexpr uint16_t WAVE_SIZE = 32;

    /// the registers on the Konami SCC
    enum Register : uint16_t {
        /// the register for the waveform for channel 1
        WAVEFORM_CH_1,
        /// the register for the waveform for channel 2
        WAVEFORM_CH_2     = 1 * WAVE_SIZE,
        /// the register for the waveform for channel 3
        WAVEFORM_CH_3     = 2 * WAVE_SIZE,
        /// the register for the waveform for channel 4
        WAVEFORM_CH_4     = 3 * WAVE_SIZE,
        /// the register for the low 8 bits of the frequency for channel 1
        FREQUENCY_CH_1_LO = 4 * WAVE_SIZE,
        /// the register for the high 4 bits of the frequency for channel 1
        FREQUENCY_CH_1_HI,
        /// the register for the low 8 bits of the frequency for channel 2
        FREQUENCY_CH_2_LO,
        /// the register for the high 4 bits of the frequency for channel 2
        FREQUENCY_CH_2_HI,
        /// the register for the low 8 bits of the frequency for channel 3
        FREQUENCY_CH_3_LO,
        /// the register for the high 4 bits of the frequency for channel 3
        FREQUENCY_CH_3_HI,
        /// the register for the low 8 bits of the frequency for channel 4
        FREQUENCY_CH_4_LO,
        /// the register for the high 4 bits of the frequency for channel 4
        FREQUENCY_CH_4_HI,
        /// the register for the low 8 bits of the frequency for channel 5
        FREQUENCY_CH_5_LO,
        /// the register for the high 4 bits of the frequency for channel 5
        FREQUENCY_CH_5_HI,
        /// the volume level for channel 1
        VOLUME_CH_1,
        /// the volume level for channel 2
        VOLUME_CH_2,
        /// the volume level for channel 3
        VOLUME_CH_3,
        /// the volume level for channel 4
        VOLUME_CH_4,
        /// the volume level for channel 5
        VOLUME_CH_5,
        /// the global power control register
        POWER
    };

    /// a flag that denotes that the volume is on for a VOLUME_CH_# register
    static constexpr uint8_t VOLUME_ON    = 0b00010000;

    /// a flag for the power register that denotes that all 5 channels are on
    static constexpr uint8_t POWER_ALL_ON = 0b00011111;

 private:
    /// the range of the amplifier on the chip
    enum { AMP_RANGE = 0x8000 };
    /// Tones above this frequency are treated as disabled tone at half volume.
    /// Power of two is more efficient (avoids division).
    static constexpr unsigned INAUDIBLE_FREQ = AMP_RANGE / 2;

    /// An oscillators on the chip.
    struct Oscillator {
        /// TODO:
        int delay = 0;
        /// the current phase of the oscillator
        int phase = 0;
        /// the last amplitude value to output from the oscillator
        int last_amp = 0;
        /// the output buffer to write samples from the oscillator to
        BLIPBuffer* output = NULL;
    };

    /// the oscillators on the chip
    Oscillator oscs[OSC_COUNT];
    /// the last time the oscillators were updated
    blip_time_t last_time = 0;
    /// the registers on the chip
    uint8_t regs[NUM_REGISTERS];
    /// the synthesizer for the oscillators on the chip
    BLIPSynthesizer<BLIP_QUALITY_MEDIUM, 1> synth;

    /// Run the oscillators until the given end time.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    void run_until(blip_time_t end_time) {
        if (end_time < last_time)
            throw Exception("end_time must be >= last_time");
        else if (end_time == last_time)
            return;
        for (unsigned index = 0; index < OSC_COUNT; index++) {
            Oscillator& osc = oscs[index];
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
        set_output(NULL);
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
    inline void set_output(unsigned channel, BLIPBuffer* buffer) {
        if (channel >= OSC_COUNT)  // make sure the channel is within bounds
            throw ChannelOutOfBoundsException(channel, OSC_COUNT);
        oscs[channel].output = buffer;
    }

    /// @brief Assign all oscillator outputs to specified buffer. If buffer
    /// is NULL, silences all oscillators.
    ///
    /// @param buffer the single buffer to output the all the voices to
    ///
    inline void set_output(BLIPBuffer* buffer) {
        for (unsigned channel = 0; channel < OSC_COUNT; channel++)
            set_output(channel, buffer);
    }

    /// @brief Set the volume level of all oscillators.
    ///
    /// @param level the value to set the volume level to, where \f$1.0\f$ is
    /// full volume. Can be overdriven past \f$1.0\f$.
    ///
    inline void set_volume(double level = 1.0) {
        synth.set_volume(0.43 / OSC_COUNT / AMP_RANGE * level);
    }

    /// @brief Set treble equalization for the synthesizers.
    ///
    /// @param equalizer the equalization parameter for the synthesizers
    ///
    inline void set_treble_eq(BLIPEqualizer const& equalizer) {
        synth.set_treble_eq(equalizer);
    }

    /// @brief Reset oscillators and internal state.
    inline void reset() {
        last_time = 0;
        for (unsigned i = 0; i < OSC_COUNT; i++)  // clear the oscillators
            memset(&oscs[i], 0, offsetof(Oscillator, output));
        // clear the registers
        memset(regs, 0, sizeof regs);
    }

    /// @brief Write to the data port.
    ///
    /// @param addr the register to write the data to
    /// @param data the byte to write to the register at given address
    ///
    inline void write(uint16_t address, uint8_t data) {
        static constexpr blip_time_t time = 0;
        // make sure the given address is legal. the starting address is 0,
        // and address is unsigned, so just check the upper bound
        if (/*address < ADDR_START or*/ address > ADDR_END)
            throw AddressSpaceException<uint16_t>(address, ADDR_START, ADDR_END);
        run_until(time);
        regs[address] = data;
    }

    /// @brief Run all oscillators up to specified time, end current frame,
    /// then start a new frame at time 0.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    inline void end_frame(blip_time_t end_time) {
        run_until(end_time);
        last_time -= end_time;
    }
};

#endif  // DSP_KONAMI_SCC_HPP_
