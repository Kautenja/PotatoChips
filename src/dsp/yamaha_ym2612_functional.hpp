// YM2612 FM sound chip emulator interface
// Copyright 2020 Christian Kauten
// Copyright 2006 Shay Green
// Copyright 2001 Jarek Burczynski
// Copyright 1998 Tatsuyuki Satoh
// Copyright 1997 Nicola Salmoria and the MAME team
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
// Version 1.4 (final beta)
//

#ifndef DSP_YAMAHA_YM2612_FUNCTIONAL_HPP_
#define DSP_YAMAHA_YM2612_FUNCTIONAL_HPP_

#include "yamaha_ym2612_operators.hpp"
#include "exceptions.hpp"

/// @brief Emulator common state.
struct EngineState {
    /// frequency base
    float freqbase = 0;

    /// general state
    GlobalOperatorState state;
    /// pointer to voices
    Voice* voices = nullptr;

    /// global envelope generator counter
    uint32_t eg_cnt = 0;
    /// global envelope generator counter works at frequency = chipclock/144/3
    uint32_t eg_timer = 0;
    /// step of eg_timer
    uint32_t eg_timer_add = 0;
    /// envelope generator timer overflows every 3 samples (on real chip)
    uint32_t eg_timer_overflow = 0;

    /// there are 2048 FNUMs that can be generated using FNUM/BLK registers
    /// but LFO works with one more bit of a precision so we really need 4096
    /// elements. fnumber->increment counter
    uint32_t fn_table[4096];
    /// maximal phase increment (used for phase overflow)
    uint32_t fn_max = 0;

    /// current LFO phase (out of 128)
    uint8_t lfo_cnt = 0;
    /// current LFO phase runs at LFO frequency
    uint32_t lfo_timer = 0;
    /// step of lfo_timer
    uint32_t lfo_timer_add = 0;
    /// LFO timer overflows every N samples (depends on LFO frequency)
    uint32_t lfo_timer_overflow = 0;
    /// current LFO AM step
    uint32_t lfo_AM_step = 0;
    /// current LFO PM step
    uint32_t lfo_PM_step = 0;

    /// Phase Modulation input for operator 2
    int32_t m2 = 0;
    /// Phase Modulation input for operator 3
    int32_t c1 = 0;
    /// Phase Modulation input for operator 4
    int32_t c2 = 0;

    /// one sample delay memory
    int32_t mem = 0;
    /// outputs of working channels
    int32_t out_fm[8];

    // TODO: make private
    // TODO: inline in set_sample_rate?
    /// @brief Initialize time tables.
    void init_timetables() {
        // DeTune table
        for (int d = 0; d <= 3; d++) {
            for (int i = 0; i <= 31; i++) {
                // -10 because chip works with 10.10 fixed point, while we use 16.16
                float rate = ((float) DT_TABLE[d * 32 + i]) * freqbase * (1 << (FREQ_SH - 10));
                state.dt_table[d][i] = (int32_t) rate;
                state.dt_table[d + 4][i] = -state.dt_table[d][i];
            }
        }
        // there are 2048 FNUMs that can be generated using FNUM/BLK registers
        // but LFO works with one more bit of a precision so we really need 4096
        // elements. calculate fnumber -> increment counter table
        for (int i = 0; i < 4096; i++) {
            // freq table for octave 7
            // phase increment counter = 20bit
            // the correct formula is
            //     F-Number = (144 * fnote * 2^20 / M) / 2^(B-1)
            // where sample clock is: M / 144
            // this means the increment value for one clock sample is
            //     FNUM * 2^(B-1) = FNUM * 64
            // for octave 7
            // we also need to handle the ratio between the chip frequency and
            // the emulated frequency (can be 1.0)
            // NOTE:
            // -10 because chip works with 10.10 fixed point, while we use 16.16
            fn_table[i] = (uint32_t)((float) i * 32 * freqbase * (1 << (FREQ_SH - 10)));
        }
        // maximal frequency is required for Phase overflow calculation, register
        // size is 17 bits (Nemesis)
        fn_max = (uint32_t)((float) 0x20000 * freqbase * (1 << (FREQ_SH - 10)));
    }

