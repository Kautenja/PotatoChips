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

#include "yamaha_ym2612.hpp"

extern signed int tl_tab[];
extern unsigned int sin_tab[];
extern const uint32_t sl_table[];
extern const uint8_t eg_inc[];
extern const uint8_t eg_rate_select[];
extern const uint8_t eg_rate_shift[];
extern const uint8_t dt_tab[];
extern const uint8_t opn_fktable[];
extern const uint32_t lfo_samples_per_step[];
extern const uint8_t lfo_ams_depth_shift[];
extern const uint8_t lfo_pm_output[7 * 8][8];
extern int32_t lfo_pm_table[];

// ---------------------------------------------------------------------------
// MARK: YM2612 Emulator class
// ---------------------------------------------------------------------------

void YamahaYM2612::reset() {
    // clear instance variables
    memset(registers, 0, sizeof registers);
    LFO = MOL = MOR = 0;
    // set the frequency scaling parameters of the OPN emulator
    OPNSetPrescaler(&OPN);
    // mode 0 , timer reset
    OPNWriteMode(&OPN, 0x27, 0x30);
    // envelope generator
    OPN.eg_timer = 0;
    OPN.eg_cnt = 0;
    // LFO
    OPN.lfo_timer = 0;
    OPN.lfo_cnt = 0;
    OPN.LFO_AM = 126;
    OPN.LFO_PM = 0;
    // state
    OPN.ST.status = 0;
    OPN.ST.mode = 0;

    OPNWriteMode(&OPN, 0x27, 0x30);
    OPNWriteMode(&OPN, 0x26, 0x00);
    OPNWriteMode(&OPN, 0x25, 0x00);
    OPNWriteMode(&OPN, 0x24, 0x00);

    reset_channels(&(OPN.ST), &CH[0], 6);

    for (int i = 0xb6; i >= 0xb4; i--) {
        OPNWriteReg(&OPN, i, 0xc0);
        OPNWriteReg(&OPN, i | 0x100, 0xc0);
    }

    for (int i = 0xb2; i >= 0x30; i--) {
        OPNWriteReg(&OPN, i, 0);
        OPNWriteReg(&OPN, i | 0x100, 0);
    }

    // DAC mode clear
    is_DAC_enabled = 0;
    out_DAC = 0;
    for (int c = 0; c < 6; c++) setST(c, 3);
}

