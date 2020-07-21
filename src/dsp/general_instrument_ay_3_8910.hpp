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

/// General Instrument AY-3-8910 sound chip emulator.
class GeneralInstrumentAy_3_8910 {
 public:
    /// the number of oscillators on the chip
    enum { OSC_COUNT = 3 };
    /// the number of registers on the chip
    enum { REG_COUNT = 16 };
    /// TODO:
    enum { AMP_RANGE = 255 };
    /// TODO:
    BLIPSynth<blip_good_quality, 1> synth_;

 private:
    /// TODO:
    struct osc_t {
        /// TODO:
        blip_time_t period;
        /// TODO:
        blip_time_t delay;
        /// TODO:
        short last_amp;
        /// TODO:
        short phase;
        /// TODO:
        BLIPBuffer* output;
    } oscs[OSC_COUNT];
    /// TODO:
    blip_time_t last_time;
    /// TODO:
    uint8_t regs[REG_COUNT];

    /// TODO:
    struct {
        blip_time_t delay;
        blargg_ulong lfsr;
    } noise;

    /// TODO:
    struct {
        blip_time_t delay;
        uint8_t const* wave;
        int pos;
        uint8_t modes[8][48]; // values already passed through volume table
    } env;

    /// TODO:
    void write_data_(int addr, int data);

    /// TODO:
    void run_until(blip_time_t);

 public:
    GeneralInstrumentAy_3_8910();

    // Set overall volume (default is 1.0)
    inline void volume(double v) { synth_.volume(0.7 / OSC_COUNT / AMP_RANGE * v); }

    // Set treble equalization (see documentation)
    inline void treble_eq(blip_eq_t const& eq) { synth_.treble_eq(eq); }

    // Set sound output of specific oscillator to buffer, where index is
    // 0, 1, or 2. If buffer is NULL, the specified oscillator is muted.
    inline void osc_output(int i, BLIPBuffer* buf) {
        assert((unsigned) i < OSC_COUNT);
        oscs[i].output = buf;
    }

    // Set buffer to generate all sound into, or disable sound if NULL
    inline void output(BLIPBuffer* buf) {
        osc_output(0, buf);
        osc_output(1, buf);
        osc_output(2, buf);
    }

    // Reset sound chip
    void reset();

    // Write to register at specified time
    inline void write(blip_time_t time, int addr, int data) {
        run_until(time);
        write_data_(addr, data);
    }

    // Run sound to specified time, end current time frame, then start a new
    // time frame at time 0. Time frames have no effect on emulation and each
    // can be whatever length is convenient.
    inline void end_frame(blip_time_t time) {
        if (time > last_time)
            run_until(time);

        assert(last_time >= time);
        last_time -= time;
    }
};

#endif  // DSP_GENERAL_INSTRUMENT_AY_3_8910_HPP_