    /// @brief Set the output sample rate and clock rate.
    ///
    /// @param sample_rate the number of samples per second
    /// @param clock_rate the number of source clock cycles per second
    ///
    void set_sample_rate(float sample_rate, float clock_rate) {
        if (sample_rate == 0) throw Exception("sample_rate must be above 0");
        if (clock_rate == 0) throw Exception("clock_rate must be above 0");
        // frequency base
        freqbase = clock_rate / sample_rate;
        // TODO: why is it necessary to scale these increments by a factor of 1/16
        //       to get the correct timings from the EG and LFO?
        // EG timer increment (updates every 3 samples)
        eg_timer_add = (1 << EG_SH) * freqbase / 16;
        eg_timer_overflow = 3 * (1 << EG_SH) / 16;
        // LFO timer increment (updates every 16 samples)
        lfo_timer_add = (1 << LFO_SH) * freqbase / 16;
        // make time tables
        init_timetables();
    }

    // TODO: enum for the operator?
    // TODO: better ASCII illustrations of the operators
    /// @brief Set algorithm, i.e., operator routing.
    ///
    /// @param voice_idx the index of the voice to set the routing of
    /// @param algorithm the algorithm to set the voice to
    ///
    inline void set_algorithm(unsigned voice_idx, uint8_t algorithm) {
        // get the voice and carrier wave
        Voice* const voice = &voices[voice_idx];
        voice->algorithm = algorithm & 7;
        int32_t *carrier = &out_fm[voice_idx];
        // get the connections
        int32_t **om1 = &voice->connect1;
        int32_t **om2 = &voice->connect3;
        int32_t **oc1 = &voice->connect2;
        int32_t **memc = &voice->mem_connect;
        // set the algorithm
        switch (voice->algorithm) {
        case 0:
            // M1---C1---MEM---M2---C2---OUT
            *om1  = &c1;
            *oc1  = &mem;
            *om2  = &c2;
            *memc = &m2;
            break;
        case 1:
            // M1------+-MEM---M2---C2---OUT
            //      C1-+
            *om1  = &mem;
            *oc1  = &mem;
            *om2  = &c2;
            *memc = &m2;
            break;
        case 2:
            // M1-----------------+-C2---OUT
            //      C1---MEM---M2-+
            *om1  = &c2;
            *oc1  = &mem;
            *om2  = &c2;
            *memc = &m2;
            break;
        case 3:
            // M1---C1---MEM------+-C2---OUT
            //                 M2-+
            *om1  = &c1;
            *oc1  = &mem;
            *om2  = &c2;
            *memc = &c2;
            break;
        case 4:
            // M1---C1-+-OUT
            // M2---C2-+
            // MEM: not used
            *om1  = &c1;
            *oc1  = carrier;
            *om2  = &c2;
            *memc = &mem;  // store it anywhere where it will not be used
            break;
        case 5:
            //    +----C1----+
            // M1-+-MEM---M2-+-OUT
            //    +----C2----+
            *om1  = nullptr;  // special mark
            *oc1  = carrier;
            *om2  = carrier;
            *memc = &m2;
            break;
        case 6:
            // M1---C1-+
            //      M2-+-OUT
            //      C2-+
            // MEM: not used
            *om1  = &c1;
            *oc1  = carrier;
            *om2  = carrier;
            *memc = &mem;  // store it anywhere where it will not be used
            break;
        case 7:
            // M1-+
            // C1-+-OUT
            // M2-+
            // C2-+
            // MEM: not used
            *om1  = carrier;
            *oc1  = carrier;
            *om2  = carrier;
            *memc = &mem;  // store it anywhere where it will not be used
            break;
        }
        voice->connect4 = carrier;
    }

