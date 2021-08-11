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
struct Voice4Op {
 public:
    /// the number of FM operators on the module
    static constexpr unsigned NUM_OPERATORS = 4;
    /// the number of FM algorithms on the module
    static constexpr unsigned NUM_ALGORITHMS = 8;

 private:
    /// general state
    OperatorContext state;
    /// four operators
    Operator operators[NUM_OPERATORS];

    /// a flag determining whether the phase increment needs to be updated
    bool update_phase_increment = false;

    /// the currently selected algorithm
    uint8_t algorithm = 0;
    /// feedback shift
    uint8_t feedback = 0;

    /// operator 1 output for feedback
    int32_t op1_out[2] = {0, 0};

    /// Phase Modulation input for operator 2
    int32_t m2 = 0;
    /// Phase Modulation input for operator 3
    int32_t c1 = 0;
    /// Phase Modulation input for operator 4
    int32_t c2 = 0;
    /// one sample delay memory
    int32_t mem = 0;

    /// the output of the operators based on the algorithm connections
    int32_t* connections[NUM_OPERATORS];
    /// where to put the delayed sample (MEM)
    int32_t *mem_connect = nullptr;
    /// delayed sample (MEM) value
    int32_t mem_value = 0;

    /// the last output sample from the voice
    int32_t audio_output = 0;

 public:
    /// @brief Initialize a new YamahaYM2612 with given sample rate.
    ///
    /// @param sample_rate the rate to draw samples from the emulator at
    /// @param clock_rate the underlying clock rate of the system
    ///
    explicit Voice4Op(float sample_rate = 44100, float clock_rate = 768000) {
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
        for (auto &op : operators) op.reset(state);
        algorithm = 0;
        feedback = 0;
        op1_out[0] = op1_out[1] = 0;
        memset(connections, 0, sizeof connections);
        mem_connect = nullptr;
        mem_value = 0;
        set_algorithm(0);
        update_phase_increment = true;
    }

    // -----------------------------------------------------------------------
    // MARK: Parameter Setters
    // -----------------------------------------------------------------------

    /// @brief Set algorithm, i.e., operator routing.
    ///
    /// @param value the algorithm / routing to set the voice to
    ///
    inline void set_algorithm(uint8_t value) {
        algorithm = value & 7;
        int32_t *carrier = &audio_output;
        // get the connections
        int32_t **om1 = &connections[Op1];
        int32_t **om2 = &connections[Op3];
        int32_t **oc1 = &connections[Op2];
        int32_t **memc = &mem_connect;
        // set the algorithm
        switch (algorithm) {
        case 0:
            // M1---C1---MEM---M2---C2---OUT
            *om1 = &c1;
            *oc1 = &mem;
            *om2 = &c2;
            *memc = &m2;
            break;
        case 1:
            // M1------+-MEM---M2---C2---OUT
            //      C1-+
            *om1 = &mem;
            *oc1 = &mem;
            *om2 = &c2;
            *memc = &m2;
            break;
        case 2:
            // M1-----------------+-C2---OUT
            //      C1---MEM---M2-+
            *om1 = &c2;
            *oc1 = &mem;
            *om2 = &c2;
            *memc = &m2;
            break;
        case 3:
            // M1---C1---MEM------+-C2---OUT
            //                 M2-+
            *om1 = &c1;
            *oc1 = &mem;
            *om2 = &c2;
            *memc = &c2;
            break;
        case 4:
            // M1---C1-+-OUT
            // M2---C2-+
            // MEM: not used
            *om1 = &c1;
            *oc1 = carrier;
            *om2 = &c2;
            *memc = &mem;  // store it anywhere where it will not be used
            break;
        case 5:
            //    +----C1----+
            // M1-+-MEM---M2-+-OUT
            //    +----C2----+
            *om1 = nullptr;  // special mark
            *oc1 = carrier;
            *om2 = carrier;
            *memc = &m2;
            break;
        case 6:
            // M1---C1-+
            //      M2-+-OUT
            //      C2-+
            // MEM: not used
            *om1 = &c1;
            *oc1 = carrier;
            *om2 = carrier;
            *memc = &mem;  // store it anywhere where it will not be used
            break;
        case 7:
            // M1-+
            // C1-+-OUT
            // M2-+
            // C2-+
            // MEM: not used
            *om1 = carrier;
            *oc1 = carrier;
            *om2 = carrier;
            *memc = &mem;  // store it anywhere where it will not be used
            break;
        }
        connections[Op4] = carrier;
    }


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

