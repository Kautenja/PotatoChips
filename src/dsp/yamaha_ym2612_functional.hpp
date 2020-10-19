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

    /// @brief Initialize time tables.
    void init_timetables() {
        // DeTune table
        for (int d = 0; d <= 3; d++) {
            for (int i = 0; i <= 31; i++) {
                // -10 because chip works with 10.10 fixed point, while we use 16.16
                float rate = ((float) dt_tab[d * 32 + i]) * freqbase * (1 << (FREQ_SH - 10));
                state.dt_tab[d][i] = (int32_t) rate;
                state.dt_tab[d + 4][i] = -state.dt_tab[d][i];
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

    /// @brief Set pre-scaler and make time tables.
    ///
    /// @param engine the emulator to set the pre-scaler and create timetables for
    ///
    void set_prescaler(int rate, int clock) {
        // frequency base
        freqbase = rate ? static_cast<float>(clock) / rate : 0;
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
};

/// @brief Set algorithm routing.
static void set_routing(EngineState* engine, Voice* voice, int carrier_index) {
    int32_t *carrier = &engine->out_fm[carrier_index];

    int32_t **om1 = &voice->connect1;
    int32_t **om2 = &voice->connect3;
    int32_t **oc1 = &voice->connect2;

    int32_t **memc = &voice->mem_connect;

    switch (voice->algorithm) {
    case 0:
        // M1---C1---MEM---M2---C2---OUT
        *om1  = &engine->c1;
        *oc1  = &engine->mem;
        *om2  = &engine->c2;
        *memc = &engine->m2;
        break;
    case 1:
        // M1------+-MEM---M2---C2---OUT
        //      C1-+
        *om1  = &engine->mem;
        *oc1  = &engine->mem;
        *om2  = &engine->c2;
        *memc = &engine->m2;
        break;
    case 2:
        // M1-----------------+-C2---OUT
        //      C1---MEM---M2-+
        *om1  = &engine->c2;
        *oc1  = &engine->mem;
        *om2  = &engine->c2;
        *memc = &engine->m2;
        break;
    case 3:
        // M1---C1---MEM------+-C2---OUT
        //                 M2-+
        *om1  = &engine->c1;
        *oc1  = &engine->mem;
        *om2  = &engine->c2;
        *memc = &engine->c2;
        break;
    case 4:
        // M1---C1-+-OUT
        // M2---C2-+
        // MEM: not used
        *om1  = &engine->c1;
        *oc1  = carrier;
        *om2  = &engine->c2;
        *memc = &engine->mem;  // store it anywhere where it will not be used
        break;
    case 5:
        //    +----C1----+
        // M1-+-MEM---M2-+-OUT
        //    +----C2----+
        *om1  = nullptr;  // special mark
        *oc1  = carrier;
        *om2  = carrier;
        *memc = &engine->m2;
        break;
    case 6:
        // M1---C1-+
        //      M2-+-OUT
        //      C2-+
        // MEM: not used
        *om1  = &engine->c1;
        *oc1  = carrier;
        *om2  = carrier;
        *memc = &engine->mem;  // store it anywhere where it will not be used
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
        *memc = &engine->mem;  // store it anywhere where it will not be used
        break;
    }
    voice->connect4 = carrier;
}

/// @brief Advance LFO to next sample.
static inline void advance_lfo(EngineState* engine) {
    if (engine->lfo_timer_overflow) {  // LFO enabled ?
        // increment LFO timer
        engine->lfo_timer +=  engine->lfo_timer_add;
        // when LFO is enabled, one level will last for
        // 108, 77, 71, 67, 62, 44, 8 or 5 samples
        while (engine->lfo_timer >= engine->lfo_timer_overflow) {
            engine->lfo_timer -= engine->lfo_timer_overflow;
            // There are 128 LFO steps
            engine->lfo_cnt = ( engine->lfo_cnt + 1 ) & 127;
            // triangle (inverted)
            // AM: from 126 to 0 step -2, 0 to 126 step +2
            if (engine->lfo_cnt<64)
                engine->lfo_AM_step = (engine->lfo_cnt ^ 63) << 1;
            else
                engine->lfo_AM_step = (engine->lfo_cnt & 63) << 1;
            // PM works with 4 times slower clock
            engine->lfo_PM_step = engine->lfo_cnt >> 2;
        }
    }
}

static inline void advance_eg_channel(EngineState* engine, Operator* oprtr) {
    // four operators per channel
    for (unsigned i = 0; i < 4; i++) {  // reset SSG-EG swap flag
        unsigned int swap_flag = 0;
        switch(oprtr->state) {
        case EG_ATT:  // attack phase
            if (!(engine->eg_cnt & ((1 << oprtr->eg_sh_ar) - 1))) {
                oprtr->volume += (~oprtr->volume * (eg_inc[oprtr->eg_sel_ar + ((engine->eg_cnt>>oprtr->eg_sh_ar) & 7)])) >> 4;
                if (oprtr->volume <= MIN_ATT_INDEX) {
                    oprtr->volume = MIN_ATT_INDEX;
                    oprtr->state = EG_DEC;
                }
            }
            break;
        case EG_DEC:  // decay phase
            if (oprtr->ssg & 0x08) {  // SSG EG type envelope selected
                if (!(engine->eg_cnt & ((1 << oprtr->eg_sh_d1r) - 1))) {
                    oprtr->volume += 4 * eg_inc[oprtr->eg_sel_d1r + ((engine->eg_cnt>>oprtr->eg_sh_d1r) & 7)];
                    if ( oprtr->volume >= static_cast<int32_t>(oprtr->sl) )
                        oprtr->state = EG_SUS;
                }
            } else {
                if (!(engine->eg_cnt & ((1 << oprtr->eg_sh_d1r) - 1))) {
                    oprtr->volume += eg_inc[oprtr->eg_sel_d1r + ((engine->eg_cnt>>oprtr->eg_sh_d1r) & 7)];
                    if (oprtr->volume >= static_cast<int32_t>(oprtr->sl))
                        oprtr->state = EG_SUS;
                }
            }
            break;
        case EG_SUS:  // sustain phase
            if (oprtr->ssg & 0x08) {  // SSG EG type envelope selected
                if (!(engine->eg_cnt & ((1 << oprtr->eg_sh_d2r) - 1))) {
                    oprtr->volume += 4 * eg_inc[oprtr->eg_sel_d2r + ((engine->eg_cnt>>oprtr->eg_sh_d2r) & 7)];
                    if (oprtr->volume >= ENV_QUIET) {
                        oprtr->volume = MAX_ATT_INDEX;
                        if (oprtr->ssg & 0x01) {  // bit 0 = hold
                            if (oprtr->ssgn & 1) {  // have we swapped once ???
                                // yes, so do nothing, just hold current level
                            } else {  // bit 1 = alternate
                                swap_flag = (oprtr->ssg & 0x02) | 1;
                            }
                        } else {  // same as KEY-ON operation
                            // restart of the Phase Generator should be here
                            oprtr->phase = 0;
                            // phase -> Attack
                            oprtr->volume = 511;
                            oprtr->state = EG_ATT;
                            // bit 1 = alternate
                            swap_flag = (oprtr->ssg & 0x02);
                        }
                    }
                }
            } else {
                if (!(engine->eg_cnt & ((1 << oprtr->eg_sh_d2r) - 1))) {
                    oprtr->volume += eg_inc[oprtr->eg_sel_d2r + ((engine->eg_cnt>>oprtr->eg_sh_d2r) & 7)];
                    if (oprtr->volume >= MAX_ATT_INDEX) {
                        oprtr->volume = MAX_ATT_INDEX;
                        // do not change oprtr->state (verified on real chip)
                    }
                }
            }
            break;
        case EG_REL:  // release phase
            if (!(engine->eg_cnt & ((1 << oprtr->eg_sh_rr) - 1))) {
                // SSG-EG affects Release phase also (Nemesis)
                oprtr->volume += eg_inc[oprtr->eg_sel_rr + ((engine->eg_cnt>>oprtr->eg_sh_rr) & 7)];
                if (oprtr->volume >= MAX_ATT_INDEX) {
                    oprtr->volume = MAX_ATT_INDEX;
                    oprtr->state = EG_OFF;
                }
            }
            break;
        }
        // get the output volume from the slot
        unsigned int out = static_cast<uint32_t>(oprtr->volume);
        // negate output (changes come from alternate bit, init comes from
        // attack bit)
        if ((oprtr->ssg & 0x08) && (oprtr->ssgn & 2) && (oprtr->state > EG_REL))
            out ^= MAX_ATT_INDEX;
        // we need to store the result here because we are going to change
        // ssgn in next instruction
        oprtr->vol_out = out + oprtr->tl;
        // reverse oprtr inversion flag
        oprtr->ssgn ^= swap_flag;
        // increment the slot and decrement the iterator
        oprtr++;
    }
}

static inline void update_phase_lfo_channel(EngineState* engine, Voice* voice) {
    uint32_t block_fnum = voice->block_fnum;
    uint32_t fnum_lfo  = ((block_fnum & 0x7f0) >> 4) * 32 * 8;
    int32_t  lfo_fn_table_index_offset = lfo_pm_table[fnum_lfo + voice->pms + engine->lfo_PM_step];
    if (lfo_fn_table_index_offset) {  // LFO phase modulation active
        block_fnum = block_fnum * 2 + lfo_fn_table_index_offset;
        uint8_t blk = (block_fnum & 0x7000) >> 12;
        uint32_t fn = block_fnum & 0xfff;
        // key-scale code
        int kc = (blk << 2) | opn_fktable[fn >> 8];
        // phase increment counter
        int fc = (engine->fn_table[fn]>>(7 - blk));
        // detects frequency overflow (credits to Nemesis)
        int finc = fc + voice->operators[Op1].DT[kc];
        // Operator 1
        if (finc < 0) finc += engine->fn_max;
        voice->operators[Op1].phase += (finc * voice->operators[Op1].mul) >> 1;
        // Operator 2
        finc = fc + voice->operators[Op2].DT[kc];
        if (finc < 0) finc += engine->fn_max;
        voice->operators[Op2].phase += (finc * voice->operators[Op2].mul) >> 1;
        // Operator 3
        finc = fc + voice->operators[Op3].DT[kc];
        if (finc < 0) finc += engine->fn_max;
        voice->operators[Op3].phase += (finc * voice->operators[Op3].mul) >> 1;
        // Operator 4
        finc = fc + voice->operators[Op4].DT[kc];
        if (finc < 0) finc += engine->fn_max;
        voice->operators[Op4].phase += (finc * voice->operators[Op4].mul) >> 1;
    } else {  // LFO phase modulation is 0
        voice->operators[Op1].phase += voice->operators[Op1].phase_increment;
        voice->operators[Op2].phase += voice->operators[Op2].phase_increment;
        voice->operators[Op3].phase += voice->operators[Op3].phase_increment;
        voice->operators[Op4].phase += voice->operators[Op4].phase_increment;
    }
}

/// @brief Return the value of operator (2,3,4) given phase, envelope, and PM.
///
/// @param phase the current phase of the operator's oscillator
/// @param env the value of the operator's envelope
/// @param pm the amount of phase modulation for the operator
///
static inline signed int op_calc(uint32_t phase, unsigned int env, signed int pm) {
    uint32_t p = (env << 3) + sin_tab[(((signed int)((phase & ~FREQ_MASK) + (pm << 15))) >> FREQ_SH) & SIN_MASK];
    if (p >= TL_TAB_LEN) return 0;
    return tl_tab[p];
}

/// @brief Return the value of operator (1) given phase, envelope, and PM.
///
/// @param phase the current phase of the operator's oscillator
/// @param env the value of the operator's envelope
/// @param pm the amount of phase modulation for the operator
///
static inline signed int op_calc1(uint32_t phase, unsigned int env, signed int pm) {
    uint32_t p = (env << 3) + sin_tab[(((signed int)((phase & ~FREQ_MASK) + pm        )) >> FREQ_SH) & SIN_MASK];
    if (p >= TL_TAB_LEN) return 0;
    return tl_tab[p];
}

static inline void chan_calc(EngineState* engine, Voice* voice) {
#define CALCULATE_VOLUME(OP) ((OP)->vol_out + (AM & (OP)->AMmask))
    uint32_t AM = engine->lfo_AM_step >> voice->ams;
    engine->m2 = engine->c1 = engine->c2 = engine->mem = 0;
    // restore delayed sample (MEM) value to m2 or c2
    *voice->mem_connect = voice->mem_value;
    // Operator 1
    unsigned int eg_out = CALCULATE_VOLUME(&voice->operators[Op1]);
    int32_t out = voice->op1_out[0] + voice->op1_out[1];
    voice->op1_out[0] = voice->op1_out[1];
    if (!voice->connect1) {  // algorithm 5
        engine->mem = engine->c1 = engine->c2 = voice->op1_out[0];
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
        *voice->connect3 += op_calc(voice->operators[Op3].phase, eg_out, engine->m2);
    // Operator 2
    eg_out = CALCULATE_VOLUME(&voice->operators[Op2]);
    if (eg_out < ENV_QUIET)
        *voice->connect2 += op_calc(voice->operators[Op2].phase, eg_out, engine->c1);
    // Operator 4
    eg_out = CALCULATE_VOLUME(&voice->operators[Op4]);
    if (eg_out < ENV_QUIET)
        *voice->connect4 += op_calc(voice->operators[Op4].phase, eg_out, engine->c2);
    // store current MEM
    voice->mem_value = engine->mem;
    // update phase counters AFTER output calculations
    if (voice->pms) {
        update_phase_lfo_channel(engine, voice);
    } else {  // no LFO phase modulation
        voice->operators[Op1].phase += voice->operators[Op1].phase_increment;
        voice->operators[Op2].phase += voice->operators[Op2].phase_increment;
        voice->operators[Op3].phase += voice->operators[Op3].phase_increment;
        voice->operators[Op4].phase += voice->operators[Op4].phase_increment;
    }
#undef CALCULATE_VOLUME
}

/// @brief Update phase increment and envelope generator
static inline void refresh_fc_eg_slot(EngineState* engine, Operator* oprtr, int fc, int kc) {
    int ksr = kc >> oprtr->KSR;
    fc += oprtr->DT[kc];
    // detects frequency overflow (credits to Nemesis)
    if (fc < 0) fc += engine->fn_max;
    // (frequency) phase increment counter
    oprtr->phase_increment = (fc * oprtr->mul) >> 1;
    if ( oprtr->ksr != ksr ) {
        oprtr->ksr = ksr;
        // calculate envelope generator rates
        if ((oprtr->ar + oprtr->ksr) < 32+62) {
            oprtr->eg_sh_ar  = eg_rate_shift [oprtr->ar  + oprtr->ksr ];
            oprtr->eg_sel_ar = eg_rate_select[oprtr->ar  + oprtr->ksr ];
        } else {
            oprtr->eg_sh_ar  = 0;
            oprtr->eg_sel_ar = 17*RATE_STEPS;
        }

        oprtr->eg_sh_d1r = eg_rate_shift [oprtr->d1r + oprtr->ksr];
        oprtr->eg_sh_d2r = eg_rate_shift [oprtr->d2r + oprtr->ksr];
        oprtr->eg_sh_rr  = eg_rate_shift [oprtr->rr  + oprtr->ksr];

        oprtr->eg_sel_d1r= eg_rate_select[oprtr->d1r + oprtr->ksr];
        oprtr->eg_sel_d2r= eg_rate_select[oprtr->d2r + oprtr->ksr];
        oprtr->eg_sel_rr = eg_rate_select[oprtr->rr  + oprtr->ksr];
    }
}

/// @brief Update phase increment counters
static inline void refresh_fc_eg_chan(EngineState* engine, Voice* voice) {
    if (voice->operators[Op1].phase_increment==-1) {
        int fc = voice->fc;
        int kc = voice->kcode;
        refresh_fc_eg_slot(engine, &voice->operators[Op1] , fc , kc );
        refresh_fc_eg_slot(engine, &voice->operators[Op2] , fc , kc );
        refresh_fc_eg_slot(engine, &voice->operators[Op3] , fc , kc );
        refresh_fc_eg_slot(engine, &voice->operators[Op4] , fc , kc );
    }
}

#endif  // DSP_YAMAHA_YM2612_FUNCTIONAL_HPP_
