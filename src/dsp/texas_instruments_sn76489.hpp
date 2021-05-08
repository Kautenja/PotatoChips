// Texas Instruments SN76489 chip emulator.
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

#ifndef DSP_TEXAS_INSTRUMENTS_SN76489_HPP_
#define DSP_TEXAS_INSTRUMENTS_SN76489_HPP_

#include "blip_buffer.hpp"
#include "exceptions.hpp"

/// Texas Instruments SN76489 chip emulator.
class TexasInstrumentsSN76489 {
 public:
    /// the number of voices on the chip
    static constexpr int OSC_COUNT = 4;
    /// the number of tone generators (pulse waveform generators) on the chip
    static constexpr int TONE_COUNT = OSC_COUNT - 1;

    /// the indexes of the voices on the chip
    enum Voice {
        /// the index of the first tone generator voice
        TONE0 = 0,
        /// the index of the second tone generator voice
        TONE1 = 1,
        /// the index of the third tone generator voice
        TONE2 = 2,
        /// the index of the noise generator voice
        NOISE = 3
    };

    /// the values for the linear feedback shift register to take.
    enum LFSR_Values {
        /// N / 512
        N_512    = 0b00,
        /// N / 1024
        N_1024   = 0b01,
        /// N / 2048
        N_2048   = 0b10,
        /// 3rd Tone Generator Output (index 2)
        N_TONE_2 = 0b11
    };

 private:
    /// an abstract base oscillator class for the chip
    struct Oscillator {
        /// the output buffer to write samples to
        BLIPBuffer* output = 0;
        /// a delay before opening the oscillator's amplifier
        int delay = 0;
        /// the value of the waveform amplitude at the last sample
        int last_amp = 0;
        /// the output volume from the synthesizer
        int volume = 0;

        /// @brief Reset the oscillator to its initial state.
        inline void reset() { delay = last_amp = volume = 0; }
    };

    /// @brief A pulse oscillator on the chip
    struct Pulse : Oscillator {
        /// the period of the oscillator
        int period = 0;
        /// the phase of the oscillator
        int phase = 0;
        /// The synthesizer for generating samples from this oscillator
        typedef BLIPSynthesizer<float, BLIP_QUALITY_GOOD, 1> Synth;
        const Synth* synth;

        /// @brief Reset the oscillator to its initial state.
        inline void reset() { period = phase = 0; Oscillator::reset(); }

        /// @brief Run the oscillator from time until end_time.
        void run(int32_t time, int32_t end_time) {
            if (!volume || period <= 128) {
                // ignore 16kHz and higher
                if (last_amp) {
                    synth->offset(time, -last_amp, output);
                    last_amp = 0;
                }
                time += delay;
                if (!period) {
                    time = end_time;
                } else if (time < end_time) {
                    // keep calculating phase
                    int count = (end_time - time + period - 1) / period;
                    phase = (phase + count) & 1;
                    time += count * period;
                }
            } else {
                int amp = phase ? volume : -volume;
                {
                    int delta = amp - last_amp;
                    if (delta) {
                        last_amp = amp;
                        synth->offset(time, delta, output);
                    }
                }

                time += delay;
                if (time < end_time) {
                    BLIPBuffer* const output = this->output;
                    int delta = amp * 2;
                    do {
                        delta = -delta;
                        synth->offset(time, delta, output);
                        time += period;
                        phase ^= 1;
                    } while (time < end_time);
                    last_amp = phase ? volume : -volume;
                }
            }
            delay = time - end_time;
        }
    };

    /// @brief A noise oscillator on the chip
    struct Noise : Oscillator {
        /// the possible noise periods
        static const int noise_periods[3];
        /// the period of the oscillator
        const int* period = &noise_periods[0];
        /// the shift value
        unsigned shifter = 0x8000;
        /// the linear feedback shift registers
        unsigned feedback = 0x9000;
        /// The synthesizer for generating samples from this oscillator
        typedef BLIPSynthesizer<float, BLIP_QUALITY_MEDIUM, 1> Synth;
        Synth synth;
        /// whether the LFSR is on
        bool is_periodic = false;