void YamahaYM2612::step() {
    int lt, rt;
    // refresh PG and EG
    refresh_fc_eg_chan(&OPN, &CH[0]);
    refresh_fc_eg_chan(&OPN, &CH[1]);
    refresh_fc_eg_chan(&OPN, &CH[2]);
    refresh_fc_eg_chan(&OPN, &CH[3]);
    refresh_fc_eg_chan(&OPN, &CH[4]);
    refresh_fc_eg_chan(&OPN, &CH[5]);
    // clear outputs
    OPN.out_fm[0] = 0;
    OPN.out_fm[1] = 0;
    OPN.out_fm[2] = 0;
    OPN.out_fm[3] = 0;
    OPN.out_fm[4] = 0;
    OPN.out_fm[5] = 0;
    // update SSG-EG output
    update_ssg_eg_channel(&(CH[0].SLOT[SLOT1]));
    update_ssg_eg_channel(&(CH[1].SLOT[SLOT1]));
    update_ssg_eg_channel(&(CH[2].SLOT[SLOT1]));
    update_ssg_eg_channel(&(CH[3].SLOT[SLOT1]));
    update_ssg_eg_channel(&(CH[4].SLOT[SLOT1]));
    update_ssg_eg_channel(&(CH[5].SLOT[SLOT1]));
    // calculate FM
    chan_calc(&OPN, &CH[0]);
    chan_calc(&OPN, &CH[1]);
    chan_calc(&OPN, &CH[2]);
    chan_calc(&OPN, &CH[3]);
    chan_calc(&OPN, &CH[4]);
    if (is_DAC_enabled)
        *&CH[5].connect4 += out_DAC;
    else
        chan_calc(&OPN, &CH[5]);
    // advance LFO
    advance_lfo(&OPN);
    // advance envelope generator
    OPN.eg_timer += OPN.eg_timer_add;
    while (OPN.eg_timer >= OPN.eg_timer_overflow) {
        OPN.eg_timer -= OPN.eg_timer_overflow;
        OPN.eg_cnt++;
        advance_eg_channel(&OPN, &(CH[0].SLOT[SLOT1]));
        advance_eg_channel(&OPN, &(CH[1].SLOT[SLOT1]));
        advance_eg_channel(&OPN, &(CH[2].SLOT[SLOT1]));
        advance_eg_channel(&OPN, &(CH[3].SLOT[SLOT1]));
        advance_eg_channel(&OPN, &(CH[4].SLOT[SLOT1]));
        advance_eg_channel(&OPN, &(CH[5].SLOT[SLOT1]));
    }
    // clip outputs
    if (OPN.out_fm[0] > 8191)
        OPN.out_fm[0] = 8191;
    else if (OPN.out_fm[0] < -8192)
        OPN.out_fm[0] = -8192;
    if (OPN.out_fm[1] > 8191)
        OPN.out_fm[1] = 8191;
    else if (OPN.out_fm[1] < -8192)
        OPN.out_fm[1] = -8192;
    if (OPN.out_fm[2] > 8191)
        OPN.out_fm[2] = 8191;
    else if (OPN.out_fm[2] < -8192)
        OPN.out_fm[2] = -8192;
    if (OPN.out_fm[3] > 8191)
        OPN.out_fm[3] = 8191;
    else if (OPN.out_fm[3] < -8192)
        OPN.out_fm[3] = -8192;
    if (OPN.out_fm[4] > 8191)
        OPN.out_fm[4] = 8191;
    else if (OPN.out_fm[4] < -8192)
        OPN.out_fm[4] = -8192;
    if (OPN.out_fm[5] > 8191)
        OPN.out_fm[5] = 8191;
    else if (OPN.out_fm[5] < -8192)
        OPN.out_fm[5] = -8192;
    // 6-channels mixing
    lt  = ((OPN.out_fm[0] >> 0) & OPN.pan[0]);
    rt  = ((OPN.out_fm[0] >> 0) & OPN.pan[1]);
    lt += ((OPN.out_fm[1] >> 0) & OPN.pan[2]);
    rt += ((OPN.out_fm[1] >> 0) & OPN.pan[3]);
    lt += ((OPN.out_fm[2] >> 0) & OPN.pan[4]);
    rt += ((OPN.out_fm[2] >> 0) & OPN.pan[5]);
    lt += ((OPN.out_fm[3] >> 0) & OPN.pan[6]);
    rt += ((OPN.out_fm[3] >> 0) & OPN.pan[7]);
    lt += ((OPN.out_fm[4] >> 0) & OPN.pan[8]);
    rt += ((OPN.out_fm[4] >> 0) & OPN.pan[9]);
    lt += ((OPN.out_fm[5] >> 0) & OPN.pan[10]);
    rt += ((OPN.out_fm[5] >> 0) & OPN.pan[11]);
    // output buffering
    MOL = lt;
    MOR = rt;
    // timer A control
    if ((OPN.ST.TAC -= static_cast<int>(OPN.ST.freqbase * 4096)) <= 0)
        TimerAOver(&OPN.ST);
    // timer B control
    if ((OPN.ST.TBC -= static_cast<int>(OPN.ST.freqbase * 4096)) <= 0)
        TimerBOver(&OPN.ST);
}

