// General Instrument AY-3-8910 sound chip emulator.
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

#ifndef DSP_GENERAL_INSTRUMENT_AY_3_8910_HPP_
#define DSP_GENERAL_INSTRUMENT_AY_3_8910_HPP_

#include "blargg_common.h"
#include "blip_buffer.hpp"

/// @brief General Instrument AY-3-8910 sound chip emulator.
/// @details
/// Emulation inaccuracies:
/// -   Noise isn't run when not in use
/// -   Changes to envelope and noise periods are delayed until next reload
/// -   Super-sonic tone should attenuate output to about 60%, not 50%
///
class GeneralInstrumentAy_3_8910 {
 public:
    /// the number of oscillators on the chip
    enum { OSC_COUNT = 3 };
    /// the number of registers on the chip
    enum { REG_COUNT = 16 };
    /// the range of the amplifier on the chip
    enum { AMP_RANGE = 255 };

 private:
    /// TODO:
    static constexpr int PERIOD_FACTOR = 16;
    /// Tones above this frequency are treated as disabled tone at half volume.
    /// Power of two is more efficient (avoids division).
    static constexpr unsigned INAUDIBLE_FREQ = 16384;

    /// With channels tied together and 1K resistor to ground (as datasheet
    /// recommends), output nearly matches logarithmic curve as claimed. Approx.
    /// 1.5 dB per step.
    static constexpr uint8_t AMP_TABLE[16] = {
    #define ENTRY(n) uint8_t (n * GeneralInstrumentAy_3_8910::AMP_RANGE + 0.5)
        ENTRY(0.000000), ENTRY(0.007813), ENTRY(0.011049), ENTRY(0.015625),
        ENTRY(0.022097), ENTRY(0.031250), ENTRY(0.044194), ENTRY(0.062500),
        ENTRY(0.088388), ENTRY(0.125000), ENTRY(0.176777), ENTRY(0.250000),
        ENTRY(0.353553), ENTRY(0.500000), ENTRY(0.707107), ENTRY(1.000000),
    #undef ENTRY
    };

    /// TODO:
    static constexpr uint8_t MODES[8] = {
    #define MODE(a0,a1, b0,b1, c0,c1) (a0 | a1<<1 | b0<<2 | b1<<3 | c0<<4 | c1<<5)
        MODE(1,0, 1,0, 1,0),
        MODE(1,0, 0,0, 0,0),
        MODE(1,0, 0,1, 1,0),
        MODE(1,0, 1,1, 1,1),
        MODE(0,1, 0,1, 0,1),
        MODE(0,1, 1,1, 1,1),
        MODE(0,1, 1,0, 0,1),
        MODE(0,1, 0,0, 0,0),
    #undef MODE
    };

    /// the noise off flag bit
    static constexpr int NOISE_OFF = 0x08;
    /// the tone off flag bit
    static constexpr int TONE_OFF  = 0x01;

    /// the oscillator type on the chip for the 5 channels
    struct osc_t {
        /// the period of the oscillator
        blip_time_t period;
        /// TODO:
        blip_time_t delay;
        /// the value of the last output from the oscillator
        short last_amp;
        /// the current phase of the oscillator
        short phase;
        /// the buffer the oscillator writes samples to
        BLIPBuffer* output;
    } oscs[OSC_COUNT];
    /// the synthesizer shared by the 5 oscillator channels
    BLIPSynth<blip_good_quality, 1> synth;
    /// the last time the oscillators were updated
    blip_time_t last_time;
    /// the registers on teh chip
    uint8_t regs[REG_COUNT];

    /// TODO:
    struct {
        /// TODO:
        blip_time_t delay;
        /// TODO:
        blargg_ulong lfsr;
    } noise;

    /// TODO:
    struct {
        /// TODO:
        blip_time_t delay;
        /// TODO:
        uint8_t const* wave;
        /// TODO:
        int pos;
        /// values already passed through volume table
        uint8_t modes[8][48];
    } env;

    /// Write to the data port.
    ///
    /// @param addr the address to write the data to
    /// @param data the data to write to the given address
    ///
    void write_data_(int addr, int data);

    /// Run the oscillators until the given end time.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    void run_until(blip_time_t);

 public:
    /// Initialize a new General Instrument AY-3-8910.
    GeneralInstrumentAy_3_8910();

    /// Set overall volume of all oscillators, where 1.0 is full volume
    ///
    /// @param level the value to set the volume to
    ///
    inline void volume(double v) {
        synth.volume(0.7 / OSC_COUNT / AMP_RANGE * v);
    }

    /// Set treble equalization for the synthesizers.
    ///
    /// @param equalizer the equalization parameter for the synthesizers
    ///
    inline void treble_eq(blip_eq_t const& eq) {
        synth.treble_eq(eq);
    }

    /// Assign single oscillator output to buffer. If buffer is NULL, silences
    /// the given oscillator.
    ///
    /// @param index the index of the oscillator to set the output for
    /// @param buffer the BLIPBuffer to output the given voice to
    /// @returns 0 if the output was set successfully, 1 if the index is invalid
    ///
    inline void set_output(int index, BLIPBuffer* buffer) {
        assert((unsigned) index < OSC_COUNT);
        oscs[index].output = buffer;
    }

    /// Assign all oscillator outputs to specified buffer. If buffer
    /// is NULL, silences all oscillators.
    ///
    /// @param buffer the BLIPBuffer to output the all the voices to
    ///
    inline void set_output(BLIPBuffer* buffer) {
        for (int i = 0; i < OSC_COUNT; i++) set_output(i, buffer);
    }

    /// Reset oscillators and internal state.
    inline void reset() {
        last_time   = 0;
        noise.delay = 0;
        noise.lfsr  = 1;

        osc_t* osc = &oscs[OSC_COUNT];
        do {
            osc--;
            osc->period   = PERIOD_FACTOR;
            osc->delay    = 0;
            osc->last_amp = 0;
            osc->phase    = 0;
        } while (osc != oscs);

        for (int i = sizeof regs; --i >= 0;) regs[i] = 0;
        regs[7] = 0xFF;
        write_data_(13, 0);
    }

    /// Write to the data port.
    ///
    /// @param data the byte to write to the data port
    ///
    inline void write(int addr, int data) {
        run_until(0);
        write_data_(addr, data);
    }

    /// Run all oscillators up to specified time, end current frame, then
    /// start a new frame at time 0.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    inline void end_frame(blip_time_t time) {
        if (time > last_time) run_until(time);
        assert(last_time >= time);
        last_time -= time;
    }
};

#endif  // DSP_GENERAL_INSTRUMENT_AY_3_8910_HPP_