    /// @brief Advance LFO to next sample.
    inline void advance_lfo() {
        if (lfo_timer_overflow) {  // LFO enabled
            // increment LFO timer
            lfo_timer += lfo_timer_add;
            // when LFO is enabled, one level will last for
            // 108, 77, 71, 67, 62, 44, 8 or 5 samples
            while (lfo_timer >= lfo_timer_overflow) {
                lfo_timer -= lfo_timer_overflow;
                // There are 128 LFO steps
                lfo_cnt = ( lfo_cnt + 1 ) & 127;
                // triangle (inverted)
                // AM: from 126 to 0 step -2, 0 to 126 step +2
                if (lfo_cnt<64)
                    lfo_AM_step = (lfo_cnt ^ 63) << 1;
                else
                    lfo_AM_step = (lfo_cnt & 63) << 1;
                // PM works with 4 times slower clock
                lfo_PM_step = lfo_cnt >> 2;
            }
        }
    }

    /// @brief Update phase increment and envelope generator
    inline void refresh_fc_eg_slot(Operator* oprtr, int fc, int kc) {
        int ksr = kc >> oprtr->KSR;
        fc += oprtr->DT[kc];
        // detects frequency overflow (credits to Nemesis)
        if (fc < 0) fc += fn_max;
        // (frequency) phase increment counter
        oprtr->phase_increment = (fc * oprtr->mul) >> 1;
        if ( oprtr->ksr != ksr ) {
            oprtr->ksr = ksr;
            // calculate envelope generator rates
            if ((oprtr->ar + oprtr->ksr) < 32+62) {
                oprtr->eg_sh_ar  = ENV_RATE_SHIFT[oprtr->ar  + oprtr->ksr];
                oprtr->eg_sel_ar = ENV_RATE_SELECT[oprtr->ar  + oprtr->ksr];
            } else {
                oprtr->eg_sh_ar  = 0;
                oprtr->eg_sel_ar = 17 * ENV_RATE_STEPS;
            }
            // set the shift
            oprtr->eg_sh_d1r = ENV_RATE_SHIFT[oprtr->d1r + oprtr->ksr];
            oprtr->eg_sh_d2r = ENV_RATE_SHIFT[oprtr->d2r + oprtr->ksr];
            oprtr->eg_sh_rr = ENV_RATE_SHIFT[oprtr->rr  + oprtr->ksr];
            // set the selector
            oprtr->eg_sel_d1r = ENV_RATE_SELECT[oprtr->d1r + oprtr->ksr];
            oprtr->eg_sel_d2r = ENV_RATE_SELECT[oprtr->d2r + oprtr->ksr];
            oprtr->eg_sel_rr = ENV_RATE_SELECT[oprtr->rr  + oprtr->ksr];
        }
    }

    /// @brief Update phase increment counters
    inline void refresh_fc_eg_chan(Voice* voice) {
        if (voice->operators[Op1].phase_increment == -1) {
            int fc = voice->fc;
            int kc = voice->kcode;
            refresh_fc_eg_slot(&voice->operators[Op1], fc, kc);
            refresh_fc_eg_slot(&voice->operators[Op2], fc, kc);
            refresh_fc_eg_slot(&voice->operators[Op3], fc, kc);
            refresh_fc_eg_slot(&voice->operators[Op4], fc, kc);
        }
    }

