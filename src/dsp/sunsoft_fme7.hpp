// SunSoft FME7 chip emulator.
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

#ifndef DSP_SUNSOFT_FME7_HPP_
#define DSP_SUNSOFT_FME7_HPP_

#include "blip_buffer.hpp"
#include "exceptions.hpp"

/// @brief SunSoft FME7 chip emulator.
class SunSoftFME7 {
 public:
    /// the number of oscillators on the chip
    static constexpr unsigned OSC_COUNT = 3;
    /// the number of registers on the chip
    /// the first address of the RAM space
    static constexpr uint8_t ADDR_START = 0;
    /// the last address of the RAM space
    static constexpr uint8_t ADDR_END   = 14;
    /// the number of registers on the chip
    static constexpr uint8_t NUM_REGISTERS = ADDR_END - ADDR_START;

    /// the indexes of the channels on the chip
    enum Channel {
        PULSEA,
        PULSEB,
        PULSEC,
    };

    /// the IO registers on the chip.
    enum Register : uint8_t {
        /// the low 8 bits of the 12 bit frequency for pulse channel A
        PULSE_A_LO   = 0x00,
        /// the high 4 bits of the 12 bit frequency for pulse channel A
        PULSE_A_HI   = 0x01,
        /// the low 8 bits of the 12 bit frequency for pulse channel B
        PULSE_B_LO   = 0x02,
        /// the high 4 bits of the 12 bit frequency for pulse channel B
        PULSE_B_HI   = 0x03,
        /// the low 8 bits of the 12 bit frequency for pulse channel C
        PULSE_C_LO   = 0x04,
        /// the high 4 bits of the 12 bit frequency for pulse channel C
        PULSE_C_HI   = 0x05,
        /// the period of the noise generator
        NOISE_PERIOD = 0x06,
        /// the noise of the noise generator
        NOISE_TONE   = 0x07,
        /// the envelope register for pulse channel A
        PULSE_A_ENV  = 0x08,
        /// the envelope register for pulse channel B
        PULSE_B_ENV  = 0x09,
        /// the envelope register for pulse channel C
        PULSE_C_ENV  = 0x0A,
        /// the low 8 bits of the envelope frequency register
        ENV_LO       = 0x0B,
        /// the high 4 bits of the envelope frequency register
        ENV_HI       = 0x0C,
        /// the envelope reset register
        ENV_RESET    = 0x0D,
        // IO_PORT_A    = 0x0E,  // unused
        // IO_PORT_B    = 0x0F   // unused
    };

 private:
    /// the range of the amplifier on the chip. It could be any potential
    /// value; 192 gives best error / quality trade-off
    enum { AMP_RANGE = 192 };
    /// the table of volume levels for the amplifier
    static const uint8_t AMP_TABLE[16];

    /// the registers on the chip
    uint8_t regs[NUM_REGISTERS];

    /// the oscillators on the chip
    struct {
        /// the output buffer to write samples to
        BLIPBuffer* output;
        /// the last amplitude value to output from the oscillator
        int last_amp;
    } oscs[OSC_COUNT];
    /// the value of the pulse waveform generators
    bool phases[OSC_COUNT] = {false, false, false};
    /// delays for the oscillators
    uint16_t delays[OSC_COUNT] = {0, 0, 0};

    /// the last time the oscillators were updated
    blip_time_t last_time;

    /// the synthesizer for generating sound from the chip
    BLIPSynthesizer<BLIP_QUALITY_GOOD, 1> synth;

    /// Run the oscillators until the given end time.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    void run_until(blip_time_t end_time) {
        if (end_time < last_time)
            throw Exception("end_time must be >= last_time");
        else if (end_time == last_time)
            return;

