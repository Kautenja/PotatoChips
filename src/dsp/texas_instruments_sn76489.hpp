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
// derived from: Game_Music_Emu 0.5.2
//

#ifndef DSP_TEXAS_INSTRUMENTS_SN76489_HPP_
#define DSP_TEXAS_INSTRUMENTS_SN76489_HPP_

#include "blip_buffer.hpp"
#include "exceptions.hpp"

/// Texas Instruments SN76489 chip emulator.
class TexasInstrumentsSN76489 {
 public:
    /// the number of oscillators on the chip
    static constexpr int OSC_COUNT = 4;

    /// the indexes of the channels on the chip
    enum Channel {
        PULSE0,
        PULSE1,
        PULSE2,
        NOISE
    };

    /// the registers on the SN76489
    enum Register : uint8_t {
        /// the frequency register for pulse generator 0
        TONE_0_FREQUENCY   = 0b10000000,
        /// the attenuation register for pulse generator 0
        TONE_0_ATTENUATION = 0b10010000,
        /// the frequency register for pulse generator 1
        TONE_1_FREQUENCY   = 0b10100000,
        /// the attenuation register for pulse generator 1
        TONE_1_ATTENUATION = 0b10110000,
        /// the frequency register for pulse generator 2
        TONE_2_FREQUENCY   = 0b11000000,
        /// the attenuation register for pulse generator 2
        TONE_2_ATTENUATION = 0b11010000,
        /// the control register for the noise generator
        NOISE_CONTROL      = 0b11100000,
        /// the attenuation register for noise generator
        NOISE_ATTENUATION  = 0b11110000
    };

    /// the values for the linear feedback shift register to take.
    enum LFSR_Values {
        /// N / 512
        N_512    = 0b00,
        /// N / 1024
        N_1024   = 0b01,
        /// N / 2048
        N_2048   = 0b10,
        /// Tone Generator #3 Output
        N_TONE_3 = 0b11
    };

    /// the FB bit in the Noise control register
    static constexpr uint8_t NOISE_FEEDBACK = 0b00000100;

 private:
    /// an abstract base oscillator class for the chip
    struct Oscillator {
        /// the output buffer to write samples to
        BLIPBuffer* output = 0;
        /// TODO:
        int delay = 0;
        /// the value of the waveform amplitude at the last sample
        int last_amp = 0;
        /// the output volume from the synthesizer
        int volume = 0;

        /// Reset the oscillator to its initial state.
        inline void reset() {
            delay = 0;
            last_amp = 0;
            volume = 0;
        }
    };

    /// a pulse oscillator on the chip
    struct Pulse : Oscillator {
        /// the period of the oscillator
        int period = 0;
        /// the phase of the oscillator
        int phase = 0;
        /// The synthesizer for generating samples from this oscillator
        typedef BLIPSynthesizer<blip_good_quality, 1> Synth;
        const Synth* synth;

        /// Reset the oscillator to its initial state.
        inline void reset() {
            period = 0;
            phase = 0;
            Oscillator::reset();
        }