    // -----------------------------------------------------------------------
    // MARK: Operator Parameter Settings
    // -----------------------------------------------------------------------

    /// @brief Set the frequency of the voice.
    ///
    /// @param frequency the frequency value measured in Hz
    ///
    inline void set_frequency(uint8_t op_index, float frequency) {
        update_phase_increment |= operators[OPERATOR_INDEXES[op_index]].set_frequency(state, frequency);
    }

    /// @brief Set the gate for the given voice.
    ///
    /// @param op_index the operator to set the gate of of (in [0, 3])
    /// @param is_open true if the gate is open, false otherwise
    /// @param prevent_clicks true to prevent clicks from note re-triggers
    ///
    inline void set_gate(uint8_t op_index, bool is_open, bool prevent_clicks = false) {
        operators[OPERATOR_INDEXES[op_index]].set_gate(is_open, prevent_clicks);
    }

    /// @brief Set the rate-scale (RS) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the rate-scale (RS) register of (in [0, 3])
    /// @param value the amount of rate-scale applied to the FM operator
    ///
    inline void set_rate_scale(uint8_t op_index, uint8_t value) {
        update_phase_increment |= operators[OPERATOR_INDEXES[op_index]].set_rate_scale(value);
    }

    /// @brief Set the attack rate (AR) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the attack rate (AR) register of (in [0, 3])
    /// @param value the rate of the attack stage of the envelope generator
    ///
    inline void set_attack_rate(uint8_t op_index, uint8_t value) {
        operators[OPERATOR_INDEXES[op_index]].set_attack_rate(value);
    }

    /// @brief Set the total level (TL) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the total level (TL) register of (in [0, 3])
    /// @param value the total amplitude of envelope generator
    ///
    inline void set_total_level(uint8_t op_index, uint8_t value) {
        operators[OPERATOR_INDEXES[op_index]].set_total_level(value);
    }

    /// @brief Set the 1st decay rate (D1) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the 1st decay rate (D1) register of (in [0, 3])
    /// @param value the rate of decay for the 1st decay stage of the envelope generator
    ///
    inline void set_decay_rate(uint8_t op_index, uint8_t value) {
        operators[OPERATOR_INDEXES[op_index]].set_decay_rate(value);
    }

    /// @brief Set the sustain level (SL) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the sustain level (SL) register of (in [0, 3])
    /// @param value the amplitude level at which the 2nd decay stage of the envelope generator begins
    ///
    inline void set_sustain_level(uint8_t op_index, uint8_t value) {
        operators[OPERATOR_INDEXES[op_index]].set_sustain_level(value);
    }

    /// @brief Set the 2nd decay rate (D2) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the 2nd decay rate (D2) register of (in [0, 3])
    /// @param value the rate of decay for the 2nd decay stage of the envelope generator
    ///
    inline void set_sustain_rate(uint8_t op_index, uint8_t value) {
        operators[OPERATOR_INDEXES[op_index]].set_sustain_rate(value);
    }

    /// @brief Set the release rate (RR) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the release rate (RR) register of (in [0, 3])
    /// @param value the rate of release of the envelope generator after key-off
    ///
    inline void set_release_rate(uint8_t op_index, uint8_t value) {
        operators[OPERATOR_INDEXES[op_index]].set_release_rate(value);
    }

    /// @brief Set the multiplier (MUL) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the multiplier  (MUL)register of (in [0, 3])
    /// @param value the value of the FM phase multiplier
    ///
    inline void set_multiplier(uint8_t op_index, uint8_t value) {
        update_phase_increment |= operators[OPERATOR_INDEXES[op_index]].set_multiplier(value);
    }

    /// @brief Set the detune (DET) register for the given voice and operator.
    ///
    /// @param op_index the operator to set the detune (DET) register of (in [0, 3])
    /// @param value the the level of detuning for the FM operator
    ///
    inline void set_detune(uint8_t op_index, uint8_t value = 4) {
        update_phase_increment |= operators[OPERATOR_INDEXES[op_index]].set_detune(state, value);
    }

