// A digital oscillator that generates different waveforms.
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

#ifndef DSP_OSCILLATORS_MI_EDGES_WAVETABLE_HPP
#define DSP_OSCILLATORS_MI_EDGES_WAVETABLE_HPP

#include <algorithm>
#include <cstdint>
#include "../math.hpp"
#include "wavetables.hpp"

/// @brief Structures for generating oscillating signals.
namespace Oscillator {

/// @brief Oscillator code from the Mutable Instruments _Edges_ module.
namespace MutableIntstrumentsEdges {

/// @brief A 48kHz digital oscillator with different shapes
/// @details
/// The available shapes are:
/// 1. Sine,
/// 2. Triangle,
/// 3. Nintendo Entertainment System (NES) Triangle,
/// 4. Sample+Hold (S+H) Noise,
/// 5. Linear Feedback Shift Register (LFSR) Noise Short, and
/// 6. Linear Feedback Shift Register (LFSR) Noise Long.
///
class DigitalOscillator {
 public:
    /// @brief The wave shapes for the oscillator
    enum class Shape {
        Sine = 0,
        Triangle,
        NES_Triangle,
        SampleHold,
        LFSR_Long,
        LFSR_Short,
        Count
    };

 private:
    /// the current shape of the wave produced by the oscillator
    Shape shape = Shape::Sine;

    /// the MIDI note that corresponds to the current pitch
    uint8_t note = 60;
    /// the current frequency of the oscillator
    float freq = rack::dsp::FREQ_C4;
    /// the current phase of the oscillator
    float phase = 0.0;

    /// the current width of the pulse in [0, 1]
    uint8_t pulseWidth = 127;
    /// The auxiliary phase for the sine wave bit crusher
    uint16_t aux_phase = 0;

    /// The random number generator state for generating random noise
    uint16_t rng = 1;
    /// A sample from the sine wave to use for the random noise generators
    uint16_t sample = 0;
    /// The output value from the oscillator (12-bit in 16-bit container)
    uint16_t value = 0;

 public:
    /// whether the gate for the oscillator is open
    bool gateOpen = true;

    /// @brief Reset the oscillator to its default state
    void reset() {
        shape = Shape::Sine;
        note = 60;
        freq = rack::dsp::FREQ_C4;
        phase = 0.0;
        pulseWidth = 127;
        aux_phase = 0;
        rng = 1;
        sample = 0;
        value = 0;
        gateOpen = true;
    }

    /// @brief Set the pitch of the oscillator.
    ///
    /// @param pitch the pitch of the oscillator in units/octave
    ///
    inline void setPitch(const float& pitch) {
        freq = Math::clip(rack::dsp::FREQ_C4 * powf(2.f, pitch), 0.0f, 20000.0f);
        note = 60 + 12 * pitch;
    }

    /// @brief Return the pitch of the oscillator.
    ///
    /// @returns the pitch of the oscillator in units/octave
    ///
    inline float getPitch() const { return std::log2(freq / rack::dsp::FREQ_C4); }

    /// @brief Set the frequency of the oscillator.
    ///
    /// @param frequency the frequency of the oscillator in Hertz
    ///
    inline void setFrequency(const float& frequency) {
        freq = frequency;
        note = 60 + 12 * std::log2(frequency / rack::dsp::FREQ_C4);
    }

    /// @brief Return the frequency of the oscillator.
    ///
    /// @returns the frequency of the oscillator in Hertz
    ///
    inline float getFrequency() const { return freq; }

    /// @brief Set the shape of the oscillator.
    ///
    /// @param shape_ the new shape for the oscillator waveform
    ///
    inline void setShape(const Shape& shape_) { shape = shape_; }

    /// @brief Return the current shape of the oscillator.
    ///
    /// @returns the shape of the oscillator waveform
    ///
    inline Shape getShape() const { return shape; }

    /// @brief Cycle the shape of the oscillator.
    inline void cycleShape() {
        const auto length = static_cast<int>(Shape::Count);
        shape = static_cast<Shape>((static_cast<int>(shape) + 1) % length);
    }

    /// @brief Set the pulse width for the sine bit-crusher.
    ///
    /// @param pulseWidth_ the width of the pulse used for the sine bit-crusher
    ///
    inline void setPulseWidth(const uint8_t& pulseWidth_) {
        pulseWidth = pulseWidth_;
    }

    /// @brief Return the pulse width for the sine bit-crusher.
    ///
    /// @returns the width of the pulse used for the sine bit-crusher
    ///
    inline uint8_t getPulseWidth() {
        return pulseWidth;
    }

    /// @brief Set the value of the shift register.
    ///
    /// @param value the new value for the shift register
    ///
    inline void setLFSR(const uint16_t& seed) { rng = seed; }

    /// @brief Return the current value of the shift register.
    ///
    /// @returns the current value of the shift register
    ///
    inline uint16_t getLFSR() const { return rng; }

