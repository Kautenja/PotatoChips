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
    /// outputs of working channels
    int32_t out_fm;

    /// Phase Modulation input for operator 2
    int32_t m2 = 0;
    /// Phase Modulation input for operator 3
    int32_t c1 = 0;
    /// Phase Modulation input for operator 4
    int32_t c2 = 0;
    /// one sample delay memory
    int32_t mem = 0;

    // TODO: enum for the operator/algorithm?
    // TODO: better ASCII illustrations of the operators
    /// @brief Set algorithm, i.e., operator routing.
    ///
    /// @param algorithm the algorithm to set the voice to
    ///
    inline void set_algorithm(uint8_t algorithm) {
        // get the voice and carrier wave
        voice.algorithm = algorithm & 7;
        int32_t *carrier = &out_fm;
        // get the connections
        int32_t **om1 = &voice.connections[Op1];
        int32_t **om2 = &voice.connections[Op3];
        int32_t **oc1 = &voice.connections[Op2];
        int32_t **memc = &voice.mem_connect;
        // set the algorithm
        switch (voice.algorithm) {
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
        voice.connections[Op4] = carrier;
    }

    /// @brief Update phase increment counters
    inline void refresh_fc_eg_chan() {
        if (!voice.update_phase_increment) return;
        int fc = voice.fc;
        int kc = voice.kcode;
        voice.operators[Op1].refresh_fc_eg_slot(state.fn_max, fc, kc);
        voice.operators[Op2].refresh_fc_eg_slot(state.fn_max, fc, kc);
        voice.operators[Op3].refresh_fc_eg_slot(state.fn_max, fc, kc);
        voice.operators[Op4].refresh_fc_eg_slot(state.fn_max, fc, kc);
        voice.update_phase_increment = false;
    }

    inline void update_phase_lfo_channel() {
        uint32_t block_fnum = voice.block_fnum;
        uint32_t fnum_lfo = ((block_fnum & 0x7f0) >> 4) * 32 * 8;
        int32_t lfo_fn_table_index_offset = LFO_PM_TABLE[fnum_lfo + voice.pms + state.lfo_PM_step];
        if (lfo_fn_table_index_offset) {  // LFO phase modulation active
            block_fnum = block_fnum * 2 + lfo_fn_table_index_offset;
            uint8_t blk = (block_fnum & 0x7000) >> 12;
            uint32_t fn = block_fnum & 0xfff;
            // key-scale code
            int kc = (blk << 2) | FREQUENCY_KEYCODE_TABLE[fn >> 8];
            // phase increment counter
            int fc = (state.fn_table[fn] >> (7 - blk));
            // detects frequency overflow (credits to Nemesis)
            int finc = fc + voice.operators[Op1].DT[kc];
            // Operator 1
            if (finc < 0) finc += state.fn_max;
            voice.operators[Op1].phase += (finc * voice.operators[Op1].mul) >> 1;
            // Operator 2
            finc = fc + voice.operators[Op2].DT[kc];
            if (finc < 0) finc += state.fn_max;
            voice.operators[Op2].phase += (finc * voice.operators[Op2].mul) >> 1;
            // Operator 3
            finc = fc + voice.operators[Op3].DT[kc];
            if (finc < 0) finc += state.fn_max;
            voice.operators[Op3].phase += (finc * voice.operators[Op3].mul) >> 1;
            // Operator 4
            finc = fc + voice.operators[Op4].DT[kc];
            if (finc < 0) finc += state.fn_max;
            voice.operators[Op4].phase += (finc * voice.operators[Op4].mul) >> 1;
        } else {  // LFO phase modulation is 0
            voice.operators[Op1].phase += voice.operators[Op1].phase_increment;
            voice.operators[Op2].phase += voice.operators[Op2].phase_increment;
            voice.operators[Op3].phase += voice.operators[Op3].phase_increment;
            voice.operators[Op4].phase += voice.operators[Op4].phase_increment;
        }
    }

    /// @brief Advance the operators to compute the next output from the voice.
    /// @details
    /// The operators advance in the order 1, 3, 2, 4.
    ///
    inline void calculate_operator_outputs() {
        // get the amount of amplitude modulation for the voice
        const uint32_t AM = state.lfo_AM_step >> voice.ams;
        // reset the algorithm outputs to 0
        m2 = c1 = c2 = mem = 0;
        // restore delayed sample (MEM) value to m2 or c2
        *voice.mem_connect = voice.mem_value;
        // Operator 1
        unsigned envelope = voice.operators[Op1].get_envelope(AM);;
        // sum [t-2] sample with [t-1] sample as the feedback carrier for op1
        int32_t feedback = voice.op1_out[0] + voice.op1_out[1];
        // set the [t-2] sample as the [t-1] sample (i.e., step the history)
        voice.op1_out[0] = voice.op1_out[1];
        // set the connection outputs from operator 1 based on the algorithm
        if (!voice.connections[Op1])  // algorithm 5
            mem = c1 = c2 = voice.op1_out[1];
        else  // other algorithms
            *voice.connections[Op1] += voice.op1_out[1];
        // calculate the next output from operator 1
        if (envelope < ENV_QUIET) {  // operator 1 envelope is open
            // if feedback is disabled, set feedback carrier to 0
            if (!voice.feedback) feedback = 0;
            // shift carrier by the feedback amount
            voice.op1_out[1] = voice.operators[Op1].calculate_output(envelope, feedback << voice.feedback);
        } else {  // clear the next output from operator 1
            voice.op1_out[1] = 0;
        }
        // Operator 3
        envelope = voice.operators[Op3].get_envelope(AM);;
        if (envelope < ENV_QUIET)
            *voice.connections[Op3] += voice.operators[Op3].calculate_output(envelope, m2 << 15);
        // Operator 2
        envelope = voice.operators[Op2].get_envelope(AM);
        if (envelope < ENV_QUIET)
            *voice.connections[Op2] += voice.operators[Op2].calculate_output(envelope, c1 << 15);
        // Operator 4
        envelope = voice.operators[Op4].get_envelope(AM);
        if (envelope < ENV_QUIET)
            *voice.connections[Op4] += voice.operators[Op4].calculate_output(envelope, c2 << 15);
        // store current MEM
        voice.mem_value = mem;
        // update phase counters AFTER output calculations
        if (voice.pms) {  // update the phase using the LFO
            update_phase_lfo_channel();
        } else {  // no LFO phase modulation
            voice.operators[Op1].phase += voice.operators[Op1].phase_increment;
            voice.operators[Op2].phase += voice.operators[Op2].phase_increment;
            voice.operators[Op3].phase += voice.operators[Op3].phase_increment;
            voice.operators[Op4].phase += voice.operators[Op4].phase_increment;
        }
    }

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
    inline void reset() {
        state.reset();
        voice.reset(state);
        // TODO: move the following reset code to voice.reset()
        setAL(0);
        voice.update_phase_increment = true;
    }

    /// @brief Run a step on the emulator to produce a sample.
    ///
    /// @returns a 16-bit PCM sample from the synthesizer
    ///
    inline int16_t step() {
        // refresh PG and EG
        refresh_fc_eg_chan();
        // clear outputs
        out_fm = 0;
        // update SSG-EG output
        for (Operator& oprtr : voice.operators)
            oprtr.update_ssg_eg_channel();
        // calculate FM
        calculate_operator_outputs();
        // advance LFO
        state.advance_lfo();
        // advance envelope generator
        state.eg_timer += state.eg_timer_add;
        while (state.eg_timer >= state.eg_timer_overflow) {
            state.eg_timer -= state.eg_timer_overflow;
            state.eg_cnt++;
            for (Operator& oprtr : voice.operators)
                oprtr.update_eg_channel(state.eg_cnt);
        }
        // clip the output to 14-bits
        // TODO: output clipping indicator
        if (out_fm > Operator::OUTPUT_MAX) {
            out_fm = Operator::OUTPUT_MAX;
        } else if (out_fm < Operator::OUTPUT_MIN) {
            out_fm = Operator::OUTPUT_MIN;
        }
        return out_fm;
    }

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
    inline void setAL(uint8_t value) { set_algorithm(value); }

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
        // set the value for the SSG register. the high bit determines whether
        // SSG mode is on and the low three bits determine the mode
        voice.operators[OPERATOR_INDEXES[op_index]].set_ssg((is_on << 3) | (mode & 7));
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
