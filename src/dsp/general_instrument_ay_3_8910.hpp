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

#ifndef DSP_GENERAL_INSTRUMENT_AY_3_8910_HPP_
#define DSP_GENERAL_INSTRUMENT_AY_3_8910_HPP_

#include "blargg_common.h"
#include "blip_buffer.hpp"

/// @brief General Instrument AY-3-8910 sound chip emulator.
/// @details
/// Emulation inaccuracies:
/// -   Noise isn't run when not in use
/// -   Changes to envelope and noise periods are delayed until next reload
/// -   Super-sonic tone should attenuate output to about 60%, not 50%
///
class GeneralInstrumentAy_3_8910 {
 public:
    /// the number of oscillators on the chip
    enum { OSC_COUNT = 3 };
    /// the number of registers on the chip
    enum { REG_COUNT = 16 };
    /// the range of the amplifier on the chip
    enum { AMP_RANGE = 255 };

    /// the register on the chip
    enum Registers {
        PERIOD_CH_A_LO,   // the low 8 bits of the 12 bit frequency for channel A
        PERIOD_CH_A_HI,   // the high 4 bits of the 12 bit frequency for channel A
        PERIOD_CH_B_LO,   // the low 8 bits of the 12 bit frequency for channel B
        PERIOD_CH_B_HI,   // the high 4 bits of the 12 bit frequency for channel B
        PERIOD_CH_C_LO,   // the low 8 bits of the 12 bit frequency for channel C
        PERIOD_CH_C_HI,   // the high 4 bits of the 12 bit frequency for channel C
        NOISE_PERIOD,     // the 5-bit noise period
        CHANNEL_ENABLES,  // the 8-bit control register
        VOLUME_CH_A,
        VOLUME_CH_B,
        VOLUME_CH_C,
        PERIOD_ENVELOPE_LO,
        PERIOD_ENVELOPE_HI,
        ENVELOPE_SHAPE,
        // IO_PORT_A,  // unused
        // IO_PORT_B   // unused
    };

    /// the flag bit for turning on the envelope for a channel's VOLUME_CH_#
    /// register
    static constexpr int PERIOD_CH_ENVELOPE_ON = 0b00010000;

    enum ChannelEnableFlag {
        CHANNEL_ENABLE_ALL_ON      = 0b00000000,
        CHANNEL_ENABLE_TONE_A_OFF  = 0b00000001,
        CHANNEL_ENABLE_TONE_B_OFF  = 0b00000010,
        CHANNEL_ENABLE_TONE_C_OFF  = 0b00000100,
        CHANNEL_ENABLE_NOISE_A_OFF = 0b00001000,
        CHANNEL_ENABLE_NOISE_B_OFF = 0b00010000,
        CHANNEL_ENABLE_NOISE_C_OFF = 0b00100000,
        // CHANNEL_ENABLE_PORT_A_OFF  = 0b01000000,  // unused
        // CHANNEL_ENABLE_PORT_B_OFF  = 0b10000000   // unused
    };

    // flags for the ENVELOPE_SHAPE register
    enum EnvelopeShapeFlag {
        ENVELOPE_SHAPE_NONE      = 0b0000,
        ENVELOPE_SHAPE_HOLD      = 0b0001,
        ENVELOPE_SHAPE_ALTERNATE = 0b0010,
        ENVELOPE_SHAPE_ATTACK    = 0b0100,
        ENVELOPE_SHAPE_CONTINUE  = 0b1000,
    };

 private:
    /// TODO:
    static constexpr int PERIOD_FACTOR = 16;
    /// the number of bits to shift for faster multiplying / dividing by
    /// PERIOD_FACTOR (which is a factor of 2)
    static constexpr int PERIOD_SHIFTS = 4;
    /// Tones above this frequency are treated as disabled tone at half volume.
    /// Power of two is more efficient (avoids division).
    static constexpr unsigned INAUDIBLE_FREQ = 16384;