    /// @brief Set whether SSG envelopes are enabled for the given operator.
    ///
    /// @param op_index the operator to set the SSG-EG register of (in [0, 3])
    /// @param is_on whether the looping envelope generator should be turned on
    ///
    inline void set_ssg_enabled(uint8_t op_index, bool is_on) {
        operators[OPERATOR_INDEXES[op_index]].set_ssg_enabled(is_on);
    }

    /// @brief Set the AM sensitivity (AMS) register for the given voice.
    ///
    /// @param value the amount of amplitude modulation (AM) sensitivity
    ///
    inline void set_am_sensitivity(uint8_t op_index, uint8_t value) {
        operators[OPERATOR_INDEXES[op_index]].set_am_sensitivity(value);
    }

    /// @brief Set the FM sensitivity (FMS) register for the given voice.
    ///
    /// @param value the amount of frequency modulation (FM) sensitivity
    ///
    inline void set_fm_sensitivity(uint8_t op_index, uint8_t value) {
        operators[OPERATOR_INDEXES[op_index]].set_fm_sensitivity(value);
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
            for (Operator& oprtr : operators)
                oprtr.refresh_phase_and_envelope();
            update_phase_increment = false;
        }
        // clear the audio output
        audio_output = 0;
        // update the SSG envelope
        for (Operator& oprtr : operators)
            oprtr.update_ssg_envelope_generator();
        // -------------------------------------------------------------------
        // calculate operator outputs
        // -------------------------------------------------------------------
        // reset the algorithm outputs to 0
        m2 = c1 = c2 = mem = 0;
        // restore delayed sample (MEM) value to m2 or c2
        *mem_connect = mem_value;
        // Operator 1
        unsigned envelope = operators[Op1].get_envelope(state);
        // sum [t-2] sample with [t-1] sample as the feedback carrier for op1
        int32_t feedback_carrier = op1_out[0] + op1_out[1];
        // set the [t-2] sample as the [t-1] sample (i.e., step the history)
        op1_out[0] = op1_out[1];
        // set the connection outputs from operator 1 based on the algorithm
        if (!connections[Op1])  // algorithm 5
            mem = c1 = c2 = op1_out[1];
        else  // other algorithms
            *connections[Op1] += op1_out[1];
        // calculate the next output from operator 1
        if (envelope < ENV_QUIET) {  // operator 1 envelope is open
            // if feedback is disabled, set feedback carrier to 0
            if (!feedback) feedback_carrier = 0;
            // shift carrier by the feedback amount
            op1_out[1] = operators[Op1].calculate_output(envelope, feedback_carrier << feedback);
        } else {  // clear the next output from operator 1
            op1_out[1] = 0;
        }
        // Operator 3
        envelope = operators[Op3].get_envelope(state);
        if (envelope < ENV_QUIET)
            *connections[Op3] += operators[Op3].calculate_output(envelope, m2 << 15);
        // Operator 2
        envelope = operators[Op2].get_envelope(state);
        if (envelope < ENV_QUIET)
            *connections[Op2] += operators[Op2].calculate_output(envelope, c1 << 15);
        // Operator 4
        envelope = operators[Op4].get_envelope(state);
        if (envelope < ENV_QUIET)
            *connections[Op4] += operators[Op4].calculate_output(envelope, c2 << 15);
        // store current MEM
        mem_value = mem;
        // update phase counters AFTER output calculations
        for (Operator& oprtr : operators)
            oprtr.update_phase_counters(state);
        // -------------------------------------------------------------------
        // advance LFO & envelope generator
        // -------------------------------------------------------------------
        state.advance_lfo();
        state.eg_timer += state.eg_timer_add;
        while (state.eg_timer >= state.eg_timer_overflow) {
            state.eg_timer -= state.eg_timer_overflow;
            state.eg_cnt++;
            for (Operator& oprtr : operators)
                oprtr.update_envelope_generator(state.eg_cnt);
        }

        return audio_output;
    }
};

}  // namespace YamahaYM2612

#endif  // DSP_YAMAHA_YM2612_VOICE4OP_HPP_
