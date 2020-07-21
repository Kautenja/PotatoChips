// General Instrument AY-3-8910 sound chip emulator.
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

#include "general_instrument_ay_3_8910.hpp"

GeneralInstrumentAy_3_8910::GeneralInstrumentAy_3_8910() {
    // build full table of the upper 8 envelope waveforms
    for (int m = 8; m--;) {
        uint8_t* out = env.modes[m];
        int flags = MODES[m];
        for (int x = 3; --x >= 0;) {
            int amp = flags & 1;
            int end = flags >> 1 & 1;
            int step = end - amp;
            amp *= 15;
            for (int y = 16; --y >= 0;) {
                *out++ = AMP_TABLE[amp];
                amp += step;
            }
            flags >>= 2;
        }
    }

    set_output(0);
    volume(1.0);
    reset();
}

void GeneralInstrumentAy_3_8910::write_data_(int addr, int data) {
    assert((unsigned) addr < REG_COUNT);
    // envelope mode
    if (addr == 13) {
        if (!(data & 8)) // convert modes 0-7 to proper equivalents
            data = (data & 4) ? 15 : 9;
        env.wave = env.modes[data - 7];
        env.pos = -48;
        // will get set to envelope period in run_until()
        env.delay = 0;
    }
    regs[addr] = data;
    // handle period changes accurately
    int i = addr >> 1;
    if (i < OSC_COUNT) {
        blip_time_t period = (regs[i * 2 + 1] & 0x0F) * (0x100L * PERIOD_FACTOR) + regs[i * 2] * PERIOD_FACTOR;
        if (!period) period = PERIOD_FACTOR;
        // adjust time of next timer expiration based on change in period
        osc_t& osc = oscs[i];
        if ((osc.delay += period - osc.period) < 0) osc.delay = 0;
        osc.period = period;
    }
    // TODO: same as above for envelope timer, and it also has a divide by
    // two after it
}

