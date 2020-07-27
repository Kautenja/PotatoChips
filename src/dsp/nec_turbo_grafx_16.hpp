// Turbo Grafx 16 sound chip emulator.
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

#ifndef DSP_NEC_TURBO_GRAFX_16_HPP_
#define DSP_NEC_TURBO_GRAFX_16_HPP_

#include "blip_buffer.hpp"
#include "exceptions.hpp"
#include <cstring>

/// Turbo Grafx 16 (PC Engine) PSG sound chip emulator.
/// @details
/// The current LFSR algorithm is not accurate to the Turbo Grafx 16
///
class NECTurboGrafx16 {
 public:
    /// the number of oscillators on the chip
    static constexpr int OSC_COUNT = 6;
    /// the first address of the RAM space
    static constexpr int ADDR_START = 0x0800;
    /// the last address of the RAM space
    static constexpr int ADDR_END   = 0x0809;
    /// the number of registers on the chip
    static constexpr uint16_t NUM_REGISTERS = ADDR_END - ADDR_START;

    /// the indexes of the channels on the chip
    enum Channel {
        WAVE0,
        WAVE1,
        WAVE2,
        WAVE3,
        WAVE4,
        WAVE5,
    };

    /// the IO registers on the chip
    enum Register : uint16_t {
        /// The register for selecting the active channel
        CHANNEL_SELECT  = 0x0800,
        /// The register for setting the main volume output from the chip
        MAIN_VOLUME     = 0x0801,
        /// The register with the low 8 bits of the 12-bit frequency value for
        /// the active channel
        CHANNEL_FREQ_LO = 0x0802,
        /// The register with the high 4 bits of the 12-bit frequency value for
        /// the active channel
        CHANNEL_FREQ_HI = 0x0803,
        /// The register for setting the volume / enabling the active channel.
        /// This register must be cleared before writing wave data for the
        /// active channel.
        CHANNEL_VOLUME  = 0x0804,
        /// The register with the stereo balance for the active channel
        CHANNEL_BALANCE = 0x0805,
        /// The register to write wave-table data to for the active channel
        CHANNEL_WAVE    = 0x0806,
        /// The register to write noise control data to for the active channel
        CHANNEL_NOISE   = 0x0807,
    };

    /// a flag for the CHANNEL_VOLUME register to enable the active channel
    static constexpr uint8_t CHANNEL_VOLUME_ENABLE = 0b10000000;

 private:
    /// reduces asymmetry and clamping when starting notes
    static constexpr bool CENTER_WAVES = true;

    /// @brief Turbo Grafx 16 sound chip emulator.
    struct Oscillator {
        /// the waveform to generate (i.e., the wavetable)
        uint8_t wave[32];
        /// the stereo volume for the oscillator
        short volume[2];
        /// the last amplitude value to emit from the oscillator
        int last_amp[2];
        /// TODO:
        int delay;
        /// the period of the oscillator
        int period;
        /// TODO:
        uint8_t noise;
        /// the phase of the oscillator
        uint8_t phase;
        /// the balance of the oscillator between left and right channels
        uint8_t balance;
        /// TODO:
        uint8_t dac;
        /// the last time that the oscillator was updated
        blip_time_t last_time;

        /// the left, center, and right channel output buffers for the oscillator
        BLIPBuffer* outputs[2];
        /// the left, center, right, and active channel output buffers for the oscillator
        BLIPBuffer* chans[3];
        /// the linear feedback shift register for noise
        unsigned noise_lfsr;
        /// the control register for the oscillator
        uint8_t control;

        /// the range of the amplifier on the oscillator
        enum { AMP_RANGE = 0x8000 };
        /// the synthesizer type that the oscillator uses
        typedef BLIPSynthesizer<blip_med_quality,1> Synthesizer;

