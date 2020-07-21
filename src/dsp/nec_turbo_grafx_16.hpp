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

struct NECTurboGrafx16_Oscillator {
    unsigned char wave[32];
    short volume[2];
    int last_amp[2];
    int delay;
    int period;
    unsigned char noise;
    unsigned char phase;
    unsigned char balance;
    unsigned char dac;
    blip_time_t last_time;

    BLIPBuffer* outputs[2];
    BLIPBuffer* chans[3];
    unsigned noise_lfsr;
    unsigned char control;

    enum { amp_range = 0x8000 };
    typedef BLIPSynth<blip_med_quality,1> synth_t;

    void run_until(synth_t& synth, blip_time_t);
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

    /// Initialize a new Turbo Grafx 16 chip.
    NECTurboGrafx16();

    /// Set overall volume of all oscillators, where 1.0 is full volume
    ///
    /// @param level the value to set the volume to
    ///
    inline void volume(double level) {
        synth.volume(1.8 / OSC_COUNT / NECTurboGrafx16_Oscillator::amp_range * level);
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
    void osc_output(int index, BLIPBuffer* center, BLIPBuffer* left, BLIPBuffer* right);

    /// Assign all oscillator outputs to specified buffer. If buffer
    /// is NULL, silences all oscillators.
    ///
    /// @param buffer the BLIPBuffer to output the all the voices to
    ///
    // inline void set_output(BLIPBuffer* buffer) {
    //     for (int i = 0; i < OSC_COUNT; i++) set_output(i, buffer);
    // }

    /// Reset oscillators and internal state.
    void reset();

    /// Write to the data port.
    ///
    /// @param data the byte to write to the data port
    ///
    void write_data(int addr, int data);

    /// Run all oscillators up to specified time, end current frame, then
    /// start a new frame at time 0.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    void end_frame(blip_time_t);

 private:
    /// TODO:
    NECTurboGrafx16_Oscillator oscs[OSC_COUNT];
    /// TODO:
    int latch;
    /// TODO:
    int balance;
    /// TODO:
    NECTurboGrafx16_Oscillator::synth_t synth;

    /// TODO:
    void balance_changed(NECTurboGrafx16_Oscillator&);

    /// TODO:
    void recalc_chans();
};

#endif  // DSP_NEC_TURBO_GRAFX_16_HPP_
