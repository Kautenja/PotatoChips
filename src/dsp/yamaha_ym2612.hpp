// YM2612 FM sound chip emulator interface
// Copyright 2020 Christian Kauten
// Copyright 2006 Shay Green
// Copyright 2001 Jarek Burczynski
// Copyright 1998 Tatsuyuki Satoh
// Copyright 1997 Nicola Salmoria and the MAME team
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

#ifndef DSP_YAMAHA_YM2612_HPP_
#define DSP_YAMAHA_YM2612_HPP_

#include "yamaha_ym2612_operators.hpp"

/// Yamaha YM2612 chip emulator
class YamahaYM2612 {
 private:
    /// general state
    GlobalOperatorState state;
    /// channel state
    Voice voice;

 public:
    /// @brief Initialize a new YamahaYM2612 with given sample rate.
    ///
    /// @param sample_rate the rate to draw samples from the emulator at
    /// @param clock_rate the underlying clock rate of the system
    ///
    explicit YamahaYM2612(double sample_rate = 44100, double clock_rate = 768000) {
        setSampleRate(sample_rate, clock_rate);
        reset();
    }

    /// @brief Set the sample rate the a new value.
    ///
    /// @param sample_rate the rate to draw samples from the emulator at
    /// @param clock_rate the underlying clock rate of the system
    ///
    inline void setSampleRate(double sample_rate, double clock_rate) {
        state.set_sample_rate(sample_rate, clock_rate);
    }

    /// @brief Reset the emulator to its initial state.
    inline void reset() { state.reset(); voice.reset(state); }

    /// @brief Run a step on the emulator and return a 14-bit sample.
    inline int16_t step() { return voice.step(state); }

    // -----------------------------------------------------------------------
    // MARK: Global control
    // -----------------------------------------------------------------------

    /// @brief Set the global LFO for the chip.
    ///
    /// @param value the value of the LFO register
    ///
    inline void setLFO(uint8_t value) { state.set_lfo(value); }

    /// @brief Set the AM sensitivity (AMS) register for the given voice.
    ///
    /// @param value the amount of amplitude modulation (AM) sensitivity
    ///
    inline void setAMS(uint8_t value) { voice.set_am_sensitivity(value); }

    /// @brief Set the FM sensitivity (FMS) register for the given voice.
    ///
    /// @param value the amount of frequency modulation (FM) sensitivity
    ///
    inline void setFMS(uint8_t value) { voice.set_fm_sensitivity(value); }

    /// @brief Set the algorithm (AL) register for the given voice.
    ///
    /// @param value the selected FM algorithm in [0, 7]
    ///
    inline void setAL(uint8_t value) { voice.set_algorithm(value); }

    /// @brief Set the feedback (FB) register for the given voice.
    ///
    /// @param value the amount of feedback for operator 1
    ///
    inline void setFB(uint8_t value) { voice.set_feedback(value); }

    /// @brief Set the gate for the given voice.
    ///
    /// @param is_open true if the gate is open, false otherwise
    ///
    inline void setGATE(bool is_open) { voice.set_gate(is_open); }

    /// @brief Set the frequency of the voice.
    ///
    /// @param value the frequency value measured in Hz
    ///
    inline void setFREQ(float value) { voice.set_frequency(state, value); }

    // -----------------------------------------------------------------------
    // MARK: Operator control
    // -----------------------------------------------------------------------

    // TODO: separate into two functions, one to enable / disable and another
    // to set the mode
    // TODO: fix why some modes aren't working right.
    /// @brief Set the SSG-envelope register for the given channel and operator.
    ///
    /// @param op_index the operator to set the SSG-EG register of (in [0, 3])
    /// @param is_on whether the looping envelope generator should be turned on
    /// @param mode the mode for the looping generator to run in (in [0, 7])
    ///
    inline void setSSG(uint8_t op_index, bool is_on, uint8_t mode) {
        voice.operators[OPERATOR_INDEXES[op_index]].set_ssg(is_on, mode);
    }

    /// @brief Set the rate-scale (RS) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the rate-scale (RS) register of (in [0, 3])
    /// @param value the amount of rate-scale applied to the FM operator
    ///
    inline void setRS(uint8_t op_index, uint8_t value) {
        voice.update_phase_increment |= voice.operators[OPERATOR_INDEXES[op_index]].set_rs(value);
    }

    /// @brief Set the attack rate (AR) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the attack rate (AR) register of (in [0, 3])
    /// @param value the rate of the attack stage of the envelope generator
    ///
    inline void setAR(uint8_t op_index, uint8_t value) {
        voice.operators[OPERATOR_INDEXES[op_index]].set_ar(value);
    }

    /// @brief Set the total level (TL) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the total level (TL) register of (in [0, 3])
    /// @param value the total amplitude of envelope generator
    ///
    inline void setTL(uint8_t op_index, uint8_t value) {
        voice.operators[OPERATOR_INDEXES[op_index]].set_tl(value);
    }

    /// @brief Set the 1st decay rate (D1) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the 1st decay rate (D1) register of (in [0, 3])
    /// @param value the rate of decay for the 1st decay stage of the envelope generator
    ///
    inline void setD1(uint8_t op_index, uint8_t value) {
        voice.operators[OPERATOR_INDEXES[op_index]].set_dr(value);
    }

    /// @brief Set the sustain level (SL) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the sustain level (SL) register of (in [0, 3])
    /// @param value the amplitude level at which the 2nd decay stage of the envelope generator begins
    ///
    inline void setSL(uint8_t op_index, uint8_t value) {
        voice.operators[OPERATOR_INDEXES[op_index]].set_sl(value);
    }

    /// @brief Set the 2nd decay rate (D2) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the 2nd decay rate (D2) register of (in [0, 3])
    /// @param value the rate of decay for the 2nd decay stage of the envelope generator
    ///
    inline void setD2(uint8_t op_index, uint8_t value) {
        voice.operators[OPERATOR_INDEXES[op_index]].set_sr(value);
    }

    /// @brief Set the release rate (RR) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the release rate (RR) register of (in [0, 3])
    /// @param value the rate of release of the envelope generator after key-off
    ///
    inline void setRR(uint8_t op_index, uint8_t value) {
        voice.operators[OPERATOR_INDEXES[op_index]].set_rr(value);
    }

    /// @brief Set the multiplier (MUL) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the multiplier  (MUL)register of (in [0, 3])
    /// @param value the value of the FM phase multiplier
    ///
    inline void setMUL(uint8_t op_index, uint8_t value) {
        voice.update_phase_increment |= voice.operators[OPERATOR_INDEXES[op_index]].set_multiplier(value);
    }

    /// @brief Set the detune (DET) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the detune (DET) register of (in [0, 3])
    /// @param value the the level of detuning for the FM operator
    ///
    inline void setDET(uint8_t op_index, uint8_t value) {
        voice.update_phase_increment |= voice.operators[OPERATOR_INDEXES[op_index]].set_detune(state, value);
    }

    /// @brief Set the amplitude modulation (AM) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the amplitude modulation (AM) register of (in [0, 3])
    /// @param value the true to enable amplitude modulation from the LFO, false to disable it
    ///
    inline void setAM(uint8_t op_index, bool value) {
        voice.operators[OPERATOR_INDEXES[op_index]].is_amplitude_mod_on = value;
    }
};

#endif  // DSP_YAMAHA_YM2612_HPP_
