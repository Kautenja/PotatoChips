// Namco 163 chip emulator.
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

#ifndef DSP_NAMCO_163_HPP_
#define DSP_NAMCO_163_HPP_

#include "blip_buffer.hpp"
#include "exceptions.hpp"

/// @brief Namco 163 chip emulator.
class Namco163 {
 public:
    /// CPU clock cycle count
    typedef int32_t cpu_time_t;
    /// 16-bit memory address
    typedef int16_t cpu_addr_t;
    /// the number of oscillators on the chip
    static constexpr unsigned OSC_COUNT = 8;
    /// the number of registers on the chip
    static constexpr unsigned REG_COUNT = 0x80;

    /// Addresses to the registers for channel 0. To get the register for
    /// channel \f$n\f$, add \f$8n\f$.
    enum Register : uint16_t {
        /// The register for the low 8 bits of the 18-bit frequency for channel 0
        FREQ_LOW = 0x40,
        /// The register for the low 8 bits of the 18-bit phase for channel 0
        PHASE_LOW,
        /// The register for the middle 8 bits of the 18-bit frequency for channel 0
        FREQ_MEDIUM,
        /// The register for the middle 8 bits of the 18-bit phase for channel 0
        PHASE_MEDIUM,
        /// The register for the high 2 bits of the 18-bit frequency for channel 0.
        /// The high 6-bits of this register determine the waveform length.
        FREQ_HIGH,
        /// The register for the high 2 bits of the 18-bit phase for channel 0
        PHASE_HIGH,
        /// The register for the address of the waveform for channel 0
        WAVE_ADDRESS,
        /// The register for the volume level for channel 0
        VOLUME
    };

    /// the number of register per voice on the chip
    static constexpr auto REGS_PER_VOICE = 8;

 private:
    /// An oscillator on the Namco163 chip.
    struct Oscillator {
        /// TODO: document
        int32_t delay;
        /// the output buffer to write samples to
        BLIPBuffer* output;
        /// TODO: document
        int16_t last_amp;
        /// the position in the waveform
        int16_t wave_pos;
    };

    /// the oscillators on the chip
    Oscillator oscs[OSC_COUNT];

    /// the time after the last run_until call
    cpu_time_t last_time = 0;
    /// the register to read / write data from / to
    int addr_reg;

    /// the RAM on the chip
    uint8_t reg[REG_COUNT];
    /// the synthesizer for producing sound from the chip
    BLIPSynthesizer<float, BLIP_QUALITY_GOOD, 15> synth;

    /// Return a reference to the register pointed to by the address register.
    /// @details
    /// increments the addr_reg by 1 after accessing
    inline uint8_t& access() {
        int addr = addr_reg & 0x7f;
        if (addr_reg & 0x80) addr_reg = (addr + 1) | 0x80;
        return reg[addr];
    }

    /// Run the emulator until specified time.
    ///
    /// @param nes_end_time the number of elapsed cycles
    ///
    void run_until(cpu_time_t nes_end_time) {
        if (nes_end_time < last_time)
            throw Exception("end_time must be >= last_time");
        else if (nes_end_time == last_time)
            return;
        // get the number of active oscillators
        unsigned int active_oscs = ((reg[0x7f] >> 4) & 7) + 1;
        for (unsigned i = OSC_COUNT - active_oscs; i < OSC_COUNT; i++) {
            Oscillator& osc = oscs[i];
            BLIPBuffer* output = osc.output;
            if (!output) continue;

            auto time = last_time * output->get_factor() + osc.delay;
            auto end_time = nes_end_time * output->get_factor();
            osc.delay = 0;
            if (time < end_time) {
                // get the register bank for this oscillator
                const uint8_t* osc_reg = &reg[i * 8 + 0x40];
                // get the volume for this voice
                int volume = osc_reg[7] & 15;
                if (!volume)  // no sound when volume is 0
                    continue;
                // calculate the length of the waveform from the L value
                int wave_size = 256 - (osc_reg[4] & 0b11111100);
                if (!wave_size)  // no sound when wave size is 0
                    continue;
                // calculate the 18-bit frequency
                uint32_t freq = (static_cast<uint32_t>(osc_reg[4] & 0b11) << 16) |
                                (static_cast<uint32_t>(osc_reg[2]       ) <<  8) |
                                 static_cast<uint32_t>(osc_reg[0]       );
                // prevent low frequencies from excessively delaying freq changes
                if (freq < 64 * active_oscs) continue;
                // calculate the period of the waveform
                auto period = (((osc_reg[4] >> 2)) * 15 * 65536 * active_oscs / freq) * output->get_factor() / wave_size;
                // backup the amplitude and position
                int last_amp = osc.last_amp;
                int wave_pos = osc.wave_pos;
                do {  // process the samples on the voice
                    // read wave sample
                    int addr = wave_pos + osc_reg[6];
                    int sample = reg[addr >> 1] >> (addr << 2 & 4);
                    wave_pos++;
                    sample = (sample & 15) * volume;
                    // output impulse if amplitude changed
                    int delta = sample - last_amp;
                    if (delta) {
                        last_amp = sample;
                        synth.offset_resampled(time, delta, output);
                    }
                    // next sample
                    time += period;
                    if (wave_pos >= wave_size) wave_pos = 0;
                } while (time < end_time);
                // update position and amplitude
                osc.wave_pos = wave_pos;
                osc.last_amp = last_amp;
            }
            osc.delay = time - end_time;
        }
        last_time = nes_end_time;
    }

    /// Disable the public copy constructor.
    Namco163(const Namco163&);

    /// Disable the public assignment operator.
    Namco163& operator=(const Namco163&);

 public:
    /// Initialize a new Namco 163 chip emulator.
    Namco163() {
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
        for (unsigned channel = 0; channel < OSC_COUNT; channel++)
            set_output(channel, buffer);
    }

    /// @brief Set the volume level of all oscillators.
    ///
    /// @param level the value to set the volume level to, where \f$1.0\f$ is
    /// full volume. Can be overdriven past \f$1.0\f$.
    ///
    inline void set_volume(double level = 1.f) {
        synth.set_volume(0.10 / OSC_COUNT * level);
    }

    /// @brief Set treble equalization for the synthesizers.
    ///
    /// @param equalizer the equalization parameter for the synthesizers
    ///
    inline void set_treble_eq(const BLIPEqualizer<float>& equalizer) {
        synth.set_treble_eq(equalizer);
    }

    /// @brief Reset internal frame counter, registers, and all oscillators.
    inline void reset() {
        last_time = 0;
        addr_reg = 0;
        memset(reg, 0, REG_COUNT);
        for (unsigned i = 0; i < OSC_COUNT; i++) {
            Oscillator& osc = oscs[i];
            osc.delay = 0;
            osc.last_amp = 0;
            osc.wave_pos = 0;
        }
    }

    /// @brief Write data to register with given address.
    ///
    /// @param address the address to write the data to
    /// @param data the data to write to the given address
    ///
    inline void write(uint16_t address, uint8_t data) {
        static constexpr int32_t time = 0;
        run_until(time);
        addr_reg = address;
        access() = data;
    }

    /// @brief Run all oscillators up to specified time, end current frame,
    /// then start a new frame at time 0.
    ///
    /// @param time the time to run the oscillators until
    ///
    inline void end_frame(cpu_time_t time) {
        run_until(time);
        last_time -= time;
    }
};

#endif  // DSP_NAMCO_163_HPP_
