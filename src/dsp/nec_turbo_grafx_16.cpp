// Turbo Grafx 16 (PC Engine) PSG sound chip emulator
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

#include "nec_turbo_grafx_16.hpp"
#include <cstring>

void NECTurboGrafx16::balance_changed(NECTurboGrafx16_Oscillator& osc) {
    static const short log_table[32] = {  // ~1.5 db per step
        #define ENTRY(factor) short (factor * NECTurboGrafx16_Oscillator::amp_range / 31.0 + 0.5)
        ENTRY(0.000000), ENTRY(0.005524), ENTRY(0.006570), ENTRY(0.007813),
        ENTRY(0.009291), ENTRY(0.011049), ENTRY(0.013139), ENTRY(0.015625),
        ENTRY(0.018581), ENTRY(0.022097), ENTRY(0.026278), ENTRY(0.031250),
        ENTRY(0.037163), ENTRY(0.044194), ENTRY(0.052556), ENTRY(0.062500),
        ENTRY(0.074325), ENTRY(0.088388), ENTRY(0.105112), ENTRY(0.125000),
        ENTRY(0.148651), ENTRY(0.176777), ENTRY(0.210224), ENTRY(0.250000),
        ENTRY(0.297302), ENTRY(0.353553), ENTRY(0.420448), ENTRY(0.500000),
        ENTRY(0.594604), ENTRY(0.707107), ENTRY(0.840896), ENTRY(1.000000),
        #undef ENTRY
    };

    int vol = (osc.control & 0x1F) - 0x1E * 2;

    int left  = vol + (osc.balance >> 3 & 0x1E) + (balance >> 3 & 0x1E);
    if (left  < 0) left  = 0;

    int right = vol + (osc.balance << 1 & 0x1E) + (balance << 1 & 0x1E);
    if (right < 0) right = 0;

    left  = log_table[left ];
    right = log_table[right];

    // optimizing for the common case of being centered also allows easy
    // panning using Effects_Buffer
    osc.outputs[0] = osc.chans[0];  // center
    osc.outputs[1] = 0;
    if (left != right) {
        osc.outputs[0] = osc.chans[1];  // left
        osc.outputs[1] = osc.chans[2];  // right
    }

    if (CENTER_WAVES) {
        osc.last_amp[0] += (left  - osc.volume[0]) * 16;
        osc.last_amp[1] += (right - osc.volume[1]) * 16;
    }

    osc.volume[0] = left;
    osc.volume[1] = right;
}
