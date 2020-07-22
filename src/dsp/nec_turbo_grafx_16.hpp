// Turbo Grafx 16 (PC Engine) PSG sound chip emulator.
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

#include "blargg_common.h"
#include "blip_buffer.hpp"
#include <cstring>

/// Turbo Grafx 16 (PC Engine) PSG sound chip emulator.
struct NECTurboGrafx16_Oscillator {
    /// TODO:
    unsigned char wave[32];
    /// TODO:
    short volume[2];
    /// TODO:
    int last_amp[2];
    /// TODO:
    int delay;
    /// TODO:
    int period;
    /// TODO:
    unsigned char noise;
    /// TODO:
    unsigned char phase;
    /// TODO:
    unsigned char balance;
    /// TODO:
    unsigned char dac;
    /// TODO:
    blip_time_t last_time;

    /// TODO:
    BLIPBuffer* outputs[2];
    /// TODO:
    BLIPBuffer* chans[3];
    /// TODO:
    unsigned noise_lfsr;
    /// TODO:
    unsigned char control;

    /// TODO:
    enum { AMP_RANGE = 0x8000 };
    /// TODO:
    typedef BLIPSynth<blip_med_quality,1> synth_t;

    /// TODO:
    void run_until(synth_t& synth_, blip_time_t end_time) {
        BLIPBuffer* const osc_outputs_0 = outputs[0]; // cache often-used values
        if (osc_outputs_0 && control & 0x80) {
            int dac = this->dac;

            int const volume_0 = volume[0];
            {
                int delta = dac * volume_0 - last_amp[0];
                if (delta) synth_.offset(last_time, delta, osc_outputs_0);
            }

            BLIPBuffer* const osc_outputs_1 = outputs[1];
            int const volume_1 = volume[1];
            if (osc_outputs_1) {
                int delta = dac * volume_1 - last_amp[1];
                if (delta) synth_.offset(last_time, delta, osc_outputs_1);
            }

            blip_time_t time = last_time + delay;
            if (time < end_time) {
                if (noise & 0x80) {
                    if (volume_0 | volume_1) {
                        // noise
                        int const period = (32 - (noise & 0x1F)) * 64; // TODO: correct?
                        unsigned noise_lfsr = this->noise_lfsr;
                        do {
                            int new_dac = 0x1F & -(noise_lfsr >> 1 & 1);
                            // Implemented using "Galios configuration"
                            // TODO: find correct LFSR algorithm
                            noise_lfsr = (noise_lfsr >> 1) ^ (0xE008 & -(noise_lfsr & 1));
                            //noise_lfsr = (noise_lfsr >> 1) ^ (0x6000 & -(noise_lfsr & 1));
                            int delta = new_dac - dac;
                            if (delta) {
                                dac = new_dac;
                                synth_.offset(time, delta * volume_0, osc_outputs_0);
                                if (osc_outputs_1)
                                    synth_.offset(time, delta * volume_1, osc_outputs_1);
                            }
                            time += period;
                        } while (time < end_time);

                        this->noise_lfsr = noise_lfsr;
                        assert(noise_lfsr);
                    }
                }
                else if (!(control & 0x40)) {
                    // wave
                    int phase = (this->phase + 1) & 0x1F; // pre-advance for optimal inner loop
                    int period = this->period * 2;
                    if (period >= 14 && (volume_0 | volume_1)) {
                        do {
                            int new_dac = wave[phase];
                            phase = (phase + 1) & 0x1F;
                            int delta = new_dac - dac;
                            if (delta) {
                                dac = new_dac;
                                synth_.offset(time, delta * volume_0, osc_outputs_0);
                                if (osc_outputs_1)
                                    synth_.offset(time, delta * volume_1, osc_outputs_1);
                            }
                            time += period;
                        } while (time < end_time);
                    } else {
                        if (!period) {
                            // TODO: Gekisha Boy assumes that period = 0 silences wave
                            //period = 0x1000 * 2;
                            period = 1;
                            //if (!(volume_0 | volume_1))
                            //  dprintf("Used period 0\n");
                        }
                        // maintain phase when silent
                        blargg_long count = (end_time - time + period - 1) / period;
                        phase += count; // phase will be masked below
                        time += count * period;
                    }
                    this->phase = (phase - 1) & 0x1F; // undo pre-advance
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

/// Turbo Grafx 16 (PC Engine) PSG sound chip emulator.
class NECTurboGrafx16 {
 public:
    /// TODO:
    static constexpr int OSC_COUNT = 6;
    /// TODO:
    static constexpr int ADDR_START = 0x0800;
    /// TODO:
    static constexpr int ADDR_END   = 0x0809;

 private:
    /// reduces asymmetry and clamping when starting notes
    static constexpr bool CENTER_WAVES = true;

    /// TODO:
    NECTurboGrafx16_Oscillator oscs[OSC_COUNT];
    /// TODO:
    int latch;
    /// TODO:
    int balance;
    /// TODO:
    NECTurboGrafx16_Oscillator::synth_t synth;

    /// TODO:
    void balance_changed(NECTurboGrafx16_Oscillator& osc) {
        static const short log_table[32] = {  // ~1.5 db per step
            #define ENTRY(factor) short (factor * NECTurboGrafx16_Oscillator::AMP_RANGE / 31.0 + 0.5)
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
        NECTurboGrafx16_Oscillator* osc = &oscs[OSC_COUNT];
        do {
            osc--;
            osc->outputs[0] = 0;
            osc->outputs[1] = 0;
            osc->chans[0] = 0;
            osc->chans[1] = 0;
            osc->chans[2] = 0;
        } while (osc != oscs);
        reset();
    }

    /// Set overall volume of all oscillators, where 1.0 is full volume
    ///
    /// @param level the value to set the volume to
    ///
    inline void set_volume(double level) {
        synth.volume(1.8 / OSC_COUNT / NECTurboGrafx16_Oscillator::AMP_RANGE * level);
    }

    /// Set treble equalization for the synthesizers.
    ///
    /// @param equalizer the equalization parameter for the synthesizers
    ///
    inline void treble_eq(blip_eq_t const& equalizer) {
        synth.treble_eq(equalizer);
    }

    /// Assign single oscillator output to buffer. If buffer is NULL, silences
    /// the given oscillator.
    ///
    /// @param index the index of the oscillator to set the output for
    /// @param buffer the BLIPBuffer to output the given voice to
    /// @returns 0 if the output was set successfully, 1 if the index is invalid
    ///
    inline void set_output(int index, BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right) {
        assert((unsigned) index < OSC_COUNT);
        oscs[index].chans[0] = center;
        oscs[index].chans[1] = left;
        oscs[index].chans[2] = right;
        NECTurboGrafx16_Oscillator* osc = &oscs[OSC_COUNT];
        do {
            osc--;
            balance_changed(*osc);
        } while (osc != oscs);
    }

    /// Assign all oscillator outputs to specified buffer. If buffer
    /// is NULL, silences all oscillators.
    ///
    /// @param buffer the BLIPBuffer to output the all the voices to
    ///
    // inline void set_output(BLIPBuffer* buffer) {
    //     for (int i = 0; i < OSC_COUNT; i++) set_output(i, buffer);
    // }

    /// Reset oscillators and internal state.
    inline void reset() {
        latch = 0;
        balance = 0xFF;

        NECTurboGrafx16_Oscillator* osc = &oscs[OSC_COUNT];
        do {
            osc--;
            memset(osc, 0, offsetof(NECTurboGrafx16_Oscillator, outputs));
            osc->noise_lfsr = 1;
            osc->control = 0x40;
            osc->balance = 0xFF;
        } while (osc != oscs);
    }

    /// Write to the data port.
    ///
    /// @param addr the address to write the data to
    /// @param data the byte to write to the data port
    ///
    void write(int addr, int data) {
        static constexpr auto time = 0;
        if (addr == 0x800) {
            latch = data & 7;
        } else if (addr == 0x801) {
            if (balance != data) {
                balance = data;
                NECTurboGrafx16_Oscillator* osc = &oscs[OSC_COUNT];
                do {
                    osc--;
                    osc->run_until(synth, time);
                    balance_changed(*oscs);
                } while (osc != oscs);
            }
        } else if (latch < OSC_COUNT) {
            NECTurboGrafx16_Oscillator& osc = oscs[latch];
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

    /// Run all oscillators up to specified time, end current frame, then
    /// start a new frame at time 0.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    inline void end_frame(blip_time_t end_time) {
        NECTurboGrafx16_Oscillator* osc = &oscs[OSC_COUNT];
        do {
            osc--;
            if (end_time > osc->last_time) osc->run_until(synth, end_time);
            assert(osc->last_time >= end_time);
            osc->last_time -= end_time;
        } while (osc != oscs);
    }
};

#endif  // DSP_NEC_TURBO_GRAFX_16_HPP_
