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

// extern signed int tl_tab[];
// extern unsigned int sin_tab[];
// extern const uint32_t sl_table[];
// extern const uint8_t eg_inc[];
// extern const uint8_t eg_rate_select[];
// extern const uint8_t eg_rate_shift[];
// extern const uint8_t dt_tab[];
// extern const uint8_t opn_fktable[];
// extern const uint32_t lfo_samples_per_step[];
// extern const uint8_t lfo_ams_depth_shift[];
// extern const uint8_t lfo_pm_output[7 * 8][8];
// extern int32_t lfo_pm_table[];
