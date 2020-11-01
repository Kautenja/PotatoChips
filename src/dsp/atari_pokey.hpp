// Atari POKEY sound chip emulator.
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

#ifndef DSP_ATARI_POKEY_HPP_
#define DSP_ATARI_POKEY_HPP_

#include "blip_buffer.hpp"
#include "exceptions.hpp"

/// TODO:
///
/// @param mask TODO:
/// @param count the number of samples to write
/// @param out the output buffer to write samples to
///
static void gen_poly(uint32_t mask, int count, uint8_t* out) {
    uint32_t n = 1;
    do {
        int bits = 0;
        int b = 0;
        do {  // implemented using Galois configuration
            bits |= (n & 1) << b;
            n = (n >> 1) ^ (mask & -(n & 1));
        } while (b++ < 7);
        *out++ = bits;
    } while (--count);
}

/// TODO:
static constexpr int poly5_len = (1 <<  5) - 1;
/// TODO:
static constexpr uint32_t poly5_mask = (1UL << poly5_len) - 1;
/// TODO:
static constexpr uint32_t poly5 = 0x167C6EA1;

/// TODO:
inline uint32_t run_poly5(uint32_t in, int shift) {
    return (in << shift & poly5_mask) | (in >> (poly5_len - shift));
}

/// TODO:
#define POLY_MASK(width, tap1, tap2) \
    ((1UL << (width - 1 - tap1)) | (1UL << (width - 1 - tap2)))

/// @brief Atari POKEY sound chip emulator.
class AtariPOKEY {
 public:
    /// the number of oscillators on the chip
    static constexpr unsigned OSC_COUNT = 4;
    /// the start address of the RAM on the chip
    static constexpr uint16_t ADDR_START = 0xD200;
    /// the end address of the RAM on the chip
    static constexpr uint16_t ADDR_END = 0xD209;
    /// the number of registers on the chip
    static constexpr uint16_t NUM_REGISTERS = ADDR_END - ADDR_START;

    /// the indexes of the channels on the chip
    enum Channel {
        PULSE0,
        PULSE1,
        PULSE2,
        PULSE3
    };

    /// the registers on the POKEY
    enum Register : uint16_t {
        /// the frequency of oscillator 1
        AUDF1  = 0xD200,
        /// the volume and distortion of oscillator 1
        AUDC1  = 0xD201,
        /// the frequency of oscillator 2
        AUDF2  = 0xD202,
        /// the volume and distortion of oscillator 2
        AUDC2  = 0xD203,
        /// the frequency of oscillator 3
        AUDF3  = 0xD204,
        /// the volume and distortion of oscillator 3
        AUDC3  = 0xD205,
        /// the frequency of oscillator 4
        AUDF4  = 0xD206,
        /// the volume and distortion of oscillator 4
        AUDC4  = 0xD207,
        /// the control register for global features
        AUDCTL = 0xD208,
        /// character base address register, resets delays to 0
        STIMER = 0xD209,
        /// Serial port 4 key control (TODO: see `write` for note on 0xD20F)
        // SKCTLS = 0xD20F
    };

    /// the number of registers per voice on the chip
    static constexpr unsigned REGS_PER_VOICE = 2;

    /// the number of registers per voice on the chip
    static constexpr unsigned CTL_FLAGS = 8;

    /// TODO:
    static constexpr int poly4_len = (1L <<  4) - 1;
    /// TODO:
    static constexpr int poly9_len = (1L <<  9) - 1;
    /// TODO:
    static constexpr int poly17_len = (1L << 17) - 1;

    /// Common tables and BLIPSynthesizer that can be shared among AtariPOKEY objects.
    class Engine {
     private:
        /// TODO:
        uint8_t poly4[poly4_len  / 8 + 1];
        /// TODO:
        uint8_t poly9[poly9_len  / 8 + 1];
        /// TODO:
        uint8_t poly17[poly17_len / 8 + 1];

        /// the synthesizer for the Atari POKEY engine
        BLIPSynthesizer<BLIP_QUALITY_GOOD, 1> synth;

        // friend the container class to access member data
        friend class AtariPOKEY;

     public:
        /// Initialize a new Atari POKEY engine data structure.
        Engine() {
            gen_poly(POLY_MASK( 4, 1, 0), sizeof poly4,  poly4 );
            gen_poly(POLY_MASK( 9, 5, 0), sizeof poly9,  poly9 );
            gen_poly(POLY_MASK(17, 5, 0), sizeof poly17, poly17);
            // comment out to recalculate poly5 constant
            // uint8_t poly5[4];
            // gen_poly(POLY_MASK( 5, 2, 0), sizeof poly5,  poly5 );
            // uint32_t n = poly5[3] * 0x1000000L + poly5[2] * 0x10000L + poly5[1] * 0x100L + poly5[0];
            // uint32_t rev = n & 1;
            // for (int i = 1; i < poly5_len; i++)
            //     rev |= (n >> i & 1) << (poly5_len - i);
            set_volume(1.0);
        }

