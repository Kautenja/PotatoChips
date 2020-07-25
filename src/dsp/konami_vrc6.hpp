// Konami VRC6 chip emulator.
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

#ifndef DSP_KONAMI_VRC6_HPP_
#define DSP_KONAMI_VRC6_HPP_

#include "blip_buffer.hpp"
#include "exceptions.hpp"

/// @brief Konami VRC6 chip emulator.
/// @details
/// the frequency scaling feature is not implemented in the emulation, i.e.,
/// register 0x9003 is invalid in this emulation
///
class KonamiVRC6 {
 public:
    /// the number of oscillators on the VRC6 chip
    static constexpr unsigned OSC_COUNT = 3;
    /// the number of registers per oscillator
    static constexpr unsigned REG_COUNT = 3;

    /// the indexes of the channels on the chip
    enum Channel { PULSE0, PULSE1, SAW };

    /// the IO registers on the VRC6 chip.
    enum Register : uint16_t {
        /// the volume register for pulse waveform generator 0
        PULSE0_DUTY_VOLUME = 0x9000,
        /// the low period register for pulse waveform generator 0
        PULSE0_PERIOD_LOW  = 0x9001,
        /// the high period register for pulse waveform generator 0
        PULSE0_PERIOD_HIGH = 0x9002,
        /// the volume register for pulse waveform generator 1
        PULSE1_DUTY_VOLUME = 0xA000,
        /// the low period register for pulse waveform generator 1
        PULSE1_PERIOD_LOW  = 0xA001,
        /// the high period register for pulse waveform generator 1
        PULSE1_PERIOD_HIGH = 0xA002,
        /// the volume register for quantized saw waveform generator
        SAW_VOLUME         = 0xB000,
        /// the low period register for quantized saw waveform generator
        SAW_PERIOD_LOW     = 0xB001,
        /// the high period register for quantized saw waveform generator
        SAW_PERIOD_HIGH    = 0xB002,
    };

    /// the number of registers per oscillator voice
    static constexpr uint16_t REGS_PER_OSC = 0x1000;

    /// a flag to enable a voice using the period high register
    static constexpr uint8_t PERIOD_HIGH_ENABLED = 0b10000000;

 private:
    /// An oscillator on the KonamiVRC6 chip.
    struct Oscillator {
        /// the register addresses for the oscillator
        enum Register {
            /// the volume register for pulse waveform generator 0
            VOLUME      = 0,
            /// the low period register for pulse waveform generator 0
            PERIOD_LOW  = 1,
            /// the high period register for pulse waveform generator 0
            PERIOD_HIGH = 2,
        };

        /// the internal registers for the oscillator
        uint8_t regs[REG_COUNT];
        /// the output buffer to write samples to
        BLIPBuffer* output;
        /// TODO: document
        int delay;
        /// the last amplitude value output from the synthesizer
        int last_amp;
        /// the phase of the waveform
        int phase;
        /// the amplitude of the waveform, only used by the saw waveform
        int amp;

        /// Return the period of the waveform.
        inline uint16_t period() const {
            // turn the low and high period registers into the 12-bit period
            // value
            return ((regs[PERIOD_HIGH] & 0x0f) << 8) | regs[PERIOD_LOW] + 1;
        }
    };

    /// the oscillators on the chip
    Oscillator oscs[OSC_COUNT];
    /// the time after the last run_until call
    blip_time_t last_time = 0;

    /// a BLIP synthesizer for the saw waveform
    BLIPSynthesizer<blip_med_quality, 31> saw_synth;
    /// a BLIP synthesizer for the square waveform
    BLIPSynthesizer<blip_good_quality, 15> square_synth;

    /// @brief Run VRC6 until specified time.
    ///
    /// @param time the number of elapsed cycles
    ///
    void run_until(blip_time_t time) {
        if (time < last_time)
            throw Exception("end_time must be >= last_time");
        else if (time == last_time)
            return;
        run_square(oscs[0], time);
        run_square(oscs[1], time);
        run_saw(time);
        last_time = time;
    }