        /// @brief Reset the oscillator to its initial state.
        inline void reset() {
            period = &noise_periods[0];
            shifter = 0x8000;
            feedback = 0x9000;
            is_periodic = false;
            Oscillator::reset();
        }

        /// @brief Run the oscillator from time until end_time.
        void run(int32_t time, int32_t end_time) {
            int amp = volume;
            if (shifter & 1) amp = -amp;

            int deltaAmp = amp - last_amp;
            if (deltaAmp) {
                last_amp = amp;
                synth.offset(time, deltaAmp, output);
            }

            time += delay;
            if (!volume) time = end_time;

            if (time < end_time) {
                int delta = amp * 2;
                int currentPeriod = *period * 2;
                if (!currentPeriod) currentPeriod = 16;
                do {
                    int changed = shifter + 1;
                    shifter = (feedback & -(shifter & 1)) ^ (shifter >> 1);
                    if (changed & 2) {  // true if bits 0 and 1 differ
                        delta = -delta;
                        synth.offset(time, delta, output);
                    }
                    time += currentPeriod;
                } while (time < end_time);
                last_amp = delta >> 1;
            }
            delay = time - end_time;
        }
    };

    /// the pulse waveform generators
    Pulse pulses[3];
    /// the synthesizer used by the pulse waveform generators
    Pulse::Synth square_synth;
    /// the noise generator
    Noise noise;
    /// The voices on the chip
    Oscillator* voices[OSC_COUNT] {
        &pulses[0],
        &pulses[1],
        &pulses[2],
        &noise
    };

    /// the last time the voices were updated
    int32_t last_time = 0;
    /// the value of the latch register
    int latch = 0;
    /// the value of the LFSR noise
    unsigned noise_feedback = 0;
    /// the value of the white noise
    unsigned looped_feedback = 0;

    /// @brief Run the voices until the given end time.
    ///
    /// @param end_time the time to run the voices until
    ///
    void run_until(int32_t end_time) {
        if (end_time < last_time) {  // time went backwards
            throw Exception("end_time must be >= last_time");
        } else if (end_time > last_time) {  // time moved forwards
            if (pulses[0].output) pulses[0].run(last_time, end_time);
            if (pulses[1].output) pulses[1].run(last_time, end_time);
            if (pulses[2].output) pulses[2].run(last_time, end_time);
            if (noise.output)     noise.run(last_time, end_time);
            last_time = end_time;
        }  // else time has not changed
    }

    /// @brief Disable the copy constructor.
    ///
    /// @param copy the instance to copy
    ///
    TexasInstrumentsSN76489(const TexasInstrumentsSN76489& copy);

    /// @brief Disable the assignment operator.
    ///
    /// @param copy the instance to copy
    ///
    TexasInstrumentsSN76489& operator=(const TexasInstrumentsSN76489&);

 public:
    /// @brief Create a new instance of TexasInstrumentsSN76489.
    TexasInstrumentsSN76489() {
        // set the synthesizer for each pulse waveform generator
        for (int i = 0; i < 3; i++) pulses[i].synth = &square_synth;
        set_output(NULL);
        set_volume();
        reset();
    }

    /// @brief Assign single oscillator output to buffer. If buffer is NULL,
    /// silences the given oscillator.
    ///
    /// @param voice_idx the index of the oscillator to set the output for
    /// @param buffer the BLIPBuffer to output the given voice to
    /// @returns 0 if the output was set successfully, 1 if the index is invalid
    /// @details
    /// If buffer is NULL, the specified oscillator is muted and emulation
    /// accuracy is reduced.
    ///
    inline void set_output(unsigned voice_idx, BLIPBuffer* buffer) {
        if (voice_idx >= OSC_COUNT)  // make sure the voice index is within bounds
            throw ChannelOutOfBoundsException(voice_idx, OSC_COUNT);
        voices[voice_idx]->output = buffer;
    }