        /// @brief Run the oscillator until specified time.
        ///
        /// @param synth the synthesizer to use for generating samples
        /// @param time the number of elapsed cycles
        ///
        void run_until(const Synthesizer& synth, blip_time_t end_time) {
            if (end_time < last_time)
                throw Exception("end_time must be >= last_time");
            else if (end_time == last_time)
                return;
            // cache often-used values
            BLIPBuffer* const osc_outputs_0 = outputs[0];
            if (osc_outputs_0 && control & 0x80) {
                int dac = this->dac;

                int const volume_0 = volume[0];
                {
                    int delta = dac * volume_0 - last_amp[0];
                    if (delta) synth.offset(last_time, delta, osc_outputs_0);
                }

                BLIPBuffer* const osc_outputs_1 = outputs[1];
                int const volume_1 = volume[1];
                if (osc_outputs_1) {
                    int delta = dac * volume_1 - last_amp[1];
                    if (delta) synth.offset(last_time, delta, osc_outputs_1);
                }

                blip_time_t time = last_time + delay;
                if (time < end_time) {
                    if (noise & 0x80) {
                        if (volume_0 | volume_1) {
                            // noise
                            // TODO: correct?
                            int const noise_period = (32 - (noise & 0x1F)) * 64;
                            do {
                                int new_dac = 0x1F & -(noise_lfsr >> 1 & 1);
                                // Implemented using "Galios configuration"
                                // TODO: find correct LFSR algorithm
                                noise_lfsr = (noise_lfsr >> 1) ^ (0xE008 & - (noise_lfsr & 1));
                                // noise_lfsr = (noise_lfsr >> 1) ^ (0x6000 & - (noise_lfsr & 1));
                                int delta = new_dac - dac;
                                if (delta) {
                                    dac = new_dac;
                                    synth.offset(time, delta * volume_0, osc_outputs_0);
                                    if (osc_outputs_1)
                                        synth.offset(time, delta * volume_1, osc_outputs_1);
                                }
                                time += noise_period;
                            } while (time < end_time);
                        }
                    } else if (!(control & 0x40)) {  // wave
                        // pre-advance for optimal inner loop
                        int phase = (this->phase + 1) & 0x1F;
                        int period = this->period * 2;
                        if (period >= 14 && (volume_0 | volume_1)) {
                            do {
                                int new_dac = wave[phase];
                                phase = (phase + 1) & 0x1F;
                                int delta = new_dac - dac;
                                if (delta) {
                                    dac = new_dac;
                                    synth.offset(time, delta * volume_0, osc_outputs_0);
                                    if (osc_outputs_1)
                                        synth.offset(time, delta * volume_1, osc_outputs_1);
                                }
                                time += period;
                            } while (time < end_time);
                        } else {
                            if (!period) {
                                // TODO: Gekisha Boy assumes that period = 0 silences wave
                                // period = 0x1000 * 2;
                                period = 1;
                                // if (!(volume_0 | volume_1))
                                //     dprintf("Used period 0\n");
                            }
                            // maintain phase when silent
                            blip_time_t count = (end_time - time + period - 1) / period;
                            phase += count; // phase will be masked below
                            time += count * period;
                        }
                        // undo pre-advance
                        this->phase = (phase - 1) & 0x1F;
                    }
                }
                time -= end_time;
                if (time < 0) time = 0;
                delay = time;

                this->dac = dac;
                last_amp[0] = dac * volume_0;
                last_amp[1] = dac * volume_1;
            }
            last_time = end_time;
        }
    };

    /// the oscillators on the chip
    Oscillator oscs[OSC_COUNT];
    /// the latch address to read/write from/to
    int latch;
    /// the balance between left and right channels
    int balance;
    /// the synthesizer for producing samples from the chip
    Oscillator::Synthesizer synth;

    /// Update the volume levels for an oscillator after changing the balance.
    ///
    /// @param osc the oscillator to update the volume levels of
    ///
    void balance_changed(Oscillator& osc) {
        static const short log_table[32] = {  // ~1.5 db per step
            #define ENTRY(factor) short (factor * Oscillator::AMP_RANGE / 31.0 + 0.5)
            ENTRY(0.000000), ENTRY(0.005524), ENTRY(0.006570), ENTRY(0.007813),
            ENTRY(0.009291), ENTRY(0.011049), ENTRY(0.013139), ENTRY(0.015625),
            ENTRY(0.018581), ENTRY(0.022097), ENTRY(0.026278), ENTRY(0.031250),
            ENTRY(0.037163), ENTRY(0.044194), ENTRY(0.052556), ENTRY(0.062500),
            ENTRY(0.074325), ENTRY(0.088388), ENTRY(0.105112), ENTRY(0.125000),
            ENTRY(0.148651), ENTRY(0.176777), ENTRY(0.210224), ENTRY(0.250000),
            ENTRY(0.297302), ENTRY(0.353553), ENTRY(0.420448), ENTRY(0.500000),
            ENTRY(0.594604), ENTRY(0.707107), ENTRY(0.840896), ENTRY(1.000000),
            #undef ENTRY
        };

        int vol = (osc.control & 0x1F) - 0x1E * 2;

        int left  = vol + (osc.balance >> 3 & 0x1E) + (balance >> 3 & 0x1E);
        if (left  < 0) left  = 0;

        int right = vol + (osc.balance << 1 & 0x1E) + (balance << 1 & 0x1E);
        if (right < 0) right = 0;

        left  = log_table[left ];
        right = log_table[right];

        // optimizing for the common case of being centered also allows easy
        // panning using Effects_Buffer
        osc.outputs[0] = osc.chans[0];  // center
        osc.outputs[1] = 0;
        if (left != right) {
            osc.outputs[0] = osc.chans[1];  // left
            osc.outputs[1] = osc.chans[2];  // right
        }

        if (CENTER_WAVES) {
            osc.last_amp[0] += (left  - osc.volume[0]) * 16;
            osc.last_amp[1] += (right - osc.volume[1]) * 16;
        }

        osc.volume[0] = left;
        osc.volume[1] = right;
    }