        /// Set the volume of the synthesizer, where 1.0 is full volume.
        ///
        /// @param level the value to set the volume to
        ///
        inline void set_volume(double level = 1.0) {
            synth.set_volume(1.0 / OSC_COUNT / 30 * level);
        }

        /// @brief Set treble equalization for the synthesizers.
        ///
        /// @param equalizer the equalization parameter for the synthesizers
        ///
        inline void set_treble_eq(const BLIPEqualizer& equalizer) {
            synth.set_treble_eq(equalizer);
        }
    };

 private:
    /// pure waves above this frequency are silenced
    static constexpr int MAX_FREQUENCY = 12000;
    // the clock rate the chip runs at
    static constexpr int CLOCK_RATE = 1789773;
    // the maximal period for an oscillator
    static constexpr int MAX_PERIOD = CLOCK_RATE / 2 / MAX_FREQUENCY;

    /// a pulse oscillator on the Atari POKEY chip.
    struct Oscillator {
        /// the registers for the oscillator data
        uint8_t regs[2] = {0, 0};
        /// the phase of the oscillators
        uint8_t phase = 0;
        /// TODO:
        uint8_t invert = 0;
        /// the last amplitude value of the oscillator
        int last_amp = 0;
        /// TODO:
        blip_time_t delay = 0;
        /// always recalculated before use; here for convenience
        blip_time_t period = 0;
        /// the output buffer the oscillator writes samples to
        BLIPBuffer* output = nullptr;

        /// @brief Reset the oscillator to its initial state.
        ///
        /// @details
        /// This function will not overwrite the current output buffer.
        ///
        inline void reset() {
            regs[0] = regs[1] = phase = invert = last_amp = delay = period = 0;
        }
    } oscs[OSC_COUNT];

    /// the synthesizer implementation for computing samples
    Engine* impl = nullptr;
    /// has been run until this time in current frame
    blip_time_t last_time = 0;
    /// the position in Poly5
    int poly5_pos = 0;
    /// the position in Poly4
    int poly4_pos = 0;
    /// the position in PolyM
    int polym_pos = 0;
    /// the control register
    int control = 0;

    /// Calculate the periods of the oscillators on the chip.
    inline void calc_periods() {
         // 15/64 kHz clock
        int divider = 28;
        if (this->control & 1)
            divider = 114;

        for (unsigned i = 0; i < OSC_COUNT; i++) {
            auto* const osc = &oscs[i];
            // cache
            int const osc_reload = osc->regs[0];
            int32_t period = (osc_reload + 1) * divider;
            static uint8_t const fast_bits[OSC_COUNT] = { 1 << 6, 1 << 4, 1 << 5, 1 << 3 };
            if (this->control & fast_bits[i]) {
                period = osc_reload + 4;
                if (i & 1) {
                    period = osc_reload * 0x100L + osc[-1].regs[0] + 7;
                    if (!(this->control & fast_bits[i - 1]))
                        period = (period - 6) * divider;
                }
            }
            osc->period = period;
        }
    }

