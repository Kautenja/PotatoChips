// Atari POKEY sound chip emulator
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

#ifndef DSP_ATARI_POKEY_HPP_
#define DSP_ATARI_POKEY_HPP_

#include "blargg_common.h"
#include "blip_buffer.hpp"
#include <cstring>

// TODO: remove blarg_ulong

static void gen_poly(blargg_ulong mask, int count, uint8_t* out) {
    blargg_ulong n = 1;
    do {
        int bits = 0;
        int b = 0;
        do {  // implemented using "Galios configuration"
            bits |= (n & 1) << b;
            n = (n >> 1) ^ (mask & -(n & 1));
        } while (b++ < 7);
        *out++ = bits;
    } while (--count);
}

// poly5
int const poly5_len = (1 <<  5) - 1;
blargg_ulong const poly5_mask = (1UL << poly5_len) - 1;
blargg_ulong const poly5 = 0x167C6EA1;

inline blargg_ulong run_poly5(blargg_ulong in, int shift) {
    return (in << shift & poly5_mask) | (in >> (poly5_len - shift));
}

#define POLY_MASK(width, tap1, tap2) \
    ((1UL << (width - 1 - tap1)) | (1UL << (width - 1 - tap2)))

class AtariPOKEYEngine;

class AtariPOKEY {
 public:
    /// the number of oscillators on the chip
    enum { OSC_COUNT = 4 };
    /// the start address of the RAM on the chip
    enum { ADDR_START = 0xD200 };
    /// the end address of the RAM on the chip
    enum { ADDR_END   = 0xD209 };

 public:
    /// Initialize a new Atari POKEY chip emulator.
    AtariPOKEY() : impl(0) { set_output(0); }

    /// TODO:
    inline void osc_output(int index, BLIPBuffer* buffer) {
        assert((unsigned) index < OSC_COUNT);
        oscs[index].output = buffer;
    }

    /// Assign all oscillator outputs to specified buffer. If buffer
    /// is NULL, silences all oscillators.
    ///
    /// @param buffer the BLIPBuffer to output the all the voices to
    ///
    inline void set_output(BLIPBuffer* buffer) {
        for (int i = 0; i < OSC_COUNT; i++) osc_output(i, buffer);
    }

    /// TODO:
    inline void reset(AtariPOKEYEngine* new_impl) {
        impl = new_impl;
        last_time = 0;
        poly5_pos = 0;
        poly4_pos = 0;
        polym_pos = 0;
        control = 0;
        for (int i = 0; i < OSC_COUNT; i++)
            memset(&oscs[i], 0, offsetof(osc_t, output));
    }

    /// TODO:
    inline void write_data(unsigned addr, int data) {
        run_until(0);
        int i = (addr ^ 0xD200) >> 1;
        if (i < OSC_COUNT) {
            oscs[i].regs[addr & 1] = data;
        } else if (addr == 0xD208) {
            control = data;
        } else if (addr == 0xD209) {
            oscs[0].delay = 0;
            oscs[1].delay = 0;
            oscs[2].delay = 0;
            oscs[3].delay = 0;
        }
        /*
        // TODO: are polynomials reset in this case?
        else if (addr == 0xD20F) {
            if ((data & 3) == 0)
                polym_pos = 0;
        }
        */
    }

    /// TODO:
    inline void end_frame(blip_time_t end_time) {
        if (end_time > last_time) run_until(end_time);
        last_time -= end_time;
    }

 private:
    /// pure waves above this frequency are silenced
    static constexpr int MAX_FREQUENCY = 12000;

    /// TODO:
    struct osc_t {
        /// TODO:
        unsigned char regs[2];
        /// TODO:
        unsigned char phase;
        /// TODO:
        unsigned char invert;
        /// TODO:
        int last_amp;
        /// TODO:
        blip_time_t delay;
        /// always recalculated before use; here for convenience
        blip_time_t period;
        /// TODO:
        BLIPBuffer* output;
    };
    /// TODO:
    osc_t oscs[OSC_COUNT];
    /// TODO:
    AtariPOKEYEngine* impl;
    /// TODO:
    blip_time_t last_time;
    /// TODO:
    int poly5_pos;
    /// TODO:
    int poly4_pos;
    /// TODO:
    int polym_pos;
    /// TODO:
    int control;

    /// TODO:
    inline void calc_periods() {
         // 15/64 kHz clock
        int divider = 28;
        if (this->control & 1)
            divider = 114;

        for (int i = 0; i < OSC_COUNT; i++) {
            osc_t* const osc = &oscs[i];
            // cache
            int const osc_reload = osc->regs[0];
            blargg_long period = (osc_reload + 1) * divider;
            static uint8_t const fast_bits[OSC_COUNT] = { 1 << 6, 1 << 4, 1 << 5, 1 << 3 };
            if (this->control & fast_bits[i]) {
                period = osc_reload + 4;
                if (i & 1) {
                    period = osc_reload * 0x100L + osc[-1].regs[0] + 7;
                    if (!(this->control & fast_bits[i - 1]))
                        period = (period - 6) * divider;
                }
            }
            osc->period = period;
        }
    }

    /// TODO:
    void run_until(blip_time_t);

    /// TODO:
    enum { poly4_len  = (1L <<  4) - 1 };
    /// TODO:
    enum { poly9_len  = (1L <<  9) - 1 };
    /// TODO:
    enum { poly17_len = (1L << 17) - 1 };

    /// TODO:
    friend class AtariPOKEYEngine;
};

/// Common tables and BLIPSynth that can be shared among AtariPOKEY objects.
class AtariPOKEYEngine {
 public:
    /// TODO:
    BLIPSynth<blip_good_quality, 1> synth;

    /// TODO:
    AtariPOKEYEngine() {
        gen_poly(POLY_MASK( 4, 1, 0), sizeof poly4,  poly4 );
        gen_poly(POLY_MASK( 9, 5, 0), sizeof poly9,  poly9 );
        gen_poly(POLY_MASK(17, 5, 0), sizeof poly17, poly17);

        if (0) {  // comment out to recalculate poly5 constant
            uint8_t poly5[4];
            gen_poly(POLY_MASK( 5, 2, 0), sizeof poly5,  poly5 );
            blargg_ulong n = poly5[3] * 0x1000000L + poly5[2] * 0x10000L +
                    poly5[1] * 0x100L + poly5[0];
            blargg_ulong rev = n & 1;
            for (int i = 1; i < poly5_len; i++)
                rev |= (n >> i & 1) << (poly5_len - i);
        }
    }

    /// TODO:
    inline void volume(double d) {
        synth.volume(1.0 / AtariPOKEY::OSC_COUNT / 30 * d);
    }

 private:
    /// TODO:
    uint8_t poly4[AtariPOKEY::poly4_len  / 8 + 1];
    /// TODO:
    uint8_t poly9[AtariPOKEY::poly9_len  / 8 + 1];
    /// TODO:
    uint8_t poly17[AtariPOKEY::poly17_len / 8 + 1];

    /// TODO:
    friend class AtariPOKEY;
};

#endif  // DSP_ATARI_POKEY_HPP_
