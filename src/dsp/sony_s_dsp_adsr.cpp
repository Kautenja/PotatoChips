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
static const uint16_t ENVELOPE_RATES[0x20] = {
    0x0000, 0x000F, 0x0014, 0x0018, 0x001E, 0x0028, 0x0030, 0x003C,
    0x0050, 0x0060, 0x0078, 0x00A0, 0x00C0, 0x00F0, 0x0140, 0x0180,
    0x01E0, 0x0280, 0x0300, 0x03C0, 0x0500, 0x0600, 0x0780, 0x0A00,
    0x0C00, 0x0F00, 0x1400, 0x1800, 0x1E00, 0x2800, 0x3C00, 0x7800
};

inline int Sony_S_DSP_ADSR::clock_envelope(unsigned voice_idx) {
    RawVoice& raw_voice = this->voices[voice_idx];
    VoiceState& voice = voice_states[voice_idx];

    int envx = voice.envx;
    if (voice.envelope_stage == EnvelopeStage::Release) {
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
        raw_voice.envx = envx >> 8;
        return envx;
    }

    int cnt = voice.envcnt;
    int adsr1 = raw_voice.adsr[0];
    if (adsr1 & 0x80) {
        switch (voice.envelope_stage) {
            case EnvelopeStage::Attack: {
                // increase envelope by 1/64 each step
                int t = adsr1 & 15;
                if (t == 15) {
                    envx += ENVELOPE_RANGE / 2;
                } else {
                    cnt -= ENVELOPE_RATES[t * 2 + 1];
                    if (cnt > 0) break;
                    envx += ENVELOPE_RANGE / 64;
                    cnt = ENVELOPE_RATE_INITIAL;
                }
                if (envx >= ENVELOPE_RANGE) {
                    envx = ENVELOPE_RANGE - 1;
                    voice.envelope_stage = EnvelopeStage::Decay;
                }
                voice.envx = envx;
                break;
            }

            case EnvelopeStage::Decay: {
                // Docs: "DR...[is multiplied] by the fixed value
                // 1-1/256." Well, at least that makes some sense.
                // Multiplying ENVX by 255/256 every time DECAY is
                // updated.
                cnt -= ENVELOPE_RATES[((adsr1 >> 3) & 0xE) + 0x10];
                if (cnt <= 0) {
                    cnt = ENVELOPE_RATE_INITIAL;
                    envx -= ((envx - 1) >> 8) + 1;
                    voice.envx = envx;
                }
                int sustain_level = raw_voice.adsr[1] >> 5;

                if (envx <= (sustain_level + 1) * 0x100)
                    voice.envelope_stage = EnvelopeStage::Sustain;
                break;
            }

            case EnvelopeStage::Sustain:
                // Docs: "SR[is multiplied] by the fixed value 1-1/256."
                // Multiplying ENVX by 255/256 every time SUSTAIN is
                // updated.
                cnt -= ENVELOPE_RATES[raw_voice.adsr[1] & 0x1F];
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
    } else {  /* GAIN mode is set */
        // Note: if the game switches between ADSR and GAIN modes
        // partway through, should the count be reset, or should it
        // continue from where it was? Does the DSP actually watch for
        // that bit to change, or does it just go along with whatever
        // it sees when it performs the update? I'm going to assume
        // the latter and not update the count, unless I see a game
        // that obviously wants the other behavior.  The effect would
        // be pretty subtle, in any case.
        int t = raw_voice.gain;
        if (t < 0x80) {
            envx = voice.envx = t << 4;
        }
        else switch (t >> 5) {
        case 4:         /* Docs: "Decrease (linear): Subtraction
                             * of the fixed value 1/64." */
            cnt -= ENVELOPE_RATES[t & 0x1F];
            if (cnt > 0)
                break;
            cnt = ENVELOPE_RATE_INITIAL;
            envx -= ENVELOPE_RANGE / 64;
            if (envx < 0) {
                envx = 0;
                if (voice.envelope_stage == EnvelopeStage::Attack)
                    voice.envelope_stage = EnvelopeStage::Decay;
            }
            voice.envx = envx;
            break;
        case 5:         /* Docs: "Decrease <sic> (exponential):
                             * Multiplication by the fixed value
                             * 1-1/256." */
            cnt -= ENVELOPE_RATES[t & 0x1F];
            if (cnt > 0)
                break;
            cnt = ENVELOPE_RATE_INITIAL;
            envx -= ((envx - 1) >> 8) + 1;
            if (envx < 0) {
                envx = 0;
                if (voice.envelope_stage == EnvelopeStage::Attack)
                    voice.envelope_stage = EnvelopeStage::Decay;
            }
            voice.envx = envx;
            break;
        case 6:         /* Docs: "Increase (linear): Addition of
                             * the fixed value 1/64." */
            cnt -= ENVELOPE_RATES[t & 0x1F];
            if (cnt > 0)
                break;
            cnt = ENVELOPE_RATE_INITIAL;
            envx += ENVELOPE_RANGE / 64;
            if (envx >= ENVELOPE_RANGE)
                envx = ENVELOPE_RANGE - 1;
            voice.envx = envx;
            break;
        case 7:         /* Docs: "Increase (bent line): Addition
                             * of the constant 1/64 up to .75 of the
                             * constant <sic> 1/256 from .75 to 1." */
            cnt -= ENVELOPE_RATES[t & 0x1F];
            if (cnt > 0)
                break;
            cnt = ENVELOPE_RATE_INITIAL;
            if (envx < ENVELOPE_RANGE * 3 / 4)
                envx += ENVELOPE_RANGE / 64;
            else
                envx += ENVELOPE_RANGE / 256;
            if (envx >= ENVELOPE_RANGE)
                envx = ENVELOPE_RANGE - 1;
            voice.envx = envx;
            break;
        }
    }
    voice.envcnt = cnt;
    raw_voice.envx = envx >> 4;
    return envx;
}

void Sony_S_DSP_ADSR::run(int16_t* output_buffer) {
    // use the global wave page address to lookup a pointer to the first entry
    // in the source directory. the wave page is multiplied by 0x100 to produce
    // the RAM address of the source directory.
    const SourceDirectoryEntry* const source_directory =
        reinterpret_cast<SourceDirectoryEntry*>(&ram[global.wave_page * 0x100]);

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
    // Keying on a voice resets that bit in ENDX.
    // global.wave_ended &= ~global.key_ons;
    // -------------------------------------------------------------------
    // MARK: Voice Processing
    // -------------------------------------------------------------------
    // buffer the outputs of the left and right echo and main channels
    // iterate over the voices on the chip
    for (unsigned voice_idx = 0; voice_idx < VOICE_COUNT; voice_idx++) {
        // get the voice's bit-mask shift value
        const int voice_bit = 1 << voice_idx;
        // cache the voice and data structures
        RawVoice& raw_voice = voices[voice_idx];
        VoiceState& voice = voice_states[voice_idx];
        // ---------------------------------------------------------------
        // MARK: Gate Processing
        // ---------------------------------------------------------------
        if (voice.on_cnt && !--voice.on_cnt) {
            // key on
            keys |= voice_bit;
            voice.addr = source_directory[raw_voice.waveform].start;
            voice.block_remain = 1;
            voice.envx = 0;
            voice.block_header = 0;
            // decode three samples immediately
            voice.fraction = 0x3FFF;
            // BRR decoder filter uses previous two samples
            voice.interp0 = 0;
            voice.interp1 = 0;
            // NOTE: Real SNES does *not* appear to initialize the
            // envelope counter to anything in particular. The first
            // cycle always seems to come at a random time sooner than
            // expected; as yet, I have been unable to find any
            // pattern.  I doubt it will matter though, so we'll go
            // ahead and do the full time for now.
            voice.envcnt = ENVELOPE_RATE_INITIAL;
            voice.envelope_stage = EnvelopeStage::Attack;
        }
        // key-on = !key-off = true
        if (global.key_ons & voice_bit & ~global.key_offs) {
            global.key_ons &= ~voice_bit;
            voice.on_cnt = 8;
        }
        // key-off = true
        if (keys & global.key_offs & voice_bit) {
            voice.envelope_stage = EnvelopeStage::Release;
            voice.on_cnt = 0;
        }

        int envx;
        if (!(keys & voice_bit) || (envx = clock_envelope(voice_idx)) < 0) {
            raw_voice.envx = 0;
            raw_voice.outx = 0;
            continue;
        }

        // Decode samples when fraction >= 1.0 (0x1000)
        for (int n = voice.fraction >> 12; --n >= 0;) {
            if (!--voice.block_remain) {
                if (voice.block_header & 1) {
                    global.wave_ended |= voice_bit;
                    if (voice.block_header & 2) {
                        // verified (played endless looping sample and ENDX was set)
                        voice.addr = source_directory[raw_voice.waveform].loop;
                    } else {  // first block was end block; don't play anything
                        goto sample_ended;
                    }
                }
                voice.block_header = ram[voice.addr++];
                voice.block_remain = 16;  // nibbles
            }

            if (
                voice.block_remain == 9 &&
                (ram[voice.addr + 5] & 3) == 1 &&
                (voice.block_header & 3) != 3
            ) {  // next block has end flag set, this block ends early
        sample_ended:
                global.wave_ended |= voice_bit;
                keys &= ~voice_bit;
                raw_voice.envx = 0;
                voice.envx = 0;
                // add silence samples to interpolation buffer
                do {
                    voice.interp3 = voice.interp2;
                    voice.interp2 = voice.interp1;
                    voice.interp1 = voice.interp0;
                    voice.interp0 = 0;
                } while (--n >= 0);
                break;
            }
        }
    }
}
