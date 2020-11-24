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

#ifndef DSP_YAMAHA_YM2612_VOICE4OP_HPP_
#define DSP_YAMAHA_YM2612_VOICE4OP_HPP_

#include "operator.hpp"

/// @brief Yamaha YM2612 emulation components.
namespace YamahaYM2612 {

/// @brief A single 4-operator FM voice.
struct Voice1Op {
 public:
    /// the maximal value that an operator can output (signed 14-bit)
    static constexpr int32_t OUTPUT_MAX = 8191;
    /// the minimal value that an operator can output (signed 14-bit)
    static constexpr int32_t OUTPUT_MIN = -8192;

    /// @brief clip the given sample to 14 bits.
    ///
    /// @param sample the sample to clip to 14 bits
    /// @returns the sample after clipping to 14 bits
    ///
    static inline int16_t clip(int16_t sample) {
        if (sample > OUTPUT_MAX)
            return OUTPUT_MAX;
        else if (sample < OUTPUT_MIN)
            return OUTPUT_MIN;
        return sample;
    }

 private:
    /// general state
    OperatorContext state;
    /// 1-op voice
    Operator oprtr;

    /// a flag determining whether the phase increment needs to be updated
    bool update_phase_increment = false;

    /// feedback shift
    uint8_t feedback = 0;

    /// operator 1 output for feedback
    int32_t output_feedback[2] = {0, 0};

    /// Phase Modulation input for operator 2
    int32_t m2 = 0;
    /// Phase Modulation input for operator 3
    int32_t c1 = 0;
    /// Phase Modulation input for operator 4
    int32_t c2 = 0;
    /// one sample delay memory
    int32_t mem = 0;

    /// the last output sample from the voice
    int32_t audio_output = 0;

 public:
    /// @brief Initialize a new YamahaYM2612 with given sample rate.
    ///
    /// @param sample_rate the rate to draw samples from the emulator at
    /// @param clock_rate the underlying clock rate of the system
    ///
    explicit Voice1Op(float sample_rate = 44100, float clock_rate = 768000) {
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
        oprtr.reset(state);
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
        update_phase_increment |= oprtr.set_frequency(state, frequency);
    }

    /// @brief Set the gate for the given voice.
    ///
    /// @param op_index the operator to set the gate of of (in [0, 3])
    /// @param is_open true if the gate is open, false otherwise
    /// @param prevent_clicks true to prevent clicks from note re-triggers
    ///
    inline void set_gate(bool is_open, bool prevent_clicks = false) {
        oprtr.set_gate(is_open, prevent_clicks);
    }

    /// @brief Set the rate-scale (RS) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the rate-scale (RS) register of (in [0, 3])
    /// @param value the amount of rate-scale applied to the FM operator
    ///
    inline void set_rate_scale(uint8_t value) {
        update_phase_increment |= oprtr.set_rs(value);
    }

    /// @brief Set the attack rate (AR) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the attack rate (AR) register of (in [0, 3])
    /// @param value the rate of the attack stage of the envelope generator
    ///
    inline void set_attack_rate(uint8_t value) {
        oprtr.set_ar(value);
    }

    /// @brief Set the total level (TL) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the total level (TL) register of (in [0, 3])
    /// @param value the total amplitude of envelope generator
    ///
    inline void set_total_level(uint8_t value) {
        oprtr.set_tl(value);
    }

    /// @brief Set the 1st decay rate (D1) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the 1st decay rate (D1) register of (in [0, 3])
    /// @param value the rate of decay for the 1st decay stage of the envelope generator
    ///
    inline void set_decay_rate(uint8_t value) {
        oprtr.set_dr(value);
    }

    /// @brief Set the sustain level (SL) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the sustain level (SL) register of (in [0, 3])
    /// @param value the amplitude level at which the 2nd decay stage of the envelope generator begins
    ///
    inline void set_sustain_level(uint8_t value) {
        oprtr.set_sl(value);
    }

    /// @brief Set the 2nd decay rate (D2) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the 2nd decay rate (D2) register of (in [0, 3])
    /// @param value the rate of decay for the 2nd decay stage of the envelope generator
    ///
    inline void set_sustain_rate(uint8_t value) {
        oprtr.set_sr(value);
    }