    /// @brief Assign all oscillator outputs to specified buffer. If buffer
    /// is NULL, silences all voices.
    ///
    /// @param buffer the single buffer to output the all the voices to
    ///
    inline void set_output(BLIPBuffer* buffer) {
        for (unsigned i = 0; i < OSC_COUNT; i++) set_output(i, buffer);
    }

    /// @brief Set the volume level of all voices.
    ///
    /// @param level the value to set the volume level to, where \f$1.0\f$ is
    /// full volume. Can be overdriven past \f$1.0\f$.
    ///
    inline void set_volume(double level = 1.0) {
        level *= 0.85 / (OSC_COUNT * 64 * 2);
        square_synth.set_volume(level);
        noise.synth.set_volume(level);
    }

    /// @brief Set treble equalization for the synthesizers.
    ///
    /// @param equalizer the equalization parameter for the synthesizers
    ///
    inline void set_treble_eq(const BLIPEqualizer<float>& equalizer) {
        square_synth.set_treble_eq(equalizer);
        noise.synth.set_treble_eq(equalizer);
    }

    /// @brief Reset voices and internal state.
    ///
    /// @param feedback TODO:
    /// @param noise_width TODO:
    ///
    void reset(unsigned feedback = 0, int noise_width = 0) {
        last_time = 0;
        latch = 0;
        // reset the noise
        if (!feedback || !noise_width) {
            feedback = 0x0009;
            noise_width = 16;
        }
        // convert to Galois configuration
        looped_feedback = 1 << (noise_width - 1);
        noise_feedback  = 0;
        while (noise_width--) {
            noise_feedback = (noise_feedback << 1) | (feedback & 1);
            feedback >>= 1;
        }
        // reset the voices
        pulses[0].reset();
        pulses[1].reset();
        pulses[2].reset();
        noise.reset();
    }

    /// @brief Set the frequency to a new value.
    ///
    /// @param voice the index of the tone generator to set the volume of
    /// @param value the 10-bit frequency to set the tone generator to
    ///
    inline void set_frequency(unsigned voice, uint16_t value) {
        pulses[voice].period = (value & 0x3FFF) << 4;
    }

    /// @brief Set the noise mode to a new value.
    ///
    /// @param value the value to set the volume to \f$\in [0, 4]\f$
    /// @param is_periodic true to enable the linear feedback shift register (LFSR)
    /// @param reset true to reset the feedback register and shifter to default
    ///
    inline void set_noise(uint8_t value, bool is_periodic, bool reset = true) {
        // cache the old values to detect changes
        auto old_period = noise.period;
        auto old_is_periodic = noise.is_periodic;
        // update the parameters in the noise structure
        noise.period = (value & 3) < 3 ?
            &Noise::noise_periods[value & 3] : &pulses[2].period;
        noise.is_periodic = is_periodic;
        // return if not resetting and the values haven't changed
        if (!reset && old_period == noise.period && old_is_periodic == is_periodic)
            return;
        // reset the feed register and shifter
        noise.feedback = is_periodic ? noise_feedback : looped_feedback;
        noise.shifter = 0x8000;
    }

    /// @brief Set the volume to a new value.
    ///
    /// @param voice the index of the voice to set the volume of
    /// @param value the value to set the volume to \f$\in [0, 15]\f$
    ///
    inline void set_amplifier_level(unsigned voice, uint8_t value) {
        static constexpr unsigned char volumes[16] =
            {64, 50, 39, 31, 24, 19, 15, 12, 9, 7, 5, 4, 3, 2, 1, 0};
        voices[voice]->volume = volumes[value & 15];
    }

    /// @brief Run all voices up to specified time, end current frame,
    /// then start a new frame at time 0.
    ///
    /// @param end_time the time to run the voices until
    ///
    inline void end_frame(int32_t end_time) {
        if (end_time > last_time) run_until(end_time);
        last_time -= end_time;
    }
};

/// the possible noise periods
const int TexasInstrumentsSN76489::Noise::noise_periods[3] = {0x100, 0x200, 0x400};

#endif  // DSP_TEXAS_INSTRUMENTS_SN76489_HPP_
