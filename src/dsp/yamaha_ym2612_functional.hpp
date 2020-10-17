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

// ---------------------------------------------------------------------------
// MARK: OPN unit
// ---------------------------------------------------------------------------

/// @brief OPN/A/B common state.
struct EngineState {
    /// chip type
    uint8_t type = 0;
    /// general state
    GlobalOperatorState state;
    /// @brief 3 slot mode state (special mode where each operator on channel
    /// 3 can have a different root frequency)
    struct SpecialModeState {
        /// fnum3,blk3: calculated
        uint32_t fc[3] = {0, 0, 0};
        /// freq3 latch
        uint8_t fn_h = 0;
        /// key code
        uint8_t kcode[3] = {0, 0, 0};
        /// current fnum value for this slot (can be different between slots of
        /// one channel in 3slot mode)
        uint32_t block_fnum[3] = {0, 0, 0};
    } special_mode_state;
    /// pointer to voices
    Voice* voices = nullptr;
    /// fm channels output masks (0xffffffff = enable) */
    unsigned int pan[6 * 2];

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
};

// ---------------------------------------------------------------------------
// MARK: Functional API
// ---------------------------------------------------------------------------

/// initialize time tables
static void init_timetables(EngineState *OPN, double freqbase) {
    // DeTune table
    for (int d = 0; d <= 3; d++) {
        for (int i = 0; i <= 31; i++) {
            // -10 because chip works with 10.10 fixed point, while we use 16.16
            double rate = ((double) dt_tab[d * 32 + i]) * freqbase * (1 << (FREQ_SH - 10));
            OPN->state.dt_tab[d][i] = (int32_t) rate;
            OPN->state.dt_tab[d + 4][i] = -OPN->state.dt_tab[d][i];
        }
    }
    // there are 2048 FNUMs that can be generated using FNUM/BLK registers
    // but LFO works with one more bit of a precision so we really need 4096
    // elements. calculate fnumber -> increment counter table
    for (int i = 0; i < 4096; i++) {
        // freq table for octave 7
        // OPN phase increment counter = 20bit
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
        OPN->fn_table[i] = (uint32_t)((double) i * 32 * freqbase * (1 << (FREQ_SH - 10)));
    }
    // maximal frequency is required for Phase overflow calculation, register
    // size is 17 bits (Nemesis)
    OPN->fn_max = (uint32_t)((double) 0x20000 * freqbase * (1 << (FREQ_SH - 10)));
}

/// Set pre-scaler and make time tables.
///
/// @param OPN the OPN emulator to set the pre-scaler and create timetables for
///
static void set_prescaler(EngineState *OPN) {
    // frequency base
    OPN->state.freqbase = (OPN->state.rate) ? ((double)OPN->state.clock / OPN->state.rate) : 0;
    // TODO: why is it necessary to scale these increments by a factor of 1/16
    //       to get the correct timings from the EG and LFO?
    // EG timer increment (updates every 3 samples)
    OPN->eg_timer_add = (1 << EG_SH) * OPN->state.freqbase / 16;
    OPN->eg_timer_overflow = 3 * (1 << EG_SH) / 16;
    // LFO timer increment (updates every 16 samples)
    OPN->lfo_timer_add = (1 << LFO_SH) * OPN->state.freqbase / 16;
    // make time tables
    init_timetables(OPN, OPN->state.freqbase);
}