    /// With channels tied together and 1K resistor to ground (as datasheet
    /// recommends), output nearly matches logarithmic curve as claimed. Approx.
    /// 1.5 dB per step.
    static constexpr uint8_t AMP_TABLE[16] = {
    #define ENTRY(n) uint8_t (n * GeneralInstrumentAy_3_8910::AMP_RANGE + 0.5)
        ENTRY(0.000000), ENTRY(0.007813), ENTRY(0.011049), ENTRY(0.015625),
        ENTRY(0.022097), ENTRY(0.031250), ENTRY(0.044194), ENTRY(0.062500),
        ENTRY(0.088388), ENTRY(0.125000), ENTRY(0.176777), ENTRY(0.250000),
        ENTRY(0.353553), ENTRY(0.500000), ENTRY(0.707107), ENTRY(1.000000),
    #undef ENTRY
    };

    /// TODO:
    static constexpr uint8_t MODES[8] = {
    #define MODE(a0,a1, b0,b1, c0,c1) (a0 | a1<<1 | b0<<2 | b1<<3 | c0<<4 | c1<<5)
        MODE(1,0, 1,0, 1,0),
        MODE(1,0, 0,0, 0,0),
        MODE(1,0, 0,1, 1,0),
        MODE(1,0, 1,1, 1,1),
        MODE(0,1, 0,1, 0,1),
        MODE(0,1, 1,1, 1,1),
        MODE(0,1, 1,0, 0,1),
        MODE(0,1, 0,0, 0,0),
    #undef MODE
    };

    /// the noise off flag bit
    static constexpr int NOISE_OFF    = 0x08;
    /// the tone off flag bit
    static constexpr int TONE_OFF     = 0x01;

    /// the oscillator type on the chip for the 5 channels
    struct osc_t {
        /// the period of the oscillator
        blip_time_t period;
        /// TODO:
        blip_time_t delay;
        /// the value of the last output from the oscillator
        short last_amp;
        /// the current phase of the oscillator
        short phase;
        /// the buffer the oscillator writes samples to
        BLIPBuffer* output;
    } oscs[OSC_COUNT];
    /// the synthesizer shared by the 5 oscillator channels
    BLIPSynth<blip_good_quality, 1> synth;
    /// the last time the oscillators were updated
    blip_time_t last_time;
    /// the registers on teh chip
    uint8_t regs[REG_COUNT];

    /// TODO:
    struct {
        /// TODO:
        blip_time_t delay;
        /// TODO:
        blargg_ulong lfsr;
    } noise;

    /// TODO:
    struct {
        /// TODO:
        blip_time_t delay;
        /// TODO:
        uint8_t const* wave;
        /// TODO:
        int pos;
        /// values already passed through volume table
        uint8_t modes[8][48];
    } env;

    /// Write to the data port.
    ///
    /// @param addr the address to write the data to
    /// @param data the data to write to the given address
    ///
    void _write(int addr, int data) {
        assert((unsigned) addr < REG_COUNT);
        if (addr == 13) {  // envelope mode
            if (!(data & 8)) // convert modes 0-7 to proper equivalents
                data = (data & 4) ? 15 : 9;
            env.wave = env.modes[data - 7];
            env.pos = -48;
            // will get set to envelope period in run_until()
            env.delay = 0;
        }
        regs[addr] = data;
        // handle period changes accurately
        // get the oscillator index by dividing by 2. there are two registers
        // for each oscillator to represent the 12-bit period across a 16-bit
        // value (with 4 unused bits)
        int i = addr >> 1;
        if (i < OSC_COUNT) {  // i refers to i'th oscillator's period registers
            // get the period from the two registers. the first register
            // contains the low 8 bits and the second register contains the
            // high 4 bits
            blip_time_t period = ((regs[(i << 1) + 1] & 0x0F) << 8) | regs[i << 1];
            // multiply by PERIOD_FACTOR to calculate the internal period value
            period <<= PERIOD_SHIFTS;
            // if the period is zero, set to the minimal value of PERIOD_FACTOR
            if (!period) period = PERIOD_FACTOR;
            // adjust time of next timer expiration based on change in period
            osc_t& osc = oscs[i];
            if ((osc.delay += period - osc.period) < 0) osc.delay = 0;
            osc.period = period;
        }
        // TODO: same as above for envelope timer, and it also has a divide by
        // two after it
    }

