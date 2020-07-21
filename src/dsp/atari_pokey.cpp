// Atari POKEY sound chip emulator
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

#include "atari_pokey.hpp"

void AtariPOKEY::run_until(blip_time_t end_time) {
    calc_periods();
    // cache
    AtariPOKEYEngine* const impl = this->impl;

    // 17/9-bit poly selection
    uint8_t const* polym = impl->poly17;
    int polym_len = poly17_len;
    if (this->control & 0x80) {
        polym_len = poly9_len;
        polym = impl->poly9;
    }
    polym_pos %= polym_len;

    for (int i = 0; i < OSC_COUNT; i++) {
        osc_t* const osc = &oscs[i];
        blip_time_t time = last_time + osc->delay;
        blip_time_t const period = osc->period;

        // output
        BLIPBuffer* output = osc->output;
        if (output) {
            int const osc_control = osc->regs[1]; // cache
            int volume = (osc_control & 0x0F) * 2;
            if (!volume || osc_control & 0x10 || // silent, DAC mode, or inaudible frequency
                    ((osc_control & 0xA0) == 0xA0 && period < 1789773 / 2 / MAX_FREQUENCY)) {
                if (!(osc_control & 0x10))
                    volume >>= 1; // inaudible frequency = half volume

                int delta = volume - osc->last_amp;
                if (delta) {
                    osc->last_amp = volume;
                    impl->synth.offset(last_time, delta, output);
                }

                // TODO: doesn't maintain high pass flip-flop (very minor issue)
            } else {
                // high pass
                static uint8_t const hipass_bits[OSC_COUNT] = { 1 << 2, 1 << 1, 0, 0 };
                blip_time_t period2 = 0; // unused if no high pass
                blip_time_t time2 = end_time;
                if (this->control & hipass_bits[i]) {
                    period2 = osc[2].period;
                    time2 = last_time + osc[2].delay;
                    if (osc->invert) {
                        // trick inner wave loop into inverting output
                        osc->last_amp -= volume;
                        volume = -volume;
                    }
                }

                if (time < end_time || time2 < end_time) {
                    // poly source
                    static uint8_t const poly1[] = { 0x55, 0x55 }; // square wave
                    uint8_t const* poly = poly1;
                    int poly_len = 8 * sizeof poly1; // can be just 2 bits, but this is faster
                    int poly_pos = osc->phase & 1;
                    int poly_inc = 1;
                    if (!(osc_control & 0x20)) {
                        poly     = polym;
                        poly_len = polym_len;
                        poly_pos = polym_pos;
                        if (osc_control & 0x40) {
                            poly     = impl->poly4;
                            poly_len = poly4_len;
                            poly_pos = poly4_pos;
                        }
                        poly_inc = period % poly_len;
                        poly_pos = (poly_pos + osc->delay) % poly_len;
                    }
                    poly_inc -= poly_len; // allows more optimized inner loop below

                    // square/poly5 wave
                    blargg_ulong wave = poly5;
                    assert(poly5 & 1); // low bit is set for pure wave
                    int poly5_inc = 0;
                    if (!(osc_control & 0x80)) {
                        wave = run_poly5(wave, (osc->delay + poly5_pos) % poly5_len);
                        poly5_inc = period % poly5_len;
                    }

                    // Run wave and high pass interleved with each catching up to the other.
                    // Disabled high pass has no performance effect since inner wave loop
                    // makes no compromise for high pass, and only runs once in that case.
                    int osc_last_amp = osc->last_amp;
                    do {
                        // run high pass
                        if (time2 < time) {
                            int delta = -osc_last_amp;
                            if (volume < 0)
                                delta += volume;
                            if (delta) {
                                osc_last_amp += delta - volume;
                                volume = -volume;
                                impl->synth.offset(time2, delta, output);
                            }
                        }
                        // must advance *past* time to avoid hang
                        while (time2 <= time) time2 += period2;
                        // run wave
                        blip_time_t end = end_time;
                        if (end > time2) end = time2;
                        while (time < end) {
                            if (wave & 1) {
                                int amp = volume & -(poly[poly_pos >> 3] >> (poly_pos & 7) & 1);
                                if ((poly_pos += poly_inc) < 0)
                                    poly_pos += poly_len;
                                int delta = amp - osc_last_amp;
                                if (delta) {
                                    osc_last_amp = amp;
                                    impl->synth.offset(time, delta, output);
                                }
                            }
                            wave = run_poly5(wave, poly5_inc);
                            time += period;
                        }
                    } while (time < end_time || time2 < end_time);

                    osc->phase = poly_pos;
                    osc->last_amp = osc_last_amp;
                }

                osc->invert = 0;
                if (volume < 0) {
                    // undo inversion trickery
                    osc->last_amp -= volume;
                    osc->invert = 1;
                }
            }
        }

        // maintain divider
        blip_time_t remain = end_time - time;
        if (remain > 0) {
            blargg_long count = (remain + period - 1) / period;
            osc->phase ^= count;
            time += count * period;
        }
        osc->delay = time - end_time;
    }

    // advance polies
    blip_time_t duration = end_time - last_time;
    last_time = end_time;
    poly4_pos = (poly4_pos + duration) % poly4_len;
    poly5_pos = (poly5_pos + duration) % poly5_len;
    // will get %'d on next call
    polym_pos += duration;
}
