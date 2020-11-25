// A 4-operator FM synthesizer based on Yamaha YM2612 emulation.
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

#ifndef DSP_YAMAHA_YM2612_FEEDBACK_OPERATOR_HPP_
#define DSP_YAMAHA_YM2612_FEEDBACK_OPERATOR_HPP_

#include "operator.hpp"

/// @brief Yamaha YM2612 emulation components.
namespace YamahaYM2612 {

/// @brief A single 4-operator FM voice.
struct FeedbackOperator : public Operator, public OperatorContext {
 private:
    /// general state
    OperatorContext state;
    /// operator output for feedback
    int32_t output_feedback[2] = {0, 0};
    /// a flag determining whether the phase increment needs to be updated
    bool update_phase_increment = false;
    /// feedback shift
    uint8_t feedback = 0;

 public:
    /// @brief Initialize a new YamahaYM2612 with given sample rate.
    ///
    /// @param sample_rate the rate to draw samples from the emulator at
    /// @param clock_rate the underlying clock rate of the system
    ///
    explicit FeedbackOperator(float sample_rate = 44100, float clock_rate = 768000) {
        set_sample_rate(sample_rate, clock_rate);
        reset();
    }

    /// @brief Set the sample rate the a new value.
    ///
    /// @param sample_rate the rate to draw samples from the emulator at
    /// @param clock_rate the underlying clock rate of the system
    ///
    inline void set_sample_rate(float sample_rate, float clock_rate) {
        state.set_sample_rate(sample_rate, clock_rate);
    }

    /// @brief Reset the voice to default.
    inline void reset() {
        state.reset();
        Operator::reset(state);
        feedback = 0;
        output_feedback[0] = output_feedback[1] = 0;
        update_phase_increment = true;
    }

    // -----------------------------------------------------------------------
    // MARK: Parameter Setters
    // -----------------------------------------------------------------------

    /// @brief Set the feedback amount.
    ///
    /// @param feedback the amount of feedback for the first operator
    ///
    inline void set_feedback(uint8_t value) {
        feedback = (value & 7) ? (value & 7) + 6 : 0;
    }

    /// @brief Set the global LFO for the voice.
    ///
    /// @param value the value of the LFO register
    ///
    inline void set_lfo(uint8_t value) { state.set_lfo(value); }

    /// @brief Set the frequency of the voice.
    ///
    /// @param frequency the frequency value measured in Hz
    ///
    inline void set_frequency(float frequency) {
        update_phase_increment |= Operator::set_frequency(state, frequency);
    }

    /// @brief Set the rate-scale (RS) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the rate-scale (RS) register of (in [0, 3])
    /// @param value the amount of rate-scale applied to the FM operator
    ///
    inline void set_rs(uint8_t value) {
        update_phase_increment |= Operator::set_rs(value);
    }

    /// @brief Set the multiplier (MUL) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the multiplier  (MUL)register of (in [0, 3])
    /// @param value the value of the FM phase multiplier
    ///
    inline void set_multiplier(uint8_t value) {
        update_phase_increment |= Operator::set_multiplier(value);
    }

    /// @brief Set the detune (DET) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the detune (DET) register of (in [0, 3])
    /// @param value the the level of detuning for the FM operator
    ///
    inline void set_detune(uint8_t value = 4) {
        update_phase_increment |= Operator::set_detune(state, value);
    }

    // -----------------------------------------------------------------------
    // MARK: Sampling / Stepping
    // -----------------------------------------------------------------------

    /// @brief Run a step on the emulator to produce a sample.
    ///
    /// @param mod the phase modulation signal
    /// @returns a 16-bit PCM sample from the synthesizer
    ///
    inline int16_t step(int16_t mod = 0) {
        // refresh phase and envelopes (KSR may have changed)
        if (update_phase_increment) {
            refresh_phase_and_envelope();
            update_phase_increment = false;
        }
        // update the SSG envelope
        update_ssg_envelope_generator();
        // calculate operator outputs
        const unsigned envelope = get_envelope(state);
        // sum [t-2] sample with [t-1] sample as the feedback carrier for op1
        int32_t fb_carrier = output_feedback[0] + output_feedback[1];
        // set the [t-2] sample as the [t-1] sample (i.e., step the history)
        output_feedback[0] = output_feedback[1];
        // set the output from the operator's feedback
        int32_t audio_output = output_feedback[1];
        // calculate the next output from operator
        if (envelope < ENV_QUIET) {  // operator envelope is open
            // if feedback is disabled, set feedback carrier to 0
            if (!feedback) fb_carrier = 0;
            // 1. shift mod by the bit-depth
            // 1. shift carrier by the feedback amount
            // 1. sum into phase modulation signal for operator
            const auto pm = (static_cast<int32_t>(mod) << 15) + (fb_carrier << feedback);
            output_feedback[1] = calculate_output(envelope, pm);
        } else {  // clear the next output from operator
            output_feedback[1] = 0;
        }
        // update phase counter AFTER output calculations
        update_phase_counters(state);
        // advance LFO & envelope generator
        state.advance_lfo();
        state.eg_timer += state.eg_timer_add;
        while (state.eg_timer >= state.eg_timer_overflow) {
            state.eg_timer -= state.eg_timer_overflow;
            state.eg_cnt++;
            update_envelope_generator(state.eg_cnt);
        }

        return audio_output;
    }
};

}  // namespace YamahaYM2612

#endif  // DSP_YAMAHA_YM2612_FEEDBACK_OPERATOR_HPP_
