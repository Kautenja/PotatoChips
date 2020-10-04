// Sony SPC700 emulator.
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
const int ENVELOPE_RANGE = 0x800;

/// This table is for envelope timing global.  It represents the number of
/// counts that should be subtracted from the counter each sample period
/// (32kHz). The counter starts at 30720 (0x7800). Each count divides exactly
/// into 0x7800 without remainder.
static const short ENVELOPE_RATES[0x20] = {
    0x0000, 0x000F, 0x0014, 0x0018, 0x001E, 0x0028, 0x0030, 0x003C,
    0x0050, 0x0060, 0x0078, 0x00A0, 0x00C0, 0x00F0, 0x0140, 0x0180,
    0x01E0, 0x0280, 0x0300, 0x03C0, 0x0500, 0x0600, 0x0780, 0x0A00,
    0x0C00, 0x0F00, 0x1400, 0x1800, 0x1E00, 0x2800, 0x3C00, 0x7800
};

inline int Sony_S_DSP_ADSR::clock_envelope() {
    unsigned voice_idx = 0;
    VoiceState& voice = voice_states[voice_idx];

    int envx = voice.envx;
    if (envelope_stage == EnvelopeStage::Release) {
        // Docs: "When in the state of "key off". the "click" sound is
        // prevented by the addition of the fixed value 1/256" WTF???
        // Alright, I'm going to choose to interpret that this way:
        // When a note is keyed off, start the RELEASE state, which
        // subtracts 1/256th each sample period (32kHz).  Note there's
        // no need for a count because it always happens every update.
        envx -= ENVELOPE_RANGE / 256;
        if (envx <= 0) {
            keys &= ~(1 << voice_idx);
            return -1;
        }
        voice.envx = envx;
        envelope_output = envx >> 8;
        return envx;
    }

    int cnt = voice.envcnt;
    switch (envelope_stage) {
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
            voice.envx = envx;
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
                voice.envx = envx;
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
                voice.envx = envx;
            }
            break;

        case EnvelopeStage::Release:
            // handled above
            break;
    }

    voice.envcnt = cnt;
    envelope_output = envx >> 4;
    return envx;
}

int16_t Sony_S_DSP_ADSR::run() {
    // -------------------------------------------------------------------
    // MARK: Key Off / Key On
    // -------------------------------------------------------------------
    // Here we check for keys on / off. Docs say that successive writes
    // to KON / KOF must be separated by at least 2 T_s periods or risk
    // being neglected.  Therefore, DSP only looks at these during an
    // update, and not at the time of the write. Only need to do this
    // once however, since the regs haven't changed over the whole
    // period we need to catch up with.
    // -------------------------------------------------------------------
    // unsigned voice_idx = 0;

    // get the voice's bit-mask shift value
    const int voice_bit = 1;
    // cache the voice and data structures
    VoiceState& voice = voice_states[0];
    // key-on
    if (voice.on_cnt && !--voice.on_cnt) {
        keys |= voice_bit;
        voice.envx = 0;
        // NOTE: Real SNES does *not* appear to initialize the
        // envelope counter to anything in particular. The first
        // cycle always seems to come at a random time sooner than
        // expected; as yet, I have been unable to find any
        // pattern.  I doubt it will matter though, so we'll go
        // ahead and do the full time for now.
        voice.envcnt = ENVELOPE_RATE_INITIAL;
        envelope_stage = EnvelopeStage::Attack;
    }
    // key-on = !key-off = true
    if (global.key_ons & voice_bit & ~global.key_offs) {
        global.key_ons &= ~voice_bit;
        voice.on_cnt = 8;
    }
    // key-off = true
    if (keys & global.key_offs & voice_bit) {
        envelope_stage = EnvelopeStage::Release;
        voice.on_cnt = 0;
    }
    // clock envelope
    if (!(keys & voice_bit) || clock_envelope() < 0)
        envelope_output = 0;

    return envelope_output;
}
