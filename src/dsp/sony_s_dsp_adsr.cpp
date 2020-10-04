// Sony S-DSP ADSR envelope generator emulator.
// Copyright 2020 Christian Kauten
// Copyright 2006 Shay Green
// Copyright 2002 Brad Martin
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
// Based on Brad Martin's OpenSPC DSP emulator
//

#include "sony_s_dsp_adsr.hpp"

/// The initial value of the envelope.
const int ENVELOPE_RATE_INITIAL = 0x7800;

/// the range of the envelope generator amplitude level (i.e., max value)
const int ENVELOPE_RANGE = 0x0800;

/// This table is for envelope timing.  It represents the number of counts
/// that should be subtracted from the counter each sample period (32kHz).
/// The counter starts at 30720 (0x7800). Each count divides exactly into
/// 0x7800 without remainder.
static const uint16_t ENVELOPE_RATES[0x20] = {
    0x0000, 0x000F, 0x0014, 0x0018, 0x001E, 0x0028, 0x0030, 0x003C,
    0x0050, 0x0060, 0x0078, 0x00A0, 0x00C0, 0x00F0, 0x0140, 0x0180,
    0x01E0, 0x0280, 0x0300, 0x03C0, 0x0500, 0x0600, 0x0780, 0x0A00,
    0x0C00, 0x0F00, 0x1400, 0x1800, 0x1E00, 0x2800, 0x3C00, 0x7800
};

inline int8_t Sony_S_DSP_ADSR::clock_envelope() {
    int envx = envelope_value;
    if (envelope_stage == EnvelopeStage::Release) {
        // Docs: "When in the state of "key off". the "click" sound is
        // prevented by the addition of the fixed value 1/256" WTF???
        // Alright, I'm going to choose to interpret that this way:
        // When a note is keyed off, start the RELEASE state, which
        // subtracts 1/256th each sample period (32kHz).  Note there's
        // no need for a count because it always happens every update.
        envx -= ENVELOPE_RANGE / 256;
        if (envx <= 0) {
            envelope_stage = EnvelopeStage::Off;
            return 0;
        }
        envelope_value = envx;
        return envx >> 8;
    }

    int cnt = envelope_counter;
    switch (envelope_stage) {
        case EnvelopeStage::Off:  // do nothing
            break;
        case EnvelopeStage::Attack: {
            // increase envelope by 1/64 each step
            if (attack == 15) {
                envx += ENVELOPE_RANGE / 2;
            } else {
                cnt -= ENVELOPE_RATES[2 * attack + 1];
                if (cnt > 0) break;
                envx += ENVELOPE_RANGE / 64;
                cnt = ENVELOPE_RATE_INITIAL;
            }
            if (envx >= ENVELOPE_RANGE) {
                envx = ENVELOPE_RANGE - 1;
                envelope_stage = EnvelopeStage::Decay;
            }
            envelope_value = envx;
            break;
        }
        case EnvelopeStage::Decay: {
            // Docs: "DR...[is multiplied] by the fixed value
            // 1-1/256." Well, at least that makes some sense.
            // Multiplying ENVX by 255/256 every time DECAY is
            // updated.
            cnt -= ENVELOPE_RATES[(decay << 1) + 0x10];

            if (cnt <= 0) {
                cnt = ENVELOPE_RATE_INITIAL;
                envx -= ((envx - 1) >> 8) + 1;
                envelope_value = envx;
            }

            if (envx <= (sustain_level + 1) * 0x100)
                envelope_stage = EnvelopeStage::Sustain;
            break;
        }
        case EnvelopeStage::Sustain:
            // Docs: "SR[is multiplied] by the fixed value 1-1/256."
            // Multiplying ENVX by 255/256 every time SUSTAIN is
            // updated.
            cnt -= ENVELOPE_RATES[sustain_rate];
            if (cnt <= 0) {
                cnt = ENVELOPE_RATE_INITIAL;
                envx -= ((envx - 1) >> 8) + 1;
                envelope_value = envx;
            }
            break;
        case EnvelopeStage::Release:  // handled above
            break;
    }

    envelope_counter = cnt;
    return envx >> 4;
}

int16_t Sony_S_DSP_ADSR::run(bool trigger, bool gate_on) {
    if (trigger) {  // trigger the envelope generator
        // reset the envelope value to 0 and the stage to attack
        envelope_value = 0;
        envelope_stage = EnvelopeStage::Attack;
        // NOTE: Real SNES does *not* appear to initialize the envelope
        // counter to anything in particular. The first cycle always seems to
        // come at a random time sooner than expected; as yet, I have been
        // unable to find any pattern.  I doubt it will matter though, so
        // we'll go ahead and do the full time for now.
        envelope_counter = ENVELOPE_RATE_INITIAL;
    } else if (envelope_stage == EnvelopeStage::Off) {
        return 0;
    } else if (!gate_on) {  // gate went low, move to release stage
        envelope_stage = EnvelopeStage::Release;
    }
    // clock the envelope generator and apply the global amplitude level
    const int output = clock_envelope();
    return (output * amplitude) >> 7;
}
