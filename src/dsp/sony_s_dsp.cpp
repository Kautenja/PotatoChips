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

#include "sony_s_dsp.hpp"
#include "sony_s_dsp_common.hpp"

inline int Sony_S_DSP::clock_envelope(unsigned voice_idx) {
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
            voice.envx = 0;
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
                cnt -= getEnvelopeRate(t * 2 + 1);
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
            cnt -= getEnvelopeRate(((adsr1 >> 3) & 0xE) + 0x10);
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
            cnt -= getEnvelopeRate(raw_voice.adsr[1] & 0x1F);
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
    } else {  // GAIN mode is set
        // TODO: if the game switches between ADSR and GAIN modes
        // partway through, should the count be reset, or should it
        // continue from where it was? Does the DSP actually watch for
        // that bit to change, or does it just go along with whatever
        // it sees when it performs the update? I'm going to assume
        // the latter and not update the count, unless I see a game
        // that obviously wants the other behavior.  The effect would
        // be pretty subtle, in any case.
        int t = raw_voice.gain;
        if (t < 0x80)  // direct GAIN mode
            envx = voice.envx = t << 4;
        else switch (t >> 5) {
        case 4:  // Decrease (linear)
            // Subtraction of the fixed value 1 / 64.
            cnt -= getEnvelopeRate(t & 0x1F);
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
        case 5:  // Decrease (exponential)
            // Multiplication by the fixed value 1 - 1/256.
            cnt -= getEnvelopeRate(t & 0x1F);
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
        case 6:  // Increase (linear)
            // Addition of the fixed value 1/64.
            cnt -= getEnvelopeRate(t & 0x1F);
            if (cnt > 0)
                break;
            cnt = ENVELOPE_RATE_INITIAL;
            envx += ENVELOPE_RANGE / 64;
            if (envx >= ENVELOPE_RANGE)
                envx = ENVELOPE_RANGE - 1;
            voice.envx = envx;
            break;
        case 7:  // Increase (bent line)
            // Addition of the constant 1/64 up to .75 of the constant 1/256
            // from .75 to 1.
            cnt -= getEnvelopeRate(t & 0x1F);
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
    // update the envelope counter and envelope output for the voice
    voice.envcnt = cnt;
    raw_voice.envx = envx >> 4;

    return envx;
}

void Sony_S_DSP::run(int16_t* output_buffer) {
    // TODO: Should we just fill the buffer with silence? Flags won't be
    // cleared during this run so it seems it should keep resetting every
    // sample.
    if (global.flags & FLAG_MASK_RESET) reset();
    // use the global wave page address to lookup a pointer to the first entry
    // in the source directory. the wave page is multiplied by 0x100 to produce
    // the RAM address of the source directory.
    const SourceDirectoryEntry* const source_directory =
        reinterpret_cast<SourceDirectoryEntry*>(&ram[global.wave_page * 0x100]);
    // get the volume of the left channel from the global registers
    int left_volume  = global.left_volume;
    int right_volume = global.right_volume;
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
    global.wave_ended &= ~global.key_ons;
    // -------------------------------------------------------------------
    // MARK: Noise
    // -------------------------------------------------------------------
    // the `noise_enables` register is a length 8 bit-mask for enabling /
    // disabling noise for each individual voice.
    if (global.noise_enables) {  // noise enabled for at least one voice
        // update the noise period based on the index of the rate in the
        // global flags register
        noise_count -= getEnvelopeRate(global.flags & FLAG_MASK_NOISE_PERIOD);
        if (noise_count <= 0) {  // rising edge of noise generator
            // reset the noise period to the initial value
            noise_count = ENVELOPE_RATE_INITIAL;
            // the LFSR is 15-bit, shift left 1 to get the 16-bit sample
            noise_amp = static_cast<int16_t>(noise << 1);
            // update the linear feedback shift register from taps 0, 1.
            noise = (((noise << 13) ^ (noise << 14)) & 0x4000) | (noise >> 1);
            // the Galois equivalent was implemented as below, but yielded
            // poor CPU performance relative to the Fibonacci method above
            // and produced a frequency response that seemed incorrect,
            // i.e., high frequency noise had a much higher low frequency
            // response. As such, the Fibonacci implementation above is
            // the preferred route for this LFSR implementation.
            //     uint16_t noise = this->noise;
            //     noise = (noise >> 1) ^ (0x6000 & -(noise & 1));
            //     this->noise = noise;
        }
    }
    // -------------------------------------------------------------------
    // MARK: Voice Processing
    // -------------------------------------------------------------------
    // store output of the the last monophonic voice for phase modulation.
    // the output for phase modulation on voice 0 is always 0. The switch can
    // be set, but has no effect. Games like Jurassic Park set the flag, but
    // it is not known why.
    int prev_outx = 0;
    // buffer the outputs of the left and right echo and main channels
    int echol = 0;
    int echor = 0;
    int left = 0;
    int right = 0;
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
            voice.interp[0] = 0;
            voice.interp[1] = 0;
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
            prev_outx = 0;
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
                    voice.interp[3] = voice.interp[2];
                    voice.interp[2] = voice.interp[1];
                    voice.interp[1] = voice.interp[0];
                    voice.interp[0] = 0;
                } while (--n >= 0);
                break;
            }

            int delta = ram[voice.addr];
            if (voice.block_remain & 1) {
                delta <<= 4;  // use lower nibble
                voice.addr++;
            }

            // Use sign-extended upper nibble
            delta = int8_t(delta) >> 4;

            // For invalid ranges (D,E,F): if the nibble is negative,
            // the result is F000.  If positive, 0000. Nothing else
            // like previous range, etc seems to have any effect.  If
            // range is valid, do the shift normally.  Note these are
            // both shifted right once to do the filters properly, but
            // the output will be shifted back again at the end.
            int shift = voice.block_header >> 4;
            delta = (delta << shift) >> 1;
            if (shift > 0x0C) delta = (delta >> 14) & ~0x7FF;

            // One, two and three point IIR filters
            int smp1 = voice.interp[0];
            int smp2 = voice.interp[1];
            if (voice.block_header & 8) {
                delta += smp1;
                delta -= smp2 >> 1;
                if (!(voice.block_header & 4)) {
                    delta += (-smp1 - (smp1 >> 1)) >> 5;
                    delta += smp2 >> 5;
                } else {
                    delta += (-smp1 * 13) >> 7;
                    delta += (smp2 + (smp2 >> 1)) >> 4;
                }
            } else if (voice.block_header & 4) {
                delta += smp1 >> 1;
                delta += (-smp1) >> 5;
            }

            voice.interp[3] = voice.interp[2];
            voice.interp[2] = smp2;
            voice.interp[1] = smp1;
            voice.interp[0] = 2 * clamp_16(delta);
        }

        // get the 14-bit frequency value
        int rate = 0x3FFF & ((raw_voice.rate[1] << 8) | raw_voice.rate[0]);
        if (global.pitch_mods & voice_bit)
            rate = (rate * (prev_outx + 32768)) >> 15;

        // Gaussian interpolation using most recent 4 samples
        int index = voice.fraction >> 2 & 0x3FC;
        voice.fraction = (voice.fraction & 0x0FFF) + rate;
        auto table1 = getGaussian(index);
        auto table2 = getGaussian(255 * 4 - index);
        int sample = ((table1[0] * voice.interp[3]) >> 12) +
                     ((table1[1] * voice.interp[2]) >> 12) +
                     ((table2[1] * voice.interp[1]) >> 12);
        sample = static_cast<int16_t>(2 * sample);
        sample +=     (table2[0] * voice.interp[0]) >> 11 & ~1;

        // if noise is enabled for this voice use the amplified noise as
        // the output, otherwise use the clamped sampled value
        int output = (global.noise_enables & voice_bit) ?
            noise_amp : clamp_16(sample);
        // scale output and set outx values
        output = (output * envx) >> 11 & ~1;
        int l = (voice.volume[0] * output) >> 7;
        int r = (voice.volume[1] * output) >> 7;

        prev_outx = output;
        raw_voice.outx = output >> 8;
        if (global.echo_ons & voice_bit) {
            echol += l;
            echor += r;
        }
        left  += l;
        right += r;
    }
    // end of channel loop

    // main volume control
    left  = (left  * left_volume) >> 7;
    right = (right * right_volume) >> 7;

    // -------------------------------------------------------------------
    // MARK: Echo FIR filter
    // -------------------------------------------------------------------

    // get the current feedback sample in the echo buffer
    EchoBufferSample* const echo_sample =
        reinterpret_cast<EchoBufferSample*>(&ram[(global.echo_page * 0x100 + echo_ptr) & 0xFFFF]);
    // increment the echo pointer by the size of the echo buffer sample (4)
    echo_ptr += sizeof(EchoBufferSample);
    // check if for the end of the ring buffer and wrap the pointer around
    // the echo delay is clamped in [0, 15] and each delay index requires
    // 2KB of RAM (0x800)
    if (echo_ptr >= (global.echo_delay & 15) * 0x800) echo_ptr = 0;
    // cache the feedback value (sign-extended to 32-bit)
    int fb_left = echo_sample->samples[EchoBufferSample::LEFT];
    int fb_right = echo_sample->samples[EchoBufferSample::RIGHT];

    // put samples in history ring buffer
    const int fir_offset = this->fir_offset;
    short (*fir_pos)[2] = &fir_buf[fir_offset];
    this->fir_offset = (fir_offset + 7) & 7;  // move backwards one step
    fir_pos[0][0] = (short) fb_left;
    fir_pos[0][1] = (short) fb_right;
    // duplicate at +8 eliminates wrap checking below
    fir_pos[8][0] = (short) fb_left;
    fir_pos[8][1] = (short) fb_right;

    // FIR
    fb_left =     fb_left * fir_coeff[7] +
            fir_pos[1][0] * fir_coeff[6] +
            fir_pos[2][0] * fir_coeff[5] +
            fir_pos[3][0] * fir_coeff[4] +
            fir_pos[4][0] * fir_coeff[3] +
            fir_pos[5][0] * fir_coeff[2] +
            fir_pos[6][0] * fir_coeff[1] +
            fir_pos[7][0] * fir_coeff[0];

    fb_right =   fb_right * fir_coeff[7] +
            fir_pos[1][1] * fir_coeff[6] +
            fir_pos[2][1] * fir_coeff[5] +
            fir_pos[3][1] * fir_coeff[4] +
            fir_pos[4][1] * fir_coeff[3] +
            fir_pos[5][1] * fir_coeff[2] +
            fir_pos[6][1] * fir_coeff[1] +
            fir_pos[7][1] * fir_coeff[0];
    // add the echo to the samples for the left and right channel
    left  += (fb_left  * global.left_echo_volume) >> 14;
    right += (fb_right * global.right_echo_volume) >> 14;

    if (!(global.flags & FLAG_MASK_ECHO_WRITE)) {  // echo buffer feedback
        // add feedback to the echo samples
        echol += (fb_left  * global.echo_feedback) >> 14;
        echor += (fb_right * global.echo_feedback) >> 14;
        // put the echo samples into the buffer
        echo_sample->samples[EchoBufferSample::LEFT] = clamp_16(echol);
        echo_sample->samples[EchoBufferSample::RIGHT] = clamp_16(echor);
    }

    // -------------------------------------------------------------------
    // MARK: Output
    // -------------------------------------------------------------------
    if (output_buffer) {  // write final samples
        // clamp the left and right samples and place them into the buffer
        output_buffer[0] = left  = clamp_16(left);
        output_buffer[1] = right = clamp_16(right);
        // increment the buffer to the position of the next stereo sample
        if (global.flags & FLAG_MASK_MUTE)  // muting
            output_buffer[0] = output_buffer[1] = 0;
    }
}