    /// Run the oscillators until the given end time.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    void run_until(blip_time_t final_end_time) {
        assert(final_end_time >= last_time);

        // noise period and initial values
        blip_time_t const noise_period_factor = PERIOD_FACTOR * 2; // verified
        blip_time_t noise_period = (regs[NOISE_PERIOD] & 0x1F) * noise_period_factor;
        if (!noise_period)
            noise_period = noise_period_factor;
        blip_time_t const old_noise_delay = noise.delay;
        blargg_ulong const old_noise_lfsr = noise.lfsr;

        // envelope period
        blip_time_t const env_period_factor = PERIOD_FACTOR * 2; // verified
        blip_time_t env_period = (regs[PERIOD_ENVELOPE_HI] * 0x100L + regs[PERIOD_ENVELOPE_LO]) * env_period_factor;
        if (!env_period)
            env_period = env_period_factor; // same as period 1 on my AY chip
        if (!env.delay)
            env.delay = env_period;

        // run each osc separately
        for (int index = 0; index < OSC_COUNT; index++) {
            osc_t* const osc = &oscs[index];
            int osc_mode = regs[CHANNEL_ENABLES] >> index;

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
                if (!(regs[ENVELOPE_SHAPE] & 1) || osc_env_pos < -32) {
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

 public:
    /// Initialize a new General Instrument AY-3-8910.
    GeneralInstrumentAy_3_8910() {
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

    /// Set overall volume of all oscillators, where 1.0 is full volume
    ///
    /// @param level the value to set the volume to
    ///
    inline void volume(double v) {
        synth.volume(0.7 / OSC_COUNT / AMP_RANGE * v);
    }

    /// Set treble equalization for the synthesizers.
    ///
    /// @param equalizer the equalization parameter for the synthesizers
    ///
    inline void treble_eq(blip_eq_t const& eq) {
        synth.treble_eq(eq);
    }

    /// Assign single oscillator output to buffer. If buffer is NULL, silences
    /// the given oscillator.
    ///
    /// @param index the index of the oscillator to set the output for
    /// @param buffer the BLIPBuffer to output the given voice to
    /// @returns 0 if the output was set successfully, 1 if the index is invalid
    ///
    inline void set_output(int index, BLIPBuffer* buffer) {
        assert((unsigned) index < OSC_COUNT);
        oscs[index].output = buffer;
    }

    /// Assign all oscillator outputs to specified buffer. If buffer
    /// is NULL, silences all oscillators.
    ///
    /// @param buffer the BLIPBuffer to output the all the voices to
    ///
    inline void set_output(BLIPBuffer* buffer) {
        for (int i = 0; i < OSC_COUNT; i++) set_output(i, buffer);
    }

    /// Reset oscillators and internal state.
    inline void reset() {
        last_time   = 0;
        noise.delay = 0;
        noise.lfsr  = 1;

        osc_t* osc = &oscs[OSC_COUNT];
        do {
            osc--;
            osc->period   = PERIOD_FACTOR;
            osc->delay    = 0;
            osc->last_amp = 0;
            osc->phase    = 0;
        } while (osc != oscs);

        for (int i = sizeof regs; --i >= 0;) regs[i] = 0;
        regs[CHANNEL_ENABLES] = 0xFF;
        _write(13, 0);
    }

    /// Write to the data port.
    ///
    /// @param data the byte to write to the data port
    ///
    inline void write(int addr, int data) {
        run_until(0);
        _write(addr, data);
    }

    /// Run all oscillators up to specified time, end current frame, then
    /// start a new frame at time 0.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    inline void end_frame(blip_time_t time) {
        if (time > last_time) run_until(time);
        assert(last_time >= time);
        last_time -= time;
    }
};

#endif  // DSP_GENERAL_INSTRUMENT_AY_3_8910_HPP_