void YamahaYM2612::write(uint8_t a, uint8_t v) {
    int addr;
    // adjust to 8 bit bus
    v &= 0xff;
    switch (a & 3) {
    case 0:  // address port 0
        OPN.ST.address = v;
        addr_A1 = 0;
        break;
    case 1:  // data port 0
        // verified on real YM2608
        if (addr_A1 != 0) break;
        // get the address from the latch and write the data
        addr = OPN.ST.address;
        registers[addr] = v;
        switch (addr & 0xf0) {
        case 0x20:  // 0x20-0x2f Mode
            switch (addr) {
            case 0x2a:  // DAC data (YM2612), level unknown
                out_DAC = ((int) v - 0x80) << 6;
                break;
            case 0x2b:  // DAC Sel (YM2612), b7 = dac enable
                is_DAC_enabled = v & 0x80;
                break;
            default:  // OPN section, write register
                OPNWriteMode(&OPN, addr, v);
            }
            break;
        default:  // 0x30-0xff OPN section, write register
            OPNWriteReg(&OPN, addr, v);
        }
        break;
    case 2:  // address port 1
        OPN.ST.address = v;
        addr_A1 = 1;
        break;
    case 3:  // data port 1
        // verified on real YM2608
        if (addr_A1 != 1) break;
        // get the address from the latch and right to the given register
        addr = OPN.ST.address;
        registers[addr | 0x100] = v;
        OPNWriteReg(&OPN, addr | 0x100, v);
        break;
    }
}

void YamahaYM2612::setAR(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].AR == value) return;
    channels[channel].operators[slot].AR = value;
    FM_SLOT *s = &CH[channel].SLOT[slots_idx[slot]];
    s->ar_ksr = (s->ar_ksr & 0xC0) | (value & 0x1f);
    set_ar_ksr(&CH[channel], s, s->ar_ksr);
}

void YamahaYM2612::setD1(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].D1 == value) return;
    channels[channel].operators[slot].D1 = value;
    FM_SLOT *s = &CH[channel].SLOT[slots_idx[slot]];
    s->dr = (s->dr & 0x80) | (value & 0x1F);
    set_dr(s, s->dr);
}

void YamahaYM2612::setSL(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].SL == value) return;
    channels[channel].operators[slot].SL = value;
    FM_SLOT *s =  &CH[channel].SLOT[slots_idx[slot]];
    s->sl_rr = (s->sl_rr & 0x0f) | ((value & 0x0f) << 4);
    set_sl_rr(s, s->sl_rr);
}

void YamahaYM2612::setD2(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].D2 == value) return;
    channels[channel].operators[slot].D2 = value;
    set_sr(&CH[channel].SLOT[slots_idx[slot]], value);
}

void YamahaYM2612::setRR(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].RR == value) return;
    channels[channel].operators[slot].RR = value;
    FM_SLOT *s =  &CH[channel].SLOT[slots_idx[slot]];
    s->sl_rr = (s->sl_rr & 0xf0) | (value & 0x0f);
    set_sl_rr(s, s->sl_rr);
}

void YamahaYM2612::setTL(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].TL == value) return;
    channels[channel].operators[slot].TL = value;
    set_tl(&CH[channel], &CH[channel].SLOT[slots_idx[slot]], value);
}

void YamahaYM2612::setMUL(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].MUL == value) return;
    channels[channel].operators[slot].MUL = value;
    CH[channel].SLOT[slots_idx[slot]].mul = (value & 0x0f) ? (value & 0x0f) * 2 : 1;
    CH[channel].SLOT[SLOT1].Incr = -1;
}

void YamahaYM2612::setDET(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].DET == value) return;
    channels[channel].operators[slot].DET = value;
    CH[channel].SLOT[slots_idx[slot]].DT  = OPN.ST.dt_tab[(value)&7];
    CH[channel].SLOT[SLOT1].Incr = -1;
}

void YamahaYM2612::setRS(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].RS == value) return;
    channels[channel].operators[slot].RS = value;
    FM_SLOT *s = &CH[channel].SLOT[slots_idx[slot]];
    s->ar_ksr = (s->ar_ksr & 0x1F) | ((value & 0x03) << 6);
    set_ar_ksr(&CH[channel], s, s->ar_ksr);
}

void YamahaYM2612::setAM(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].AM == value) return;
    channels[channel].operators[slot].AM = value;
    FM_SLOT *s = &CH[channel].SLOT[slots_idx[slot]];
    s->AMmask = (value) ? ~0 : 0;
}