void GeneralInstrumentAy_3_8910::run_until(blip_time_t final_end_time) {
    assert(final_end_time >= last_time);

    // noise period and initial values
    blip_time_t const noise_period_factor = PERIOD_FACTOR * 2; // verified
    blip_time_t noise_period = (regs[6] & 0x1F) * noise_period_factor;
    if (!noise_period)
        noise_period = noise_period_factor;
    blip_time_t const old_noise_delay = noise.delay;
    blargg_ulong const old_noise_lfsr = noise.lfsr;

    // envelope period
    blip_time_t const env_period_factor = PERIOD_FACTOR * 2; // verified
    blip_time_t env_period = (regs[12] * 0x100L + regs[11]) * env_period_factor;
    if (!env_period)
        env_period = env_period_factor; // same as period 1 on my AY chip
    if (!env.delay)
        env.delay = env_period;

    // run each osc separately
    for (int index = 0; index < OSC_COUNT; index++) {
        osc_t* const osc = &oscs[index];
        int osc_mode = regs[7] >> index;

        // output
        BLIPBuffer* const osc_output = osc->output;
        if (!osc_output)
            continue;

        // period
        int half_vol = 0;
        blip_time_t inaudible_period = (blargg_ulong) (osc_output->get_clock_rate() +
                INAUDIBLE_FREQ) / (INAUDIBLE_FREQ * 2);
        if (osc->period <= inaudible_period && !(osc_mode & TONE_OFF)) {
            half_vol = 1; // Actually around 60%, but 50% is close enough
            osc_mode |= TONE_OFF;
        }

        // envelope
        blip_time_t start_time = last_time;
        blip_time_t end_time   = final_end_time;
        int const vol_mode = regs[0x08 + index];
        int volume = AMP_TABLE[vol_mode & 0x0F] >> half_vol;
        int osc_env_pos = env.pos;
        if (vol_mode & 0x10) {
            volume = env.wave[osc_env_pos] >> half_vol;
            // use envelope only if it's a repeating wave or a ramp that hasn't finished
            if (!(regs[13] & 1) || osc_env_pos < -32) {
                end_time = start_time + env.delay;
                if (end_time >= final_end_time)
                    end_time = final_end_time;
            } else if (!volume) {
                osc_mode = NOISE_OFF | TONE_OFF;
            }
        } else if (!volume) {
            osc_mode = NOISE_OFF | TONE_OFF;
        }

        // tone time
        blip_time_t const period = osc->period;
        blip_time_t time = start_time + osc->delay;
        if (osc_mode & TONE_OFF) {  // maintain tone's phase when off
            blargg_long count = (final_end_time - time + period - 1) / period;
            time += count * period;
            osc->phase ^= count & 1;
        }

        // noise time
        blip_time_t ntime = final_end_time;
        blargg_ulong noise_lfsr = 1;
        if (!(osc_mode & NOISE_OFF)) {
            ntime = start_time + old_noise_delay;
            noise_lfsr = old_noise_lfsr;
        }

        // The following efficiently handles several cases (least demanding first):
        // * Tone, noise, and envelope disabled, where channel acts as 4-bit DAC
        // * Just tone or just noise, envelope disabled
        // * Envelope controlling tone and/or noise
        // * Tone and noise disabled, envelope enabled with high frequency
        // * Tone and noise together
        // * Tone and noise together with envelope

        // This loop only runs one iteration if envelope is disabled. If envelope
        // is being used as a waveform (tone and noise disabled), this loop will
        // still be reasonably efficient since the bulk of it will be skipped.
        while (1) {
            // current amplitude
            int amp = 0;
            if ((osc_mode | osc->phase) & 1 & (osc_mode >> 3 | noise_lfsr))
                amp = volume;
            {
                int delta = amp - osc->last_amp;
                if (delta) {
                    osc->last_amp = amp;
                    synth.offset(start_time, delta, osc_output);
                }
            }

            // Run wave and noise interleved with each catching up to the other.
            // If one or both are disabled, their "current time" will be past end time,
            // so there will be no significant performance hit.
            if (ntime < end_time || time < end_time) {
                // Since amplitude was updated above, delta will always be +/- volume,
                // so we can avoid using last_amp every time to calculate the delta.
                int delta = amp * 2 - volume;
                int delta_non_zero = delta != 0;
                int phase = osc->phase | (osc_mode & TONE_OFF); assert(TONE_OFF == 0x01);
                do {
                    // run noise
                    blip_time_t end = end_time;
                    if (end_time > time) end = time;
                    if (phase & delta_non_zero) {
                        while (ntime <= end) {  // must advance *past* time to avoid hang
                            int changed = noise_lfsr + 1;
                            noise_lfsr = (-(noise_lfsr & 1) & 0x12000) ^ (noise_lfsr >> 1);
                            if (changed & 2) {
                                delta = -delta;
                                synth.offset(ntime, delta, osc_output);
                            }
                            ntime += noise_period;
                        }
                    } else {
                        // 20 or more noise periods on average for some music
                        blargg_long remain = end - ntime;
                        blargg_long count = remain / noise_period;
                        if (remain >= 0)
                            ntime += noise_period + count * noise_period;
                    }

                    // run tone
                    end = end_time;
                    if (end_time > ntime) end = ntime;
                    if (noise_lfsr & delta_non_zero) {
                        while (time < end) {
                            delta = -delta;
                            synth.offset(time, delta, osc_output);
                            time += period;
                            //phase ^= 1;
                        }
                        //assert(phase == (delta > 0));
                        phase = unsigned (-delta) >> (CHAR_BIT * sizeof (unsigned) - 1);
                        // (delta > 0)
                    } else {
                        // loop usually runs less than once
                        //SUB_CASE_COUNTER((time < end) * (end - time + period - 1) / period);
                        while (time < end) {
                            time += period;
                            phase ^= 1;
                        }
                    }
                } while (time < end_time || ntime < end_time);

                osc->last_amp = (delta + volume) >> 1;
                if (!(osc_mode & TONE_OFF))
                    osc->phase = phase;
            }

            if (end_time >= final_end_time)
                break;  // breaks first time when envelope is disabled

            // next envelope step
            if (++osc_env_pos >= 0)
                osc_env_pos -= 32;
            volume = env.wave[osc_env_pos] >> half_vol;

            start_time = end_time;
            end_time += env_period;
            if (end_time > final_end_time)
                end_time = final_end_time;
        }
        osc->delay = time - final_end_time;

        if (!(osc_mode & NOISE_OFF)) {
            noise.delay = ntime - final_end_time;
            noise.lfsr = noise_lfsr;
        }
    }

    // TODO: optimized saw wave envelope?

    // maintain envelope phase
    blip_time_t remain = final_end_time - last_time - env.delay;
    if (remain >= 0) {
        blargg_long count = (remain + env_period) / env_period;
        env.pos += count;
        if (env.pos >= 0)
            env.pos = (env.pos & 31) - 32;
        remain -= count * env_period;
        assert(-remain <= env_period);
    }
    env.delay = -remain;
    assert(env.delay > 0);
    assert(env.pos < 0);

    last_time = final_end_time;
}
