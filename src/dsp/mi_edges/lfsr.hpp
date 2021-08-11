// An oscillator that generates Linear Feedback Shift Register (LFSR) noise.
// Copyright 2020 Christian Kauten
// Copyright 2015 Emilie Gillet (emilie.o.gillet@gmail.com)
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

#ifndef DSP_OSCILLATORS_MI_EDGES_LFSR_HPP
#define DSP_OSCILLATORS_MI_EDGES_LFSR_HPP

#include <cstdint>
#include "../math.hpp"

/// @brief Structures for generating oscillating signals.
namespace Oscillator {

/// @brief Oscillator code from the Mutable Instruments _Edges_ module.
namespace MutableIntstrumentsEdges {

/// @brief A 16-bit Linear Feedback Shift Register (LFSR) oscillator.
class LFSR {
 private:
    /// the current frequency of the oscillator
    float freq = 440.f;
    /// the current phase of the oscillator
    float phase = 0.0;

    /// the number of bits to shift
    uint8_t shifts = 6;
    /// The random number register
    uint16_t lfsr = 1;
    /// the current sample
    float value = 0.f;

 public:
    /// @brief Set the frequency of the oscillator.
    ///
    /// @param frequency the frequency of the oscillator in Hertz
    ///
    inline void setFrequency(float frequency) { freq = frequency; }

    /// @brief Return the frequency of the oscillator.
    ///
    /// @returns the frequency of the oscillator in Hertz
    ///
    inline float getFrequency() { return freq; }

    /// @brief Set the number of bits to shift.
    ///
    /// @param shifts_ the new number of bits to shift
    ///
    inline void setShifts(uint8_t shifts_) {
        static constexpr uint8_t MIN = 1;
        static constexpr uint8_t MAX = 15;
        shifts = Math::clip(shifts_, MIN, MAX);
    }

    /// @brief Return the number of bits to shift.
    ///
    /// @returns the number of bits to shift when sampling the LFSR
    ///
    inline uint8_t getShifts() const { return shifts; }

    /// @brief Set the value of the shift register.
    ///
    /// @param value the new value for the shift register
    ///
    inline void setLFSR(uint16_t seed) { lfsr = seed; }

    /// @brief Return the current value of the shift register.
    ///
    /// @returns the current value of the shift register
    ///
    inline uint16_t getLFSR() const { return lfsr; }

    /// @brief Return the value from the oscillator.
    ///
    /// @returns the output from the oscillator in the range \f$[-1, 1]\f$
    ///
    inline float getValue() const { return value; }

    /// @brief Reset the oscillator to default internal state.
    void reset() {
        phase = 0.f;
        lfsr = 1;
        value = 0.f;
    }

    /// @brief Process a sample from the oscillator.
    ///
    /// @param deltaTime the amount of time between samples
    ///
    void process(float deltaTime) {
        // add the change in phase based on the sample time and the frequency
        phase += Math::clip(freq * deltaTime, 1e-6f, 0.5f);
        if (phase >= 1.f) {  // update the shift register to sample new value
            phase -= 1.f;  // circularly increment the phase counter
            // LFSR logic
            const uint8_t tap = lfsr >> shifts;
            const uint8_t random_bit = (lfsr ^ tap) & 1;
            lfsr >>= 1;
            uint16_t sample = 0x0cff;
            if (random_bit) {
                lfsr |= 0x4000;
                sample = 0x0300;
            }
            // divide the 12-bit sample by 4096.0 to normalize in [0.0, 1.0]
            // multiply by 2 and subtract 1 to get the value in [-1.0, 1.0]
            value = (2 * (sample / 4096.f) - 1);
        }
    }
};

}  // namespace MutableIntstrumentsEdges

}  // namespace Oscillator

#endif  // DSP_OSCILLATORS_MI_EDGES_LFSR_HPP