    /// @brief Run a square waveform until specified time.
    ///
    /// @param osc the oscillator to run
    /// @param time the number of elapsed cycles
    ///
    void run_square(Oscillator& osc, blip_time_t end_time) {
        BLIPBuffer* output = osc.output;
        if (!output) return;

        int volume = osc.regs[Oscillator::VOLUME] & 15;
        if (!(osc.regs[Oscillator::PERIOD_HIGH] & 0x80)) volume = 0;

        int gate = osc.regs[Oscillator::VOLUME] & 0x80;
        int duty = ((osc.regs[Oscillator::VOLUME] >> 4) & 7) + 1;
        int delta = ((gate || osc.phase < duty) ? volume : 0) - osc.last_amp;
        blip_time_t time = last_time;
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

    /// @brief Run a saw waveform until specified time.
    ///
    /// @param time the number of elapsed cycles
    ///
    void run_saw(blip_time_t end_time) {
        Oscillator& osc = oscs[2];
        BLIPBuffer* output = osc.output;
        if (!output) return;

        int amp = osc.amp;
        int amp_step = osc.regs[Oscillator::VOLUME] & 0x3F;
        blip_time_t time = last_time;
        int last_amp = osc.last_amp;

        if (!(osc.regs[Oscillator::PERIOD_HIGH] & 0x80) || !(amp_step | amp)) {
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

    /// Disable the public copy constructor.
    KonamiVRC6(const KonamiVRC6&);

    /// Disable the public assignment operator.
    KonamiVRC6& operator=(const KonamiVRC6&);

 public:
    /// @brief Initialize a new VRC6 chip emulator.
    KonamiVRC6() {
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
    inline void set_volume(double level = 1.f) {
        level *= 0.0967 * 2;
        saw_synth.volume(level);
        square_synth.volume(level * 0.5);
    }

    /// @brief Set treble equalization for the synthesizers.
    ///
    /// @param equalizer the equalization parameter for the synthesizers
    ///
    inline void set_treble_eq(BLIPEqualizer const& equalizer) {
        saw_synth.treble_eq(equalizer);
        square_synth.treble_eq(equalizer);
    }

    /// @brief Reset internal frame counter, registers, and all oscillators.
    inline void reset() {
        last_time = 0;
        for (unsigned i = 0; i < OSC_COUNT; i++) {
            Oscillator& osc = oscs[i];
            for (unsigned j = 0; j < REG_COUNT; j++)
                osc.regs[j] = 0;
            osc.delay = 0;
            osc.last_amp = 0;
            osc.phase = 1;
            osc.amp = 0;
        }
    }

    /// @brief Write a value to the given oscillator's register.
    ///
    /// @param address the register address to write to
    /// @param data the data to write to the register value
    ///
    inline void write(uint16_t address, uint8_t data) {
        // the number of elapsed cycles
        static constexpr blip_time_t time = 0;
        // run the emulator up to the given time
        run_until(time);
        // get the register number from the address (lowest 2 bits). all 12
        // bits are gathered for error handling
        uint8_t register_address = address & 0b111111111111;
        // get the oscillator index from the address (lowest 2 bits of highest
        // nibble). the lowest 3 bits are taken for error handling. the MSB is
        // always 1, but this is not validated with error handling.
        uint8_t oscillator_index = ((address >> 12) & 0b111) - 1;
        if (oscillator_index >= OSC_COUNT)  // invalid oscillator index
            throw ChannelOutOfBoundsException(oscillator_index, OSC_COUNT);
        if (register_address >= REG_COUNT)  // invalid register address
            throw AddressSpaceException<uint16_t>(register_address, 0, REG_COUNT);
        // set the value for the oscillator index and register address
        oscs[oscillator_index].regs[register_address] = data;
    }

    /// @brief Run all oscillators up to specified time, end current frame,
    /// then start a new frame at time 0.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    inline void end_frame(blip_time_t time) {
        if (time > last_time) run_until(time);
        last_time -= time;
    }
};

#endif  // DSP_KONAMI_VRC6_HPP_
