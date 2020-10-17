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
