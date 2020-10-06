// Sony S-DSP ADSR envelope generator emulator.
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

#ifndef DSP_SONY_S_DSP_ADSR_HPP_
#define DSP_SONY_S_DSP_ADSR_HPP_

#include <cstdint>

/// @brief Sony S-DSP ADSR envelope generator emulator.
class Sony_S_DSP_ADSR {
 private:
    // The following fields are in a particular order for byte-alignment
    // -----------------------------------------------------------------------
    // Byte 1
    // -----------------------------------------------------------------------
    /// the attack rate (4-bits)
    uint8_t attack : 4;
    /// the decay rate (3-bits)
    uint8_t decay : 3;
    /// a dummy bit for byte alignment
    const uint8_t unused_spacer_for_byte_alignment : 1;
    // -----------------------------------------------------------------------
    // Byte 2
    // -----------------------------------------------------------------------
    /// the sustain rate (5-bits)
    uint8_t sustain_rate : 5;
    /// the sustain level (3-bits)
    uint8_t sustain_level : 3;
    // -----------------------------------------------------------------------
    // Byte 3
    // -----------------------------------------------------------------------
    /// the total amplitude level of the envelope generator (8-bit)
    int8_t amplitude = 0;
    // -----------------------------------------------------------------------
    // Byte 4
    // -----------------------------------------------------------------------
    /// The stages of the ADSR envelope generator.
    enum class EnvelopeStage : uint8_t { Off, Attack, Decay, Sustain, Release };
    /// the current stage of the envelope generator
    EnvelopeStage envelope_stage = EnvelopeStage::Off;
    // -----------------------------------------------------------------------
    // Byte 5, 6, 7, 8
    // -----------------------------------------------------------------------
    /// the current value of the envelope generator
    int16_t envelope_value = 0;
    /// the sample (time) counter for the envelope
    int16_t envelope_counter = 0;

    /// The initial value of the envelope.
    static constexpr int ENVELOPE_RATE_INITIAL = 0x7800;

    /// the range of the envelope generator amplitude level (i.e., max value)
    static constexpr int ENVELOPE_RANGE = 0x0800;

    /// Return the envelope rate for the given index in the table.
    ///
    /// @param index the index of the envelope rate to return in the table
    /// @returns the envelope rate at given index in the table
    ///
    static inline uint16_t getEnvelopeRate(int index) {
        // This table is for envelope timing.  It represents the number of
        // counts that should be subtracted from the counter each sample
        // period (32kHz). The counter starts at 30720 (0x7800). Each count
        // divides exactly into 0x7800 without remainder.
        static const uint16_t ENVELOPE_RATES[0x20] = {
            0x0000, 0x000F, 0x0014, 0x0018, 0x001E, 0x0028, 0x0030, 0x003C,
            0x0050, 0x0060, 0x0078, 0x00A0, 0x00C0, 0x00F0, 0x0140, 0x0180,
            0x01E0, 0x0280, 0x0300, 0x03C0, 0x0500, 0x0600, 0x0780, 0x0A00,
            0x0C00, 0x0F00, 0x1400, 0x1800, 0x1E00, 0x2800, 0x3C00, 0x7800
        };
        return ENVELOPE_RATES[index];
    }