    /// @brief Run emulator until specified time, so that any DMC memory reads
    /// can be accounted for (i.e. inserting CPU wait states).
    ///
    /// @param end_time the number of elapsed cycles
    ///
    void run_until(blip_time_t end_time) {
        if (end_time < last_time)  // invalid end time
            throw Exception("final_end_time must be >= last_time");
        else if (end_time == last_time)  // no change in time
            return;

        calc_periods();

        // 17/9-bit poly selection
        uint8_t const* polym = impl->poly17;
        int polym_len = poly17_len;
        if (this->control & 0x80) {
            polym_len = poly9_len;
            polym = impl->poly9;
        }
        polym_pos %= polym_len;

        for (unsigned i = 0; i < OSC_COUNT; i++) {
            auto* const osc = &oscs[i];
            blip_time_t time = last_time + osc->delay;
            blip_time_t const period = osc->period;
            if (osc->output) {
                uint8_t const osc_control = osc->regs[1];
                int8_t volume = (osc_control & 0x0F) << 1;
                // silent, DAC mode, or inaudible frequency
                if (
                    !volume ||
                    osc_control & 0x10 ||
                    ((osc_control & 0xA0) == 0xA0 && period < MAX_PERIOD)
                ) {
                    // inaudible frequency = half volume
                    if (!(osc_control & 0x10)) volume >>= 1;
                    // calculate the change in amplitude
                    int delta = volume - osc->last_amp;
                    if (delta) {  // if the amplitude changed, update the synth
                        osc->last_amp = volume;
                        impl->synth.offset(last_time, delta, osc->output);
                    }
                    // TODO: doesn't maintain high pass flip-flop (minor issue)
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
                        uint32_t wave = poly5;
                        // assert(poly5 & 1);  // low bit is set for pure wave
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
                                    impl->synth.offset(time2, delta, osc->output);
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
                                        impl->synth.offset(time, delta, osc->output);
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
                int32_t count = (remain + period - 1) / period;
                osc->phase ^= count;
                time += count * period;
            }
            osc->delay = time - end_time;
        }

        // advance polys
        blip_time_t duration = end_time - last_time;
        last_time = end_time;
        poly4_pos = (poly4_pos + duration) % poly4_len;
        poly5_pos = (poly5_pos + duration) % poly5_len;
        // will get %'d on next call
        polym_pos += duration;
    }

 public:
    /// Initialize a new Atari POKEY chip emulator.
    ///
    /// @param engine_ the engine to initialize the POKEY with. if nullptr, a
    /// new engine is created for this POKEY instance
    ///
    explicit AtariPOKEY(Engine* engine_ = nullptr) {
        set_output(NULL);
        reset(engine_ == nullptr ? new Engine : engine_);
    }

    /// @brief Assign single oscillator output to buffer. If buffer is NULL,
    /// silences the given oscillator.
    ///
    /// @param channel the index of the oscillator to set the output for
    /// @param buffer the BLIPBuffer to output the given voice to
    /// @returns 0 if the output was set successfully, 1 if the index is invalid
    ///
    inline void set_output(unsigned channel, BLIPBuffer* buffer) {
        if (channel >= OSC_COUNT)  // make sure the channel is within bounds
            throw ChannelOutOfBoundsException(channel, OSC_COUNT);
        oscs[channel].output = buffer;
    }

    /// @brief Assign all oscillator outputs to specified buffer. If buffer
    /// is NULL, silences all oscillators.
    ///
    /// @param buffer the single buffer to output the all the voices to
    ///
    inline void set_output(BLIPBuffer* buffer) {
        for (unsigned i = 0; i < OSC_COUNT; i++) set_output(i, buffer);
    }

    /// @brief Set the volume level of all oscillators.
    ///
    /// @param level the value to set the volume level to, where \f$1.0\f$ is
    /// full volume. Can be overdriven past \f$1.0\f$.
    ///
    inline void set_volume(double level = 1.0) { impl->set_volume(level); }

    /// @brief Set treble equalization for the synthesizers.
    ///
    /// @param equalizer the equalization parameter for the synthesizers
    ///
    inline void set_treble_eq(const BLIPEqualizer& equalizer) {
        impl->set_treble_eq(equalizer);
    }

    /// @brief Reset internal frame counter, registers, and all oscillators.
    ///
    /// @param new_engine the engine to use after resetting the chip
    ///
    inline void reset(Engine* new_engine = nullptr) {
        if (new_engine == nullptr && impl == nullptr)  // cannot reset without engine
            throw Exception("cannot reset with implied engine without setting engine");
        else if (new_engine != nullptr)  // set the engine
            impl = new_engine;
        // reset the instance variables
        last_time = 0;
        poly5_pos = 0;
        poly4_pos = 0;
        polym_pos = 0;
        control = 0;
        for (Oscillator& osc : oscs) osc.reset();
    }

    /// @brief Write data to register with given address.
    ///
    /// @param address the address to write the data to
    /// @param data the data to write to the given address
    ///
    inline void write(uint16_t address, uint8_t data) {
        // make sure the given address is legal
        if (address < ADDR_START or address > ADDR_END)
            throw AddressSpaceException<uint16_t>(address, ADDR_START, ADDR_END);
        unsigned i = (address ^ 0xD200) >> 1;
        if (i < OSC_COUNT) {
            oscs[i].regs[address & 1] = data;
        } else if (address == 0xD208) {
            control = data;
        } else if (address == 0xD209) {
            oscs[0].delay = 0;
            oscs[1].delay = 0;
            oscs[2].delay = 0;
            oscs[3].delay = 0;
        } else if (address == 0xD20F) {
            // TODO: are polynomials reset in this case?
            // if ((data & 3) == 0) polym_pos = 0;
        }
    }

    /// Run all oscillators up to specified time, end current frame, then
    /// start a new frame at time 0.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    inline void end_frame(blip_time_t end_time) {
        run_until(end_time);
        last_time -= end_time;
    }
};

#endif  // DSP_ATARI_POKEY_HPP_