        /// Run the oscillator from time until end_time.
        void run(blip_time_t time, blip_time_t end_time) {
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
                    this->last_amp = phase ? volume : -volume;
                }
            }
            delay = time - end_time;
        }
    };

    /// a noise oscillator on the chip
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
        typedef BLIPSynthesizer<blip_med_quality, 1> Synth;
        Synth synth;

        /// Reset the oscillator to its initial state.
        inline void reset() {
            period = &noise_periods[0];
            shifter = 0x8000;
            feedback = 0x9000;
            Oscillator::reset();
        }

        /// Run the oscillator from time until end_time.
        void run(blip_time_t time, blip_time_t end_time) {
            int amp = volume;
            if (shifter & 1)
                amp = -amp;

            {
                int delta = amp - last_amp;
                if (delta) {
                    last_amp = amp;
                    synth.offset(time, delta, output);
                }
            }

            time += delay;
            if (!volume)
                time = end_time;

            if (time < end_time) {
                BLIPBuffer* const output = this->output;
                unsigned shifter = this->shifter;
                int delta = amp * 2;
                int period = *this->period * 2;
                if (!period)
                    period = 16;

                do {
                    int changed = shifter + 1;
                    shifter = (feedback & -(shifter & 1)) ^ (shifter >> 1);
                    if (changed & 2) // true if bits 0 and 1 differ
                    {
                        delta = -delta;
                        synth.offset(time, delta, output);
                    }
                    time += period;
                } while (time < end_time);

                this->shifter = shifter;
                this->last_amp = delta >> 1;
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
    /// The oscillators on the chip
    Oscillator* oscs[OSC_COUNT] {
        &pulses[0],
        &pulses[1],
        &pulses[2],
        &noise
    };

    /// the last time the oscillators were updated
    blip_time_t last_time = 0;
    /// the value of the latch register
    int latch = 0;
    /// the value of the LFSR noise
    unsigned noise_feedback = 0;
    /// the value of the white noise
    unsigned looped_feedback = 0;

    /// Run the oscillators until the given end time.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    void run_until(blip_time_t end_time) {
        if (end_time < last_time)
            throw Exception("end_time must be >= last_time");
        if (end_time > last_time) {  // run oscillators if time is different
            if (pulses[0].output) pulses[0].run(last_time, end_time);
            if (pulses[1].output) pulses[1].run(last_time, end_time);
            if (pulses[2].output) pulses[2].run(last_time, end_time);
            if (noise.output)      noise.run(last_time, end_time);
            last_time = end_time;
        }
    }

    /// Disable the copy constructor.
    TexasInstrumentsSN76489(const TexasInstrumentsSN76489&);

    /// Disable the assignment operator
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
        oscs[channel]->output = buffer;
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
        level *= 0.85 / (OSC_COUNT * 64 * 2);
        square_synth.volume(level);
        noise.synth.volume(level);
    }

    /// @brief Set treble equalization for the synthesizers.
    ///
    /// @param equalizer the equalization parameter for the synthesizers
    ///
    inline void set_treble_eq(const BLIPEqualizer& equalizer) {
        square_synth.set_treble_eq(equalizer);
        noise.synth.set_treble_eq(equalizer);
    }

    /// @brief Reset oscillators and internal state.
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
        // convert to "Galios configuration"
        looped_feedback = 1 << (noise_width - 1);
        noise_feedback  = 0;
        while (noise_width--) {
            noise_feedback = (noise_feedback << 1) | (feedback & 1);
            feedback >>= 1;
        }
        // reset the oscillators
        pulses[0].reset();
        pulses[1].reset();
        pulses[2].reset();
        noise.reset();
    }

    // TODO: update with address / latch separated from data port
    /// @brief Write to the data port.
    ///
    /// @param data the byte to write to the data port
    ///
    void write(uint8_t data) {
        // the possible volume values
        static constexpr unsigned char volumes[16] = {
            64, 50, 39, 31, 24, 19, 15, 12, 9, 7, 5, 4, 3, 2, 1, 0
        };
        // set the latch if the MSB is high
        if (data & 0x80) latch = data;
        // get the index of the register
        int index = (latch >> 5) & 3;
        if (latch & 0x10) {  // volume
            oscs[index]->volume = volumes[data & 15];
        } else if (index < 3) {  // pulse frequency
            Pulse& sq = pulses[index];
            if (data & 0x80)
                sq.period = (sq.period & 0xFF00) | (data << 4 & 0x00FF);
            else
                sq.period = (sq.period & 0x00FF) | (data << 8 & 0x3F00);
        } else {  // noise
            int select = data & 3;
            if (select < 3)
                noise.period = &Noise::noise_periods[select];
            else
                noise.period = &pulses[2].period;
            noise.feedback = (data & 0x04) ? noise_feedback : looped_feedback;
            noise.shifter = 0x8000;
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

/// the possible noise periods
const int TexasInstrumentsSN76489::Noise::noise_periods[3] = {0x100, 0x200, 0x400};

#endif  // DSP_TEXAS_INSTRUMENTS_SN76489_HPP_