        for (unsigned index = 0; index < OSC_COUNT; index++) {
            // int mode = regs[7] >> index;
            int vol_mode = regs[010 + index];
            int volume = AMP_TABLE[vol_mode & 0x0F];

            BLIPBuffer* const osc_output = oscs[index].output;
            if (!osc_output) continue;

            // period
            int const period_factor = 16;
            unsigned period = (regs[index * 2 + 1] & 0x0F) * 0x100 * period_factor + regs[index * 2] * period_factor;
            if (period < 50) {  // around 22 kHz
                volume = 0;
                if (!period) // on my AY-3-8910A, period doesn't have extra one added
                    period = period_factor;
            }

            // current amplitude
            int amp = volume;
            if (!phases[index])
                amp = 0;

            {  // scope for amp change update
                int delta = amp - oscs[index].last_amp;
                if (delta) {
                    oscs[index].last_amp = amp;
                    synth.offset(last_time, delta, osc_output);
                }
            }

            blip_time_t time = last_time + delays[index];
            if (time < end_time) {
                int delta = amp * 2 - volume;
                if (volume) {
                    do {
                        delta = -delta;
                        synth.offset(time, delta, osc_output);
                        time += period;
                    } while (time < end_time);
                    oscs[index].last_amp = (delta + volume) >> 1;
                    phases[index] = (delta > 0);
                } else {
                    // maintain phase when silent
                    int count = (end_time - time + period - 1) / period;
                    phases[index] ^= count & 1;
                    time += (long) count * period;
                }
            }
            delays[index] = time - end_time;
        }
        last_time = end_time;
    }

    /// Disable the copy constructor.
    SunSoftFME7(const SunSoftFME7&);

    /// Disable the assignment operator.
    SunSoftFME7& operator=(const SunSoftFME7&);

 public:
    /// Initialize a new SunSoft FME7 chip emulator.
    SunSoftFME7() {
        set_output(NULL);
        set_volume();
        reset();
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
    inline void set_volume(double level = 1.0) {
        synth.set_volume(0.38 / AMP_RANGE * level);
    }

    /// @brief Set treble equalization for the synthesizers.
    ///
    /// @param equalizer the equalization parameter for the synthesizers
    ///
    inline void set_treble_eq(BLIPEqualizer const& equalizer) {
        synth.set_treble_eq(equalizer);
    }

    /// @brief Reset internal state, registers, and all oscillators.
    inline void reset() {
        memset(regs, 0, sizeof regs);
        last_time = 0;
        for (unsigned i = 0; i < OSC_COUNT; i++)
            oscs[i].last_amp = 0;
    }

    /// @brief Write data to the chip port.
    ///
    /// @param address the byte to write to the latch port
    /// @param data the byte to write to the data port
    ///
    /// @details
    /// Sets the latch to address, then write the data using the latch
    ///
    inline void write(uint8_t address, uint8_t data) {
        static constexpr blip_time_t time = 0;
        // make sure the given address is legal. the minimal address is zero,
        // so just check the maximal address
        if (/*address < ADDR_START or*/ address > ADDR_END)
            throw AddressSpaceException<uint16_t>(address, ADDR_START, ADDR_END);
        run_until(time);
        regs[address] = data;
    }

    /// @brief Run all oscillators up to specified time, end current frame,
    /// then start a new frame at time 0.
    ///
    /// @param end_time the time to run the oscillators until
    ///
    inline void end_frame(blip_time_t time) {
        run_until(time);
        last_time -= time;
    }
};

// set the table of volume levels for the amplifier
const uint8_t SunSoftFME7::AMP_TABLE[16] = {
    #define ENTRY(n) static_cast<uint8_t>(n * AMP_RANGE + 0.5)
    ENTRY(0.0000), ENTRY(0.0078), ENTRY(0.0110), ENTRY(0.0156),
    ENTRY(0.0221), ENTRY(0.0312), ENTRY(0.0441), ENTRY(0.0624),
    ENTRY(0.0883), ENTRY(0.1249), ENTRY(0.1766), ENTRY(0.2498),
    ENTRY(0.3534), ENTRY(0.4998), ENTRY(0.7070), ENTRY(1.0000)
    #undef ENTRY
};

#endif  // DSP_SUNSOFT_FME7_HPP_