 public:
    /// Initialize a new Turbo Grafx 16 chip.
    NECTurboGrafx16() {
        for (unsigned i = 0; i < OSC_COUNT; i++) {
            oscs[i].outputs[0] = 0;
            oscs[i].outputs[1] = 0;
            oscs[i].chans[0] = 0;
            oscs[i].chans[1] = 0;
            oscs[i].chans[2] = 0;
        }
        set_volume();
        reset();
    }

    /// @brief Assign single oscillator output to buffer. If buffer is NULL,
    /// silences the given oscillator.
    ///
    /// @param channel the index of the oscillator to set the output for
    /// @param buffer the BLIPBuffer to output the given voice to
    /// @returns 0 if the output was set successfully, 1 if the index is invalid
    ///
    inline void set_output(unsigned channel, BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right) {
        if (channel >= OSC_COUNT)  // make sure the channel is within bounds
            throw ChannelOutOfBoundsException(channel, OSC_COUNT);
        oscs[channel].chans[0] = center;
        oscs[channel].chans[1] = left;
        oscs[channel].chans[2] = right;
        for (unsigned i = 0; i < OSC_COUNT; i++) balance_changed(oscs[i]);
    }

    /// @brief Assign all oscillator outputs to specified buffer. If buffer
    /// is NULL, silences all oscillators.
    ///
    /// @param buffer the single buffer to output the all the voices to
    ///
    inline void set_output(BLIPBuffer* buffer) {
        for (unsigned i = 0; i < OSC_COUNT; i++)
            set_output(i, buffer, buffer, buffer);
    }

    /// @brief Set the volume level of all oscillators.
    ///
    /// @param level the value to set the volume level to, where \f$1.0\f$ is
    /// full volume. Can be overdriven past \f$1.0\f$.
    ///
    inline void set_volume(double level = 1.0) {
        synth.volume(1.8 / OSC_COUNT / Oscillator::AMP_RANGE * level);
    }

    /// @brief Set treble equalization for the synthesizers.
    ///
    /// @param equalizer the equalization parameter for the synthesizers
    ///
    inline void set_treble_eq(BLIPEqualizer const& equalizer) {
        synth.treble_eq(equalizer);
    }

    /// @brief Reset internal state, registers, and all oscillators.
    inline void reset() {
        latch = 0;
        balance = 0xFF;

        Oscillator* osc = &oscs[OSC_COUNT];
        do {
            osc--;
            memset(osc, 0, offsetof(Oscillator, outputs));
            osc->noise_lfsr = 1;
            osc->control = 0x40;
            osc->balance = 0xFF;
        } while (osc != oscs);
    }

    /// @brief Write to the data port.
    ///
    /// @param addr the address to write the data to
    /// @param data the byte to write to the data port
    ///
    void write(uint16_t addr, uint8_t data) {
        // make sure the given address is legal
        if (addr < ADDR_START or addr > ADDR_END)
            throw AddressSpaceException<uint16_t>(addr, ADDR_START, ADDR_END);
        static constexpr auto time = 0;
        if (addr == 0x800) {
            latch = data & 7;
        } else if (addr == 0x801) {
            if (balance != data) {
                balance = data;
                for (unsigned i = 0; i < OSC_COUNT; i++) {
                    oscs[i].run_until(synth, time);
                    balance_changed(*oscs);
                }
            }
        } else if (latch < OSC_COUNT) {
            Oscillator& osc = oscs[latch];
            osc.run_until(synth, time);
            switch (addr) {
            case 0x802:
                osc.period = (osc.period & 0xF00) | data;
                break;
            case 0x803:
                osc.period = (osc.period & 0x0FF) | ((data & 0x0F) << 8);
                break;
            case 0x804:
                if (osc.control & 0x40 & ~data)
                    osc.phase = 0;
                osc.control = data;
                balance_changed(osc);
                break;
            case 0x805:
                osc.balance = data;
                balance_changed(osc);
                break;
            case 0x806:
                data &= 0x1F;
                if (!(osc.control & 0x40)) {
                    osc.wave[osc.phase] = data;
                    osc.phase = (osc.phase + 1) & 0x1F;
                } else if (osc.control & 0x80) {
                    osc.dac = data;
                }
                break;
             case 0x807:
                if (&osc >= &oscs[4])
                    osc.noise = data;
                break;
            }
        }
    }

    /// @brief Run all oscillators up to specified time, end current frame,
    /// then start a new frame at time 0.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    inline void end_frame(blip_time_t end_time) {
        for (unsigned channel = 0; channel < OSC_COUNT; channel++) {
            oscs[channel].run_until(synth, end_time);
            oscs[channel].last_time -= end_time;
        }
    }
};

#endif  // DSP_NEC_TURBO_GRAFX_16_HPP_