    /// @brief Process the envelope for the voice with given index.
    ///
    /// @returns the output value from the envelope generator
    ///
    inline int8_t clock_envelope() {
        switch (envelope_stage) {
        case EnvelopeStage::Off:
            break;
        case EnvelopeStage::Attack:
            // increase envelope by 1/64 each step
            if (attack == 15) {
                envelope_value += ENVELOPE_RANGE / 2;
            } else {
                envelope_counter -= getEnvelopeRate(2 * attack + 1);
                if (envelope_counter > 0) break;
                envelope_value += ENVELOPE_RANGE / 64;
                envelope_counter = ENVELOPE_RATE_INITIAL;
            }
            if (envelope_value >= ENVELOPE_RANGE) {
                envelope_value = ENVELOPE_RANGE - 1;
                envelope_stage = EnvelopeStage::Decay;
            }
            break;
        case EnvelopeStage::Decay:
            // Docs: "DR...[is multiplied] by the fixed value
            // 1-1/256." Well, at least that makes some sense.
            // Multiplying ENVX by 255/256 every time DECAY is
            // updated.
            envelope_counter -= getEnvelopeRate((decay << 1) + 0x10);
            if (envelope_counter <= 0) {
                envelope_counter = ENVELOPE_RATE_INITIAL;
                envelope_value -= ((envelope_value - 1) >> 8) + 1;
            }
            if (envelope_value <= (sustain_level + 1) * 0x100)
                envelope_stage = EnvelopeStage::Sustain;
            break;
        case EnvelopeStage::Sustain:
            // Docs: "SR[is multiplied] by the fixed value 1-1/256."
            // Multiplying ENVX by 255/256 every time SUSTAIN is
            // updated.
            envelope_counter -= getEnvelopeRate(sustain_rate);
            if (envelope_counter <= 0) {
                envelope_counter = ENVELOPE_RATE_INITIAL;
                envelope_value -= ((envelope_value - 1) >> 8) + 1;
            }
            break;
        case EnvelopeStage::Release:
            // Docs: "When in the state of "key off". the "click" sound is
            // prevented by the addition of the fixed value 1/256" WTF???
            // Alright, I'm going to choose to interpret that this way:
            // When a note is keyed off, start the RELEASE state, which
            // subtracts 1/256th each sample period (32kHz).  Note there's
            // no need for a count because it always happens every update.
            envelope_value -= ENVELOPE_RANGE / 256;
            if (envelope_value <= 0) {
                envelope_stage = EnvelopeStage::Off;
                envelope_value = 0;
            }
            break;
        }

        return envelope_value >> 4;
    }

 public:
    /// the sample rate of the S-DSP in Hz
    static constexpr unsigned SAMPLE_RATE = 32000;

    /// @brief Initialize a new Sony_S_DSP_ADSR.
    Sony_S_DSP_ADSR() : unused_spacer_for_byte_alignment(0) {
        attack = 0;
        decay = 0;
        sustain_rate = 0;
        sustain_level = 0;
    }

    /// @brief Set the attack parameter to a new value.
    ///
    /// @param value the attack rate to use.
    ///
    inline void setAttack(uint8_t value) { attack = value; }

    /// @brief Set the decay parameter to a new value.
    ///
    /// @param value the decay rate to use.
    ///
    inline void setDecay(uint8_t value) { decay = value; }

    /// @brief Set the sustain rate parameter to a new value.
    ///
    /// @param value the sustain rate to use.
    ///
    inline void setSustainRate(uint8_t value) { sustain_rate = value; }

    /// @brief Set the sustain level parameter to a new value.
    ///
    /// @param value the sustain level to use.
    ///
    inline void setSustainLevel(uint8_t value) { sustain_level = value; }

    /// @brief Set the amplitude parameter to a new value.
    ///
    /// @param value the amplitude level to use.
    ///
    inline void setAmplitude(int8_t value) { amplitude = value; }

    /// @brief Run DSP for some samples and write them to the given buffer.
    int16_t run(bool trigger, bool gate_on) {
        if (trigger) {  // trigger the envelope generator
            // reset the envelope value to 0 and the stage to attack
            envelope_value = 0;
            envelope_stage = EnvelopeStage::Attack;
            // NOTE: Real SNES does *not* appear to initialize the envelope
            // counter to anything in particular. The first cycle always seems to
            // come at a random time sooner than expected; as yet, I have been
            // unable to find any pattern.  I doubt it will matter though, so
            // we'll go ahead and do the full time for now.
            envelope_counter = ENVELOPE_RATE_INITIAL;
        } else if (envelope_stage == EnvelopeStage::Off) {
            return 0;
        } else if (!gate_on) {  // gate went low, move to release stage
            envelope_stage = EnvelopeStage::Release;
        }
        // clock the envelope generator and apply the global amplitude level
        const int output = clock_envelope();
        return (output * amplitude) >> 7;
    }
};

#endif  // DSP_SONY_S_DSP_ADSR_HPP_