    inline void update_phase_lfo_channel(Voice* voice) {
        uint32_t block_fnum = voice->block_fnum;
        uint32_t fnum_lfo  = ((block_fnum & 0x7f0) >> 4) * 32 * 8;
        int32_t  lfo_fn_table_index_offset = LFO_PM_TABLE[fnum_lfo + voice->pms + lfo_PM_step];
        if (lfo_fn_table_index_offset) {  // LFO phase modulation active
            block_fnum = block_fnum * 2 + lfo_fn_table_index_offset;
            uint8_t blk = (block_fnum & 0x7000) >> 12;
            uint32_t fn = block_fnum & 0xfff;
            // key-scale code
            int kc = (blk << 2) | FREQUENCY_KEYCODE_TABLE[fn >> 8];
            // phase increment counter
            int fc = (fn_table[fn]>>(7 - blk));
            // detects frequency overflow (credits to Nemesis)
            int finc = fc + voice->operators[Op1].DT[kc];
            // Operator 1
            if (finc < 0) finc += fn_max;
            voice->operators[Op1].phase += (finc * voice->operators[Op1].mul) >> 1;
            // Operator 2
            finc = fc + voice->operators[Op2].DT[kc];
            if (finc < 0) finc += fn_max;
            voice->operators[Op2].phase += (finc * voice->operators[Op2].mul) >> 1;
            // Operator 3
            finc = fc + voice->operators[Op3].DT[kc];
            if (finc < 0) finc += fn_max;
            voice->operators[Op3].phase += (finc * voice->operators[Op3].mul) >> 1;
            // Operator 4
            finc = fc + voice->operators[Op4].DT[kc];
            if (finc < 0) finc += fn_max;
            voice->operators[Op4].phase += (finc * voice->operators[Op4].mul) >> 1;
        } else {  // LFO phase modulation is 0
            voice->operators[Op1].phase += voice->operators[Op1].phase_increment;
            voice->operators[Op2].phase += voice->operators[Op2].phase_increment;
            voice->operators[Op3].phase += voice->operators[Op3].phase_increment;
            voice->operators[Op4].phase += voice->operators[Op4].phase_increment;
        }
    }

    inline void chan_calc(Voice* voice) {
#define CALCULATE_VOLUME(OP) ((OP)->vol_out + (AM & (OP)->AMmask))
        uint32_t AM = lfo_AM_step >> voice->ams;
        m2 = c1 = c2 = mem = 0;
        // restore delayed sample (MEM) value to m2 or c2
        *voice->mem_connect = voice->mem_value;
        // Operator 1
        unsigned int eg_out = CALCULATE_VOLUME(&voice->operators[Op1]);
        int32_t out = voice->op1_out[0] + voice->op1_out[1];
        voice->op1_out[0] = voice->op1_out[1];
        if (!voice->connect1) {  // algorithm 5
            mem = c1 = c2 = voice->op1_out[0];
        } else {  // other algorithms
            *voice->connect1 += voice->op1_out[0];
        }
        voice->op1_out[1] = 0;
        if (eg_out < ENV_QUIET) {
            if (!voice->feedback) out = 0;
            voice->op1_out[1] = op_calc1(voice->operators[Op1].phase, eg_out, (out << voice->feedback) );
        }
        // Operator 3
        eg_out = CALCULATE_VOLUME(&voice->operators[Op3]);
        if (eg_out < ENV_QUIET)
            *voice->connect3 += op_calc(voice->operators[Op3].phase, eg_out, m2);
        // Operator 2
        eg_out = CALCULATE_VOLUME(&voice->operators[Op2]);
        if (eg_out < ENV_QUIET)
            *voice->connect2 += op_calc(voice->operators[Op2].phase, eg_out, c1);
        // Operator 4
        eg_out = CALCULATE_VOLUME(&voice->operators[Op4]);
        if (eg_out < ENV_QUIET)
            *voice->connect4 += op_calc(voice->operators[Op4].phase, eg_out, c2);
        // store current MEM
        voice->mem_value = mem;
        // update phase counters AFTER output calculations
        if (voice->pms) {
            update_phase_lfo_channel(voice);
        } else {  // no LFO phase modulation
            voice->operators[Op1].phase += voice->operators[Op1].phase_increment;
            voice->operators[Op2].phase += voice->operators[Op2].phase_increment;
            voice->operators[Op3].phase += voice->operators[Op3].phase_increment;
            voice->operators[Op4].phase += voice->operators[Op4].phase_increment;
        }
#undef CALCULATE_VOLUME
    }
};

#endif  // DSP_YAMAHA_YM2612_FUNCTIONAL_HPP_
