// Individual oscillators based on the NES 2A03 synthesis chip.
// Copyright 2020 Christian Kauten
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

#ifndef NES_OSCILLATORS_HPP
#define NES_OSCILLATORS_HPP

#include <cstdint>
#include <functional>
#include "blip_buffer/blip_buffer.hpp"
#include "blip_buffer/blip_synth.hpp"

/// CPU clock cycle count
typedef int32_t cpu_time_t;
/// 16-bit memory address
typedef int16_t cpu_addr_t;

/// An abstract base type for NES oscillators.
struct Oscillator {
    unsigned char regs[4];
    bool reg_written[4];
    BLIPBuffer* output;
    /// length counter (0 if unused by oscillator)
    int length_counter;
    /// delay until next (potential) transition
    int delay;
    /// last amplitude oscillator was outputting
    int last_amp;

    inline void clock_length(int halt_mask) {
        if (length_counter && !(regs[0] & halt_mask)) length_counter--;
    }

    inline int period() const {
        return (regs[3] & 7) * 0x100 + (regs[2] & 0xff);
    }

    inline void reset() {
        delay = 0;
        last_amp = 0;
    }

    inline int update_amp(int amp) {
        int delta = amp - last_amp;
        last_amp = amp;
        return delta;
    }
};

/// An envelope-based NES oscillator
struct Envelope : Oscillator {
    int envelope;
    int env_delay;

    void clock_envelope() {
        int period = regs[0] & 15;
        if (reg_written[3]) {
            reg_written[3] = false;
            env_delay = period;
            envelope = 15;
        } else if (--env_delay < 0) {
            env_delay = period;
            if (envelope | (regs[0] & 0x20))
                envelope = (envelope - 1) & 15;
        }
    }

    inline int volume() const {
        return length_counter == 0 ?
            0 : (regs[0] & 0x10) ? (regs[0] & 15) : envelope;
    }

    inline void reset() {
        envelope = 0;
        env_delay = 0;
        Oscillator::reset();
    }
};

/// The square wave oscillator from the NES.
struct Pulse : Envelope {
    enum { negate_flag = 0x08 };
    enum { shift_mask = 0x07 };
    enum { phase_range = 8 };
    int phase;
    int sweep_delay;

    typedef BLIPSynth<BLIPQuality::Good, 15> Synth;
    // shared between squares
    const Synth* synth;

    void clock_sweep(int negative_adjust) {
        int sweep = regs[1];

        if (--sweep_delay < 0) {
            reg_written[1] = true;

            int period = this->period();
            int shift = sweep & shift_mask;
            if (shift && (sweep & 0x80) && period >= 8) {
                int offset = period >> shift;

                if (sweep & negate_flag)
                    offset = negative_adjust - offset;

                if (period + offset < 0x800) {
                    period += offset;
                    // rewrite period
                    regs[2] = period & 0xff;
                    regs[3] = (regs[3] & ~7) | ((period >> 8) & 7);
                }
            }
        }

        if (reg_written[1]) {
            reg_written[1] = false;
            sweep_delay = (sweep >> 4) & 7;
        }
    }

    void run(cpu_time_t time, cpu_time_t end_time) {
        if (!output) return;

        const int volume = this->volume();
        const int period = this->period();
        int offset = period >> (regs[1] & shift_mask);
        if (regs[1] & negate_flag)
            offset = 0;

        const int timer_period = (period + 1) * 2;
        if (volume == 0 || period < 8 || (period + offset) >= 0x800) {
            if (last_amp) {
                synth->offset(time, -last_amp, output);
                last_amp = 0;
            }
            time += delay;
            if (time < end_time) {
                // maintain proper phase
                int count = (end_time - time + timer_period - 1) / timer_period;
                phase = (phase + count) & (phase_range - 1);
                time += static_cast<cpu_time_t>(count * timer_period);
            }
        } else {
            // handle duty select
            int duty_select = (regs[0] >> 6) & 3;
            int duty = 1 << duty_select;  // 1, 2, 4, 2
            int amp = 0;
            if (duty_select == 3) {
                duty = 2;  // negated 25%
                amp = volume;
            }
            if (phase < duty)
                amp ^= volume;

            int delta = update_amp(amp);
            if (delta)
                synth->offset(time, delta, output);

            time += delay;
            if (time < end_time) {
                BLIPBuffer* const output = this->output;
                const Synth* synth = this->synth;
                int delta = amp * 2 - volume;
                int phase = this->phase;

                do {
                    phase = (phase + 1) & (phase_range - 1);
                    if (phase == 0 || phase == duty) {
                        delta = -delta;
                        synth->offset_inline(time, delta, output);
                    }
                    time += timer_period;
                } while (time < end_time);

                last_amp = (delta + volume) >> 1;
                this->phase = phase;
            }
        }
        delay = time - end_time;
    }