    /// @brief Set the sample to a new value.
    ///
    /// @param sample_ the new sample for the oscillator
    ///
    inline void setSample(const uint16_t& sample_) { sample = sample_; }

    /// @brief Return the current sample.
    ///
    /// @returns the current 12-bit sample from the oscillator
    ///
    inline uint16_t getSample() const { return sample; }

    /// @brief Return the value from the oscillator.
    ///
    /// @returns the 12-bit value of the oscillator normalize in \f$[-1, 1]\f$
    ///
    inline float getValue() const {
        // divide the 12-bit value by 4096.0 to normalize in [0.0, 1.0]
        // multiply by 2 and subtract 1 to get the value in [-1.0, 1.0]
        // return gateOpen * 2 * static_cast<float>(value >> 12) - 1;
        return gateOpen * 2 * (value / 4096.f) - 1;
    }

    /// @brief Process a sample from the oscillator.
    ///
    /// @param deltaTime the amount of time between samples
    ///
    void process(const float& deltaTime) {
        if (!gateOpen) {
            value = 0.f;
            return;
        }
        // Advance phase counter
        const float deltaPhase = Math::clip(freq * deltaTime, 1e-6f, 0.5f);
        phase += deltaPhase;
        if (phase >= 1.f) phase -= floor(phase);
        // calculate quantized versions of the phase and sample time
        const auto phaseQ = std::numeric_limits<uint16_t>::max() * phase;
        const auto deltaPhaseQ = std::numeric_limits<uint16_t>::max() * deltaPhase;
        switch(shape) {  // render the waveform
        case Shape::Sine:         { return renderSine(phaseQ);                  }
        case Shape::Triangle:     { return renderBandlimitedTriangle(phaseQ);   }
        case Shape::NES_Triangle: { return renderBandlimitedTriangle(phaseQ);   }
        case Shape::SampleHold:   { return renderNoise(phaseQ, deltaPhaseQ);    }
        case Shape::LFSR_Long:    { return renderNoiseNES(phaseQ, deltaPhaseQ); }
        case Shape::LFSR_Short:   { return renderNoiseNES(phaseQ, deltaPhaseQ); }
        default:                  { return;                                     }
        };
    }

 private:
    /// Render a sine wave from the oscillator.
    void renderSine(const uint16_t& phase) {
        uint16_t aux_phase_increment = bitcrusher_increments[pulseWidth];
        aux_phase = aux_phase + aux_phase_increment;
        if (aux_phase < aux_phase_increment || !aux_phase_increment) {
            sample = interpolate(triangle_6, phase) << 8;
        }
        // set the value to the current sample
        value = sample >> 4;
    }

    /// Render a triangle wave from the oscillator.
    void renderBandlimitedTriangle(const uint16_t& phase) {
        // determine gains for mixing between wave-tables based on MIDI note
        uint8_t balance = ((note - 12) << 4) | ((note - 12) >> 4);
        uint8_t gain_2 = balance & 0xf0;
        uint8_t gain_1 = ~gain_2;
        // determine the base wave-table (NES triangle or regular triangle)
        uint8_t base = shape == Shape::NES_Triangle ? NES_TRIANGLE_0 : TRIANGLE_0;
        // lookup first wave-table
        uint8_t index = balance & 0xf;
        const uint8_t* wave_1 = lookup_table[base + index];
        // lookup second wave-table
        index = std::min<uint8_t>(index + 1, NUM_WAVETABLES);
        const uint8_t* wave_2 = lookup_table[base + index];
        // interpolate the value between the wave-tables
        value = interpolate(wave_1, wave_2, gain_1, gain_2, phase) >> 4;
    }

    /// Render NES noise from the oscillator.
    void renderNoiseNES(const uint16_t& phase, const uint16_t& deltaPhase) {
        if (phase < deltaPhase) {  // sample a new value
            uint8_t tap = shape == Shape::LFSR_Short ? rng >> 6 : rng >> 1;
            uint8_t random_bit = (rng ^ tap) & 1;
            rng >>= 1;
            if (random_bit) {
                rng |= 0x4000;
                sample = 0x0300;
            } else {
                sample = 0x0cff;
            }
        }
        // set the value to the current sample
        value = sample;
    }

    /// Render and hold noise from the oscillator.
    void renderNoise(const uint16_t& phase, const uint16_t& deltaPhase) {
        if (phase < deltaPhase) {  // sample a new value
            rng = (rng >> 1) ^ (-(rng & 1) & 0xb400);
            sample = rng & 0x0fff;
            sample = 512 + ((sample * 3) >> 2);
        }
        // set the value to the current sample
        value = sample;
    }
};

}  // namespace MutableIntstrumentsEdges

}  // namespace Oscillator

#endif  // DSP_OSCILLATORS_MI_EDGES_WAVETABLE_HPP