    /// @brief Set the release rate (RR) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the release rate (RR) register of (in [0, 3])
    /// @param value the rate of release of the envelope generator after key-off
    ///
    inline void set_release_rate(uint8_t value) {
        oprtr.set_rr(value);
    }

    /// @brief Set the multiplier (MUL) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the multiplier  (MUL)register of (in [0, 3])
    /// @param value the value of the FM phase multiplier
    ///
    inline void set_multiplier(uint8_t value) {
        update_phase_increment |= oprtr.set_multiplier(value);
    }

    /// @brief Set the detune (DET) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the detune (DET) register of (in [0, 3])
    /// @param value the the level of detuning for the FM operator
    ///
    inline void set_detune(uint8_t value = 4) {
        update_phase_increment |= oprtr.set_detune(state, value);
    }

    /// @brief Set whether SSG envelopes are enabled for the given operator.
    ///
    /// @param op_index the operator to set the SSG-EG register of (in [0, 3])
    /// @param is_on whether the looping envelope generator should be turned on
    ///
    inline void set_ssg_enabled(bool is_on) {
        oprtr.set_ssg_enabled(is_on);
    }

    /// @brief Set the AM sensitivity (AMS) register for the given voice.
    ///
    /// @param value the amount of amplitude modulation (AM) sensitivity
    ///
    inline void set_am_sensitivity(uint8_t value) {
        oprtr.set_am_sensitivity(value);
    }

    /// @brief Set the FM sensitivity (FMS) register for the given voice.
    ///
    /// @param value the amount of frequency modulation (FM) sensitivity
    ///
    inline void set_fm_sensitivity(uint8_t value) {
        oprtr.set_fm_sensitivity(value);
    }

    // -----------------------------------------------------------------------
    // MARK: Sampling / Stepping
    // -----------------------------------------------------------------------

    /// @brief Run a step on the emulator to produce a sample.
    ///
    /// @returns a 16-bit PCM sample from the synthesizer
    ///
    inline int16_t step() {
        // refresh phase and envelopes (KSR may have changed)
        if (update_phase_increment) {
            oprtr.refresh_phase_and_envelope();
            update_phase_increment = false;
        }
        // clear the audio output
        audio_output = 0;
        // update the SSG envelope
        oprtr.update_ssg_envelope_generator();
        // -------------------------------------------------------------------
        // calculate operator outputs
        // -------------------------------------------------------------------
        // reset the algorithm outputs to 0
        m2 = c1 = c2 = mem = 0;
        // Operator 1
        unsigned envelope = oprtr.get_envelope(state);
        // sum [t-2] sample with [t-1] sample as the feedback carrier for op1
        int32_t feedback_carrier = output_feedback[0] + output_feedback[1];
        // set the [t-2] sample as the [t-1] sample (i.e., step the history)
        output_feedback[0] = output_feedback[1];
        // set the connection outputs from operator 1 based on the algorithm
        audio_output += output_feedback[1];
        // calculate the next output from operator 1
        if (envelope < ENV_QUIET) {  // operator 1 envelope is open
            // if feedback is disabled, set feedback carrier to 0
            if (!feedback) feedback_carrier = 0;
            // shift carrier by the feedback amount
            output_feedback[1] = oprtr.calculate_output(envelope, feedback_carrier << feedback);
        } else {  // clear the next output from operator 1
            output_feedback[1] = 0;
        }

        // update phase counter AFTER output calculations
        oprtr.update_phase_counters(state);
        // -------------------------------------------------------------------
        // advance LFO & envelope generator
        // -------------------------------------------------------------------
        state.advance_lfo();
        state.eg_timer += state.eg_timer_add;
        while (state.eg_timer >= state.eg_timer_overflow) {
            state.eg_timer -= state.eg_timer_overflow;
            state.eg_cnt++;
            oprtr.update_envelope_generator(state.eg_cnt);
        }

        return audio_output;
    }
};

}  // namespace YamahaYM2612

#endif  // DSP_YAMAHA_YM2612_VOICE4OP_HPP_