/// set algorithm connection
static void setup_connection(EngineState *OPN, Voice *CH, int ch) {
    int32_t *carrier = &OPN->out_fm[ch];

    int32_t **om1 = &CH->connect1;
    int32_t **om2 = &CH->connect3;
    int32_t **oc1 = &CH->connect2;

    int32_t **memc = &CH->mem_connect;

    switch( CH->algorithm ) {
    case 0:
        // M1---C1---MEM---M2---C2---OUT
        *om1 = &OPN->c1;
        *oc1 = &OPN->mem;
        *om2 = &OPN->c2;
        *memc= &OPN->m2;
        break;
    case 1:
        // M1------+-MEM---M2---C2---OUT
        //      C1-+
        *om1 = &OPN->mem;
        *oc1 = &OPN->mem;
        *om2 = &OPN->c2;
        *memc= &OPN->m2;
        break;
    case 2:
        // M1-----------------+-C2---OUT
        //      C1---MEM---M2-+
        *om1 = &OPN->c2;
        *oc1 = &OPN->mem;
        *om2 = &OPN->c2;
        *memc= &OPN->m2;
        break;
    case 3:
        // M1---C1---MEM------+-C2---OUT
        //                 M2-+
        *om1 = &OPN->c1;
        *oc1 = &OPN->mem;
        *om2 = &OPN->c2;
        *memc= &OPN->c2;
        break;
    case 4:
        // M1---C1-+-OUT
        // M2---C2-+
        // MEM: not used
        *om1 = &OPN->c1;
        *oc1 = carrier;
        *om2 = &OPN->c2;
        *memc= &OPN->mem;  // store it anywhere where it will not be used
        break;
    case 5:
        //    +----C1----+
        // M1-+-MEM---M2-+-OUT
        //    +----C2----+
        *om1 = nullptr;  // special mark
        *oc1 = carrier;
        *om2 = carrier;
        *memc= &OPN->m2;
        break;
    case 6:
        // M1---C1-+
        //      M2-+-OUT
        //      C2-+
        // MEM: not used
        *om1 = &OPN->c1;
        *oc1 = carrier;
        *om2 = carrier;
        *memc= &OPN->mem;  // store it anywhere where it will not be used
        break;
    case 7:
        // M1-+
        // C1-+-OUT
        // M2-+
        // C2-+
        // MEM: not used
        *om1 = carrier;
        *oc1 = carrier;
        *om2 = carrier;
        *memc= &OPN->mem;  // store it anywhere where it will not be used
        break;
    }
    CH->connect4 = carrier;
}

/// advance LFO to next sample.
static inline void advance_lfo(EngineState *OPN) {
    if (OPN->lfo_timer_overflow) {  // LFO enabled ?
        // increment LFO timer
        OPN->lfo_timer +=  OPN->lfo_timer_add;
        // when LFO is enabled, one level will last for
        // 108, 77, 71, 67, 62, 44, 8 or 5 samples
        while (OPN->lfo_timer >= OPN->lfo_timer_overflow) {
            OPN->lfo_timer -= OPN->lfo_timer_overflow;
            // There are 128 LFO steps
            OPN->lfo_cnt = ( OPN->lfo_cnt + 1 ) & 127;
            // triangle (inverted)
            // AM: from 126 to 0 step -2, 0 to 126 step +2
            if (OPN->lfo_cnt<64)
                OPN->lfo_AM_step = (OPN->lfo_cnt ^ 63) << 1;
            else
                OPN->lfo_AM_step = (OPN->lfo_cnt & 63) << 1;
            // PM works with 4 times slower clock
            OPN->lfo_PM_step = OPN->lfo_cnt >> 2;
        }
    }
}