    inline void reset() {
        sweep_delay = 0;
        Envelope::reset();
    }
};

/// The quantized triangle wave oscillator from the NES.
struct Triangle : Oscillator {
    enum { phase_range = 16 };
    int phase;
    int linear_counter;
    BLIPSynth<BLIPQuality::Good, 15> synth;

    inline int calc_amp() const {
        int amp = phase_range - phase;
        if (amp < 0)
            amp = phase - (phase_range + 1);
        return amp;
    }

    void run(cpu_time_t time, cpu_time_t end_time) {
        if (!output) return;

        // TODO: track phase when period < 3
        // TODO: Output 7.5 on dac when period < 2? More accurate,
        //       but results in more clicks.

        int delta = update_amp(calc_amp());
        if (delta)
            synth.offset(time, delta, output);

        time += delay;
        const int timer_period = period() + 1;
        if (length_counter == 0 || linear_counter == 0 || timer_period < 3) {
            time = end_time;
        } else if (time < end_time) {
            BLIPBuffer* const output = this->output;

            int phase = this->phase;
            int volume = 1;
            if (phase > phase_range) {
                phase -= phase_range;
                volume = -volume;
            }

            do {
                if (--phase == 0) {
                    phase = phase_range;
                    volume = -volume;
                } else {
                    synth.offset_inline(time, volume, output);
                }

                time += timer_period;
            } while (time < end_time);

            if (volume < 0) phase += phase_range;
            this->phase = phase;
            last_amp = calc_amp();
        }
        delay = time - end_time;
    }

    void clock_linear_counter() {
        if (reg_written[3])
            linear_counter = regs[0] & 0x7f;
        else if (linear_counter)
            linear_counter--;

        if (!(regs[0] & 0x80))
            reg_written[3] = false;
    }

    inline void reset() {
        linear_counter = 0;
        phase = phase_range;
        Oscillator::reset();
    }
};

static constexpr int16_t noise_period_table[16] = {
    0x004, 0x008, 0x010, 0x020, 0x040, 0x060, 0x080, 0x0A0,
    0x0CA, 0x0FE, 0x17C, 0x1FC, 0x2FA, 0x3F8, 0x7F2, 0xFE4
};

/// The noise oscillator from the NES.
struct Noise : Envelope {
    int noise;
    BLIPSynth<BLIPQuality::Medium, 15> synth;

    void run(cpu_time_t time, cpu_time_t end_time) {
        if (!output) return;

        const int volume = this->volume();
        int amp = (noise & 1) ? volume : 0;
        int delta = update_amp(amp);
        if (delta)
            synth.offset(time, delta, output);

        time += delay;
        if (time < end_time) {
            constexpr int mode_flag = 0x80;

            int period = noise_period_table[regs[2] & 15];
            if (!volume) {
                // round to next multiple of period
                time += (end_time - time + period - 1) / period * period;

                // approximate noise cycling while muted, by shuffling up noise
                // register
                // TODO: precise muted noise cycling?
                if (!(regs[2] & mode_flag)) {
                    int feedback = (noise << 13) ^ (noise << 14);
                    noise = (feedback & 0x4000) | (noise >> 1);
                }
            } else {
                BLIPBuffer* const output = this->output;

                // using re-sampled time avoids conversion in synth.offset()
                auto rperiod = output->resampled_duration(period);
                auto rtime = output->resampled_time(time);

                int noise = this->noise;
                int delta = amp * 2 - volume;
                const int tap = (regs[2] & mode_flag ? 8 : 13);

                do {
                    int feedback = (noise << tap) ^ (noise << 14);
                    time += period;

                    if ((noise + 1) & 2) {
                        // bits 0 and 1 of noise differ
                        delta = -delta;
                        synth.offset_resampled(rtime, delta, output);
                    }

                    rtime += rperiod;
                    noise = (feedback & 0x4000) | (noise >> 1);
                } while (time < end_time);

                last_amp = (delta + volume) >> 1;
                this->noise = noise;
            }
        }
        delay = time - end_time;
    }

    inline void reset() {
        noise = 1 << 14;
        Envelope::reset();
    }
};

#endif  // NES_OSCILLATORS_HPP