static inline void advance_eg_channel(EngineState *OPN, Operator* oprtr) {
    // four operators per channel
    for (unsigned i = 4; i > 0; i--) {  // reset SSG-EG swap flag
        unsigned int swap_flag = 0;
        switch(oprtr->state) {
        case EG_ATT:  // attack phase
            if (!(OPN->eg_cnt & ((1 << oprtr->eg_sh_ar) - 1))) {
                oprtr->volume += (~oprtr->volume * (eg_inc[oprtr->eg_sel_ar + ((OPN->eg_cnt>>oprtr->eg_sh_ar) & 7)])) >> 4;
                if (oprtr->volume <= MIN_ATT_INDEX) {
                    oprtr->volume = MIN_ATT_INDEX;
                    oprtr->state = EG_DEC;
                }
            }
            break;
        case EG_DEC:  // decay phase
            if (oprtr->ssg & 0x08) {  // SSG EG type envelope selected
                if (!(OPN->eg_cnt & ((1 << oprtr->eg_sh_d1r) - 1))) {
                    oprtr->volume += 4 * eg_inc[oprtr->eg_sel_d1r + ((OPN->eg_cnt>>oprtr->eg_sh_d1r) & 7)];
                    if ( oprtr->volume >= static_cast<int32_t>(oprtr->sl) )
                        oprtr->state = EG_SUS;
                }
            } else {
                if (!(OPN->eg_cnt & ((1 << oprtr->eg_sh_d1r) - 1))) {
                    oprtr->volume += eg_inc[oprtr->eg_sel_d1r + ((OPN->eg_cnt>>oprtr->eg_sh_d1r) & 7)];
                    if (oprtr->volume >= static_cast<int32_t>(oprtr->sl))
                        oprtr->state = EG_SUS;
                }
            }
            break;
        case EG_SUS:  // sustain phase
            if (oprtr->ssg & 0x08) {  // SSG EG type envelope selected
                if (!(OPN->eg_cnt & ((1 << oprtr->eg_sh_d2r) - 1))) {
                    oprtr->volume += 4 * eg_inc[oprtr->eg_sel_d2r + ((OPN->eg_cnt>>oprtr->eg_sh_d2r) & 7)];
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
                if (!(OPN->eg_cnt & ((1 << oprtr->eg_sh_d2r) - 1))) {
                    oprtr->volume += eg_inc[oprtr->eg_sel_d2r + ((OPN->eg_cnt>>oprtr->eg_sh_d2r) & 7)];
                    if (oprtr->volume >= MAX_ATT_INDEX) {
                        oprtr->volume = MAX_ATT_INDEX;
                        // do not change oprtr->state (verified on real chip)
                    }
                }
            }
            break;
        case EG_REL:  // release phase
            if (!(OPN->eg_cnt & ((1 << oprtr->eg_sh_rr) - 1))) {
                // SSG-EG affects Release phase also (Nemesis)
                oprtr->volume += eg_inc[oprtr->eg_sel_rr + ((OPN->eg_cnt>>oprtr->eg_sh_rr) & 7)];
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

static inline void update_phase_lfo_channel(EngineState *OPN, Voice *CH) {
    uint32_t block_fnum = CH->block_fnum;
    uint32_t fnum_lfo  = ((block_fnum & 0x7f0) >> 4) * 32 * 8;
    int32_t  lfo_fn_table_index_offset = lfo_pm_table[fnum_lfo + CH->pms + OPN->lfo_PM_step];
    if (lfo_fn_table_index_offset) {  // LFO phase modulation active
        block_fnum = block_fnum * 2 + lfo_fn_table_index_offset;
        uint8_t blk = (block_fnum & 0x7000) >> 12;
        uint32_t fn = block_fnum & 0xfff;
        // key-scale code
        int kc = (blk << 2) | opn_fktable[fn >> 8];
        // phase increment counter
        int fc = (OPN->fn_table[fn]>>(7 - blk));
        // detects frequency overflow (credits to Nemesis)
        int finc = fc + CH->operators[Op1].DT[kc];
        // Operator 1
        if (finc < 0) finc += OPN->fn_max;
        CH->operators[Op1].phase += (finc * CH->operators[Op1].mul) >> 1;
        // Operator 2
        finc = fc + CH->operators[Op2].DT[kc];
        if (finc < 0) finc += OPN->fn_max;
        CH->operators[Op2].phase += (finc * CH->operators[Op2].mul) >> 1;
        // Operator 3
        finc = fc + CH->operators[Op3].DT[kc];
        if (finc < 0) finc += OPN->fn_max;
        CH->operators[Op3].phase += (finc * CH->operators[Op3].mul) >> 1;
        // Operator 4
        finc = fc + CH->operators[Op4].DT[kc];
        if (finc < 0) finc += OPN->fn_max;
        CH->operators[Op4].phase += (finc * CH->operators[Op4].mul) >> 1;
    } else {  // LFO phase modulation is 0
        CH->operators[Op1].phase += CH->operators[Op1].phase_increment;
        CH->operators[Op2].phase += CH->operators[Op2].phase_increment;
        CH->operators[Op3].phase += CH->operators[Op3].phase_increment;
        CH->operators[Op4].phase += CH->operators[Op4].phase_increment;
    }
}

static inline void chan_calc(EngineState *OPN, Voice *CH) {
#define CALCULATE_VOLUME(OP) ((OP)->vol_out + (AM & (OP)->AMmask))
    uint32_t AM = OPN->lfo_AM_step >> CH->ams;
    OPN->m2 = OPN->c1 = OPN->c2 = OPN->mem = 0;
    // restore delayed sample (MEM) value to m2 or c2
    *CH->mem_connect = CH->mem_value;
    // oprtr 1
    unsigned int eg_out = CALCULATE_VOLUME(&CH->operators[Op1]);
    int32_t out = CH->op1_out[0] + CH->op1_out[1];
    CH->op1_out[0] = CH->op1_out[1];
    if (!CH->connect1) {  // algorithm 5
        OPN->mem = OPN->c1 = OPN->c2 = CH->op1_out[0];
    } else {  // other algorithms
        *CH->connect1 += CH->op1_out[0];
    }
    CH->op1_out[1] = 0;
    if (eg_out < ENV_QUIET) {
        if (!CH->feedback) out = 0;
        CH->op1_out[1] = op_calc1(CH->operators[Op1].phase, eg_out, (out << CH->feedback) );
    }
    // oprtr 3
    eg_out = CALCULATE_VOLUME(&CH->operators[Op3]);
    if (eg_out < ENV_QUIET)
        *CH->connect3 += op_calc(CH->operators[Op3].phase, eg_out, OPN->m2);
    // oprtr 2
    eg_out = CALCULATE_VOLUME(&CH->operators[Op2]);
    if (eg_out < ENV_QUIET)
        *CH->connect2 += op_calc(CH->operators[Op2].phase, eg_out, OPN->c1);
    // oprtr 4
    eg_out = CALCULATE_VOLUME(&CH->operators[Op4]);
    if (eg_out < ENV_QUIET)
        *CH->connect4 += op_calc(CH->operators[Op4].phase, eg_out, OPN->c2);
    // store current MEM
    CH->mem_value = OPN->mem;
    // update phase counters AFTER output calculations
    if (CH->pms) {
        update_phase_lfo_channel(OPN, CH);
    } else {  // no LFO phase modulation
        CH->operators[Op1].phase += CH->operators[Op1].phase_increment;
        CH->operators[Op2].phase += CH->operators[Op2].phase_increment;
        CH->operators[Op3].phase += CH->operators[Op3].phase_increment;
        CH->operators[Op4].phase += CH->operators[Op4].phase_increment;
    }
#undef CALCULATE_VOLUME
}

/// update phase increment and envelope generator
static inline void refresh_fc_eg_slot(EngineState *OPN, Operator* oprtr, int fc, int kc) {
    int ksr = kc >> oprtr->KSR;
    fc += oprtr->DT[kc];
    // detects frequency overflow (credits to Nemesis)
    if (fc < 0) fc += OPN->fn_max;
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

/// update phase increment counters
static inline void refresh_fc_eg_chan(EngineState *OPN, Voice *CH) {
    if ( CH->operators[Op1].phase_increment==-1) {
        int fc = CH->fc;
        int kc = CH->kcode;
        refresh_fc_eg_slot(OPN, &CH->operators[Op1] , fc , kc );
        refresh_fc_eg_slot(OPN, &CH->operators[Op2] , fc , kc );
        refresh_fc_eg_slot(OPN, &CH->operators[Op3] , fc , kc );
        refresh_fc_eg_slot(OPN, &CH->operators[Op4] , fc , kc );
    }
}

/// write a OPN mode register 0x20-0x2f.
static void write_mode(EngineState *OPN, int r, int v) {
    switch (r) {
    case 0x21:  // Test
        break;
    case 0x22:  // LFO FREQ (YM2608/YM2610/YM2610B/YM2612)
        if (v & 8) {  // LFO enabled ?
            OPN->lfo_timer_overflow = lfo_samples_per_step[v&7] << LFO_SH;
        } else {
            // hold LFO waveform in reset state
            OPN->lfo_timer_overflow = 0;
            OPN->lfo_timer = 0;
            OPN->lfo_cnt   = 0;
            OPN->lfo_PM_step    = 0;
            OPN->lfo_AM_step    = 126;
        }
        break;
    case 0x24:  // timer A High 8
        OPN->state.TA = (OPN->state.TA & 0x0003) | (v << 2);
        break;
    case 0x25:  // timer A Low 2
        OPN->state.TA = (OPN->state.TA & 0x03fc) | (v & 3);
        break;
    case 0x26:  // timer B
        OPN->state.TB = v;
        break;
    case 0x27:  // mode, timer control
        set_timers(&(OPN->state), v);
        break;
    case 0x28:  // key on / off
        uint8_t c = v & 0x03;
        if (c == 3) break;
        if ((v & 0x04) && (OPN->type & TYPE_6CH)) c += 3;
        Voice* CH = OPN->voices;
        CH = &CH[c];
        if (v & 0x10) set_keyon(CH, Op1); else set_keyoff(CH, Op1);
        if (v & 0x20) set_keyon(CH, Op2); else set_keyoff(CH, Op2);
        if (v & 0x40) set_keyon(CH, Op3); else set_keyoff(CH, Op3);
        if (v & 0x80) set_keyon(CH, Op4); else set_keyoff(CH, Op4);
        break;
    }
}

/// write a OPN register (0x30-0xff).
static void write_register(EngineState *OPN, int r, int v) {
    uint8_t c = getVoice(r);
    // 0xX3, 0xX7, 0xXB, 0xXF
    if (c == 3) return;
    if (r >= 0x100) c+=3;
    // get the channel
    Voice* const CH = &OPN->voices[c];
    // get the operator
    Operator* const oprtr = &(CH->operators[getOp(r)]);
    switch (r & 0xf0) {
    case 0x30:  // DET, MUL
        set_det_mul(&OPN->state, CH, oprtr, v);
        break;
    case 0x40:  // TL
        set_tl(CH, oprtr, v);
        break;
    case 0x50:  // KS, AR
        set_ar_ksr(CH, oprtr, v);
        break;
    case 0x60:  // bit7 = AM ENABLE, DR
        set_dr(oprtr, v);
        if (OPN->type & TYPE_LFOPAN)  // YM2608/2610/2610B/2612
            oprtr->AMmask = (v & 0x80) ? ~0 : 0;
        break;
    case 0x70:  // SR
        set_sr(oprtr, v);
        break;
    case 0x80:  // SL, RR
        set_sl_rr(oprtr, v);
        break;
    case 0x90:  // SSG-EG
        oprtr->ssg  =  v&0x0f;
        // recalculate EG output
        if ((oprtr->ssg & 0x08) && (oprtr->ssgn ^ (oprtr->ssg & 0x04)) && (oprtr->state > EG_REL))
            oprtr->vol_out = ((uint32_t) (0x200 - oprtr->volume) & MAX_ATT_INDEX) + oprtr->tl;
        else
            oprtr->vol_out = (uint32_t) oprtr->volume + oprtr->tl;
        break;
    case 0xa0:
        switch (getOp(r)) {
        case 0:  {  // 0xa0-0xa2 : FNUM1
            uint32_t fn = (((uint32_t)( (OPN->state.fn_h) & 7)) << 8) + v;
            uint8_t blk = OPN->state.fn_h >> 3;
            /* key-scale code */
            CH->kcode = (blk << 2) | opn_fktable[(fn >> 7) & 0xf];
            /* phase increment counter */
            CH->fc = OPN->fn_table[fn * 2] >> (7 - blk);
            /* store fnum in clear form for LFO PM calculations */
            CH->block_fnum = (blk << 11) | fn;
            CH->operators[Op1].phase_increment = -1;
            break;
        }
        case 1:  // 0xa4-0xa6 : FNUM2,BLK
            OPN->state.fn_h = v&0x3f;
            break;
        case 2:  // 0xa8-0xaa : 3CH FNUM1
            if (r < 0x100) {
                uint32_t fn = (((uint32_t)(OPN->special_mode_state.fn_h & 7)) << 8) + v;
                uint8_t blk = OPN->special_mode_state.fn_h >> 3;
                /* keyscale code */
                OPN->special_mode_state.kcode[c]= (blk << 2) | opn_fktable[(fn >> 7) & 0xf];
                /* phase increment counter */
                OPN->special_mode_state.fc[c] = OPN->fn_table[fn * 2] >> (7 - blk);
                OPN->special_mode_state.block_fnum[c] = (blk << 11) | fn;
                (OPN->voices)[2].operators[Op1].phase_increment = -1;
            }
            break;
        case 3:  // 0xac-0xae : 3CH FNUM2, BLK
            if (r < 0x100)
                OPN->special_mode_state.fn_h = v&0x3f;
            break;
        }
        break;
    case 0xb0:
        switch (getOp(r)) {
        case 0: {  // 0xb0-0xb2 : feedback (FB), algorithm (ALGO)
            int feedback = (v >> 3) & 7;
            CH->algorithm = v & 7;
            CH->feedback = feedback ? feedback + 6 : 0;
            setup_connection(OPN, CH, c);
            break;
        }
        case 1:  // 0xb4-0xb6 : L, R, AMS, PMS (YM2612/YM2610B/YM2610/YM2608)
            if (OPN->type & TYPE_LFOPAN) {
                // b0-2 PMS
                // CH->pms = PM depth * 32 (index in lfo_pm_table)
                CH->pms = (v & 7) * 32;
                // b4-5 AMS
                CH->ams = lfo_ams_depth_shift[(v >> 4) & 0x03];
                // PAN :  b7 = L, b6 = R
                OPN->pan[c * 2    ] = (v & 0x80) ? ~0 : 0;
                OPN->pan[c * 2 + 1] = (v & 0x40) ? ~0 : 0;
            }
            break;
        }
        break;
    }
}

#endif  // DSP_YAMAHA_YM2612_FUNCTIONAL_HPP_
