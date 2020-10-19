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
// derived from: Game_Music_Emu 0.5.2
// Version 1.4 (final beta)
//

#ifndef DSP_YAMAHA_YM2612_HPP_
#define DSP_YAMAHA_YM2612_HPP_

#include <iostream>
#include "yamaha_ym2612_functional.hpp"

// ---------------------------------------------------------------------------
// MARK: Object-Oriented API
// ---------------------------------------------------------------------------

/// Yamaha YM2612 chip emulator
class YamahaYM2612 {
 public:
    /// the number of FM operators on the module
    static constexpr unsigned NUM_OPERATORS = 4;
    /// the number of independent FM synthesis oscillators on the module
    static constexpr unsigned NUM_VOICES = 6;
    /// the number of FM algorithms on the module
    static constexpr unsigned NUM_ALGORITHMS = 8;

 private:
    /// OPN engine state
    EngineState engine;
    /// channel state
    Voice voices[NUM_VOICES];

    // TODO: remove and replace references with access to internal emulator
    //       data structures
    /// A structure with channel data for a YM2612 voice.
    struct Channel {
        /// the index of the active FM algorithm
        uint8_t AL = 0;
        /// the amount of feedback being applied to operator 1
        uint8_t FB = 0;
        /// the attenuator (and switch) for amplitude modulation from the LFO
        uint8_t AMS = 0;
        /// the attenuator (and switch) for frequency modulation from the LFO
        uint8_t FMS = 0;
        /// the four FM operators for the channel
        struct Operator {
            /// the attack rate
            uint8_t AR = 0;
            /// the 1st decay stage rate
            uint8_t D1 = 0;
            /// the amplitude to start the second decay stage
            uint8_t SL = 0;
            /// the 2nd decay stage rate
            uint8_t D2 = 0;
            /// the release rate
            uint8_t RR = 0;
            /// the total amplitude of the envelope
            uint8_t TL = 0;
            /// the multiplier for the FM frequency
            uint8_t MUL = 0;
            /// the amount of detuning to apply (DET * epsilon + frequency)
            uint8_t DET = 0;
            /// the Rate scale for key-tracking the envelope rates
            uint8_t RS = 0;
            /// whether amplitude modulation from the LFO enabled
            uint8_t AM = 0;
        } operators[NUM_OPERATORS];
    } parameters[NUM_VOICES];

    /// the stereo master output from the chip emulator
    int16_t stereo_output[2] = {0, 0};

 public:
    /// @brief Initialize a new YamahaYM2612 with given sample rate.
    ///
    /// @param clock_rate the underlying clock rate of the system
    /// @param sample_rate the rate to draw samples from the emulator at
    ///
    YamahaYM2612(double clock_rate = 768000, double sample_rate = 44100) {
        engine.voices = voices;
        engine.state.clock = clock_rate;
        engine.state.rate = sample_rate;
        reset();
    }

    /// @brief Set the sample rate the a new value.
    ///
    /// @param clock_rate the underlying clock rate of the system
    /// @param sample_rate the rate to draw samples from the emulator at
    ///
    inline void setSampleRate(double clock_rate, double sample_rate) {
        engine.state.clock = clock_rate;
        engine.state.rate = sample_rate;
        set_prescaler(&engine);
    }

    /// @brief Reset the emulator to its initial state
    inline void reset() {
        stereo_output[0] = stereo_output[1] = 0;
        // set the frequency scaling parameters of the engine emulator
        set_prescaler(&engine);
        // mode 0 , timer reset (address 0x27)
        set_timers(&engine.state, 0x30);
        // envelope generator
        engine.eg_timer = 0;
        engine.eg_cnt = 0;
        // LFO
        engine.lfo_timer = 0;
        engine.lfo_cnt = 0;
        engine.lfo_AM_step = 126;
        engine.lfo_PM_step = 0;
        // state
        engine.state.status = 0;
        engine.state.mode = 0;

        // timer B (address 0x26)
        engine.state.TB = 0x00;
        // timer A Low 2 (address 0x25)
        engine.state.TA = (engine.state.TA & 0x03fc) | (0x00 & 3);
        // timer A High 8 (address 0x24)
        engine.state.TA = (engine.state.TA & 0x0003) | (0x00 << 2);

        reset_voices(&engine.state, &voices[0], NUM_VOICES);

        setLFO(0);
        for (unsigned voice_idx = 0; voice_idx < NUM_VOICES; voice_idx++) {
            Voice& voice = voices[voice_idx];
            // set both bits of pan to enable both channel outputs
            setPAN(voice_idx, 3);
            set_algorithm(&engine, &voice, voice_idx, 0);
            set_feedback(&voice, 0);
            setFREQ(voice_idx, 0);
            setGATE(voice_idx, 0);
            setAMS(voice_idx, 0);
            setFMS(voice_idx, 0);
        }

        for (unsigned i = 0xb2; i >= 0x30; i--) {
            write_register(&engine, i, 0);
            write_register(&engine, i | 0x100, 0);
        }
    }

    /// @brief Run a step on the emulator
    inline void step() {
        int lt, rt;
        // refresh PG and EG
        refresh_fc_eg_chan(&engine, &voices[0]);
        refresh_fc_eg_chan(&engine, &voices[1]);
        refresh_fc_eg_chan(&engine, &voices[2]);
        refresh_fc_eg_chan(&engine, &voices[3]);
        refresh_fc_eg_chan(&engine, &voices[4]);
        refresh_fc_eg_chan(&engine, &voices[5]);
        // clear outputs
        engine.out_fm[0] = 0;
        engine.out_fm[1] = 0;
        engine.out_fm[2] = 0;
        engine.out_fm[3] = 0;
        engine.out_fm[4] = 0;
        engine.out_fm[5] = 0;
        // update SSG-EG output
        update_ssg_eg_channel(voices[0].operators);
        update_ssg_eg_channel(voices[1].operators);
        update_ssg_eg_channel(voices[2].operators);
        update_ssg_eg_channel(voices[3].operators);
        update_ssg_eg_channel(voices[4].operators);
        update_ssg_eg_channel(voices[5].operators);
        // calculate FM
        chan_calc(&engine, &voices[0]);
        chan_calc(&engine, &voices[1]);
        chan_calc(&engine, &voices[2]);
        chan_calc(&engine, &voices[3]);
        chan_calc(&engine, &voices[4]);
        chan_calc(&engine, &voices[5]);
        // advance LFO
        advance_lfo(&engine);
        // advance envelope generator
        engine.eg_timer += engine.eg_timer_add;
        while (engine.eg_timer >= engine.eg_timer_overflow) {
            engine.eg_timer -= engine.eg_timer_overflow;
            engine.eg_cnt++;
            advance_eg_channel(&engine, voices[0].operators);
            advance_eg_channel(&engine, voices[1].operators);
            advance_eg_channel(&engine, voices[2].operators);
            advance_eg_channel(&engine, voices[3].operators);
            advance_eg_channel(&engine, voices[4].operators);
            advance_eg_channel(&engine, voices[5].operators);
        }
        // clip outputs
        if (engine.out_fm[0] > 8191)
            engine.out_fm[0] = 8191;
        else if (engine.out_fm[0] < -8192)
            engine.out_fm[0] = -8192;
        if (engine.out_fm[1] > 8191)
            engine.out_fm[1] = 8191;
        else if (engine.out_fm[1] < -8192)
            engine.out_fm[1] = -8192;
        if (engine.out_fm[2] > 8191)
            engine.out_fm[2] = 8191;
        else if (engine.out_fm[2] < -8192)
            engine.out_fm[2] = -8192;
        if (engine.out_fm[3] > 8191)
            engine.out_fm[3] = 8191;
        else if (engine.out_fm[3] < -8192)
            engine.out_fm[3] = -8192;
        if (engine.out_fm[4] > 8191)
            engine.out_fm[4] = 8191;
        else if (engine.out_fm[4] < -8192)
            engine.out_fm[4] = -8192;
        if (engine.out_fm[5] > 8191)
            engine.out_fm[5] = 8191;
        else if (engine.out_fm[5] < -8192)
            engine.out_fm[5] = -8192;
        // 6-parameters mixing
        lt  = ((engine.out_fm[0] >> 0) & engine.pan[0]);
        rt  = ((engine.out_fm[0] >> 0) & engine.pan[1]);
        lt += ((engine.out_fm[1] >> 0) & engine.pan[2]);
        rt += ((engine.out_fm[1] >> 0) & engine.pan[3]);
        lt += ((engine.out_fm[2] >> 0) & engine.pan[4]);
        rt += ((engine.out_fm[2] >> 0) & engine.pan[5]);
        lt += ((engine.out_fm[3] >> 0) & engine.pan[6]);
        rt += ((engine.out_fm[3] >> 0) & engine.pan[7]);
        lt += ((engine.out_fm[4] >> 0) & engine.pan[8]);
        rt += ((engine.out_fm[4] >> 0) & engine.pan[9]);
        lt += ((engine.out_fm[5] >> 0) & engine.pan[10]);
        rt += ((engine.out_fm[5] >> 0) & engine.pan[11]);
        // output buffering
        stereo_output[0] = lt;
        stereo_output[1] = rt;
        // timer A control
        if ((engine.state.TAC -= static_cast<int>(engine.state.freqbase * 4096)) <= 0)
            timer_A_over(&engine.state);
        // timer B control
        if ((engine.state.TBC -= static_cast<int>(engine.state.freqbase * 4096)) <= 0)
            timer_B_over(&engine.state);
    }

    /// @brief Set the global LFO for the chip.
    ///
    /// @param value the value of the LFO register
    /// @details
    /// ## Mapping values to frequencies in Hz
    /// | value | LFO frequency (Hz)
    /// |:------|:-------------------|
    /// | 0     | 3.98
    /// | 1     | 5.56
    /// | 2     | 6.02
    /// | 3     | 6.37
    /// | 4     | 6.88
    /// | 5     | 9.63
    /// | 6     | 48.1
    /// | 7     | 72.2
    ///
    inline void setLFO(uint8_t value) {
        engine.lfo_timer_overflow = lfo_samples_per_step[value & 7] << LFO_SH;
    }

    // -----------------------------------------------------------------------
    // MARK: Global control for each channel
    // -----------------------------------------------------------------------

    /// @brief Set the frequency for the given channel.
    ///
    /// @param voice_idx the voice on the chip to set the frequency for
    /// @param frequency the frequency value measured in Hz
    ///
    inline void setFREQ(uint8_t voice_idx, float frequency) {
        // cache the voice to set the frequency of
        Voice& voice = voices[voice_idx];
        // Shift the frequency to the base octave and calculate the octave to
        // play. The base octave is defined as a 10-bit number in [0, 1023].
        int octave = 2;
        for (; frequency >= 1024; octave++) frequency /= 2;
        // NOTE: arbitrary shift calculated by producing note from a ground
        //       truth oscillator and comparing the output from YM2612 via
        //       division.
        //       1.458166333006277
        // TODO: why is this arbitrary shift necessary to tune to C4?
        frequency = frequency / 1.458;
        // cast the shifted frequency to a 16-bit container
        const uint16_t freq16bit = frequency;
        // -------------------------------------------------------------------
        // MARK: Frequency Low
        // -------------------------------------------------------------------
        const auto freqLow = freq16bit & 0xff;
        uint32_t fn = (((uint32_t)( (engine.state.fn_h) & 7)) << 8) + freqLow;
        uint8_t blk = engine.state.fn_h >> 3;
        /* key-scale code */
        voice.kcode = (blk << 2) | opn_fktable[(fn >> 7) & 0xf];
        /* phase increment counter */
        voice.fc = engine.fn_table[fn * 2] >> (7 - blk);
        /* store fnum in clear form for LFO PM calculations */
        voice.block_fnum = (blk << 11) | fn;
        voice.operators[Op1].phase_increment = -1;
        // -------------------------------------------------------------------
        // MARK: Frequency High
        // -------------------------------------------------------------------
        const auto freqHigh = ((freq16bit >> 8) & 0x07) | ((octave & 0x07) << 3);
        engine.state.fn_h = freqHigh & 0x3f;
    }

    /// @brief Set the gate for the given voice.
    ///
    /// @param voice_idx the voice on the chip to set the gate for
    /// @param is_open true if the gate is open, false otherwise
    ///
    inline void setGATE(uint8_t voice_idx, bool is_open) {
        set_gate(&engine, (is_open * 0xF0) + ((voice_idx / 3) * 4 + voice_idx % 3));
    }

    /// @brief Set the algorithm (AL) register for the given voice.
    ///
    /// @param voice_idx the voice to set the algorithm register of
    /// @param algorithm the selected FM algorithm in [0, 7]
    ///
    inline void setAL(uint8_t voice_idx, uint8_t algorithm) {
        // TODO: replace with check on voice.algorithm
        if (parameters[voice_idx].AL == algorithm) return;
        parameters[voice_idx].AL = algorithm;
        // get the voice and set the value
        Voice& voice = voices[voice_idx];
        voice.FB_ALG = (voice.FB_ALG & 0x38) | (algorithm & 7);
        set_algorithm(&engine, &voice, voice_idx, algorithm);
    }

    /// @brief Set the feedback (FB) register for the given voice.
    ///
    /// @param voice the voice to set the feedback register of
    /// @param feedback the amount of feedback for operator 1
    ///
    inline void setFB(uint8_t voice_idx, uint8_t feedback) {
        // TODO: replace with check on voice.feedback
        if (parameters[voice_idx].FB == feedback) return;
        parameters[voice_idx].FB = feedback;
        // get the voice and set the value
        Voice& voice = voices[voice_idx];
        voice.FB_ALG = (voice.FB_ALG & 7)| ((feedback & 7) << 3);
        set_feedback(&voice, feedback);
    }

    /// @brief Set the AM sensitivity (AMS) register for the given voice.
    ///
    /// @param voice_idx the voice to set the AM sensitivity register of
    /// @param ams the amount of amplitude modulation (AM) sensitivity
    ///
    inline void setAMS(uint8_t voice_idx, uint8_t ams) {
        // TODO: replace with check on voice.LR_AMS_FMS
        if (parameters[voice_idx].AMS == ams) return;
        parameters[voice_idx].AMS = ams;
        // get the voice and set the value
        Voice& voice = voices[voice_idx];
        voice.LR_AMS_FMS = (voice.LR_AMS_FMS & 0xCF)| ((ams & 3) << 4);
        voice.ams = lfo_ams_depth_shift[ams & 0x03];
    }

    /// @brief Set the FM sensitivity (FMS) register for the given voice.
    ///
    /// @param voice_idx the voice to set the FM sensitivity register of
    /// @param value the amount of frequency modulation (FM) sensitivity
    ///
    inline void setFMS(uint8_t voice_idx, uint8_t fms) {
        // TODO: replace with check on voice.LR_AMS_FMS
        if (parameters[voice_idx].FMS == fms) return;
        parameters[voice_idx].FMS = fms;
        // get the voice and set the value
        Voice& voice = voices[voice_idx];
        voice.LR_AMS_FMS = (voice.LR_AMS_FMS & 0xF8)| (fms & 7);
        voice.pms = (fms & 7) * 32;
    }

    /// @brief Set the state (ST) register for the given voice, i.e., the pan.
    ///
    /// @param voice the voice to set the state register of
    /// @param state the value of the state register. the first bit enables the
    /// right channel. the second bit enables the left channel
    ///
    inline void setPAN(uint8_t voice_idx, uint8_t state) {
        // TODO: replace with check on voice.LR_AMS_FMS
        // if (parameters[voice_idx].PAN == state) return;
        // parameters[voice_idx].PAN = state;
        // get the voice and set the value
        Voice& voice = voices[voice_idx];
        voice.LR_AMS_FMS = (voice.LR_AMS_FMS & 0x3F)| ((state & 3) << 6);
        set_pan(&engine, voice_idx, state);
    }

    // -----------------------------------------------------------------------
    // MARK: Operator control for each channel
    // -----------------------------------------------------------------------

    /// @brief Set the SSG-envelope register for the given channel and operator.
    ///
    /// @param voice the channel to set the SSG-EG register of (in [0, 6])
    /// @param op_index the operator to set the SSG-EG register of (in [0, 3])
    /// @param is_on whether the looping envelope generator should be turned on
    /// @param mode the mode for the looping generator to run in (in [0, 7])
    /// @details
    /// The mode can be any of the following:
    ///
    /// Table: SSG-EG LFO Patterns
    /// | At | Al | H | LFO Pattern |
    /// |:---|:---|:--|:------------|
    /// | 0  | 0  | 0 |  \\\\       |
    /// |    |    |   |             |
    /// | 0  | 0  | 1 |  \___       |
    /// |    |    |   |             |
    /// | 0  | 1  | 0 |  \/\/       |
    /// |    |    |   |             |
    /// |    |    |   |   ___       |
    /// | 0  | 1  | 1 |  \          |
    /// |    |    |   |             |
    /// | 1  | 0  | 0 |  ////       |
    /// |    |    |   |             |
    /// |    |    |   |   ___       |
    /// | 1  | 0  | 1 |  /          |
    /// |    |    |   |             |
    /// | 1  | 1  | 0 |  /\/\       |
    /// |    |    |   |             |
    /// | 1  | 1  | 1 |  /___       |
    /// |    |    |   |             |
    ///
    /// The shapes are generated using Attack, Decay and Sustain phases.
    ///
    /// Each single character in the diagrams above represents this whole
    /// sequence:
    ///
    /// - when KEY-ON = 1, normal Attack phase is generated (*without* any
    ///   difference when compared to normal mode),
    ///
    /// - later, when envelope level reaches minimum level (max volume),
    ///   the EG switches to Decay phase (which works with bigger steps
    ///   when compared to normal mode - see below),
    ///
    /// - later when envelope level passes the SL level,
    ///   the EG swithes to Sustain phase (which works with bigger steps
    ///   when compared to normal mode - see below),
    ///
    /// - finally when envelope level reaches maximum level (min volume),
    ///   the EG switches to Attack phase again (depends on actual waveform).
    ///
    /// Important is that when switch to Attack phase occurs, the phase counter
    /// of that operator will be zeroed-out (as in normal KEY-ON) but not always.
    /// (I haven't found the rule for that - perhaps only when the output
    /// level is low)
    ///
    /// The difference (when compared to normal Envelope Generator mode) is
    /// that the resolution in Decay and Sustain phases is 4 times lower;
    /// this results in only 256 steps instead of normal 1024.
    /// In other words:
    /// when SSG-EG is disabled, the step inside of the EG is one,
    /// when SSG-EG is enabled, the step is four (in Decay and Sustain phases).
    ///
    /// Times between the level changes are the same in both modes.
    ///
    /// Important:
    /// Decay 1 Level (so called SL) is compared to actual SSG-EG output, so
    /// it is the same in both SSG and no-SSG modes, with this exception:
    ///
    /// when the SSG-EG is enabled and is generating raising levels
    /// (when the EG output is inverted) the SL will be found at wrong level!!!
    /// For example, when SL=02:
    ///     0 -6 = -6dB in non-inverted EG output
    ///     96-6 = -90dB in inverted EG output
    /// Which means that EG compares its level to SL as usual, and that the
    /// output is simply inverted after all.
    ///
    /// The Yamaha's manuals say that AR should be set to 0x1f (max speed).
    /// That is not necessary, but then EG will be generating Attack phase.
    ///
    inline void setSSG(uint8_t voice, uint8_t op_index, bool is_on, uint8_t mode) {
        // get the value for the SSG register. the high bit determines whether
        // SSG mode is on and the low three bits determine the mode
        const uint8_t value = (is_on << 3) | (mode & 7);
        // get the operator and check if the value has changed. If there is no
        // change return, otherwise set the value and proceed
        Operator* const oprtr = &voices[voice].operators[OPERATOR_INDEXES[op_index]];
        if (oprtr->ssg == value) return;
        oprtr->ssg = value;
        // recalculate EG output
        if ((oprtr->ssg & 0x08) && (oprtr->ssgn ^ (oprtr->ssg & 0x04)) && (oprtr->state > EG_REL))
            oprtr->vol_out = ((uint32_t) (0x200 - oprtr->volume) & MAX_ATT_INDEX) + oprtr->tl;
        else
            oprtr->vol_out = (uint32_t) oprtr->volume + oprtr->tl;
    }

    /// @brief Set the attack rate (AR) register for the given voice and operator.
    ///
    /// @param voice the voice to set the attack rate (AR) register of (in [0, 6])
    /// @param op_index the operator to set the attack rate (AR) register of (in [0, 3])
    /// @param value the rate of the attack stage of the envelope generator
    ///
    inline void setAR(uint8_t voice, uint8_t op_index, uint8_t value) {
        // TODO: replace with check on oprtr->ar_ksr
        if (parameters[voice].operators[op_index].AR == value) return;
        parameters[voice].operators[op_index].AR = value;
        Operator* const oprtr = &voices[voice].operators[OPERATOR_INDEXES[op_index]];
        oprtr->ar_ksr = (oprtr->ar_ksr & 0xC0) | (value & 0x1f);
        set_ar_ksr(&voices[voice], oprtr, oprtr->ar_ksr);
    }

    /// @brief Set the 1st decay rate (D1) register for the given voice and operator.
    ///
    /// @param voice the voice to set the 1st decay rate (D1) register of (in [0, 6])
    /// @param op_index the operator to set the 1st decay rate (D1) register of (in [0, 3])
    /// @param value the rate of decay for the 1st decay stage of the envelope generator
    ///
    inline void setD1(uint8_t voice, uint8_t op_index, uint8_t value) {
        // TODO: replace with check on oprtr->dr
        if (parameters[voice].operators[op_index].D1 == value) return;
        parameters[voice].operators[op_index].D1 = value;
        Operator* const oprtr = &voices[voice].operators[OPERATOR_INDEXES[op_index]];
        oprtr->dr = (oprtr->dr & 0x80) | (value & 0x1F);
        set_dr(oprtr, oprtr->dr);
    }

    /// @brief Set the sustain level (SL) register for the given voice and operator.
    ///
    /// @param voice the voice to set the sustain level (SL) register of (in [0, 6])
    /// @param op_index the operator to set the sustain level (SL) register of (in [0, 3])
    /// @param value the amplitude level at which the 2nd decay stage of the envelope generator begins
    ///
    inline void setSL(uint8_t voice, uint8_t op_index, uint8_t value) {
        // TODO: replace with check on oprtr->sl_rr
        if (parameters[voice].operators[op_index].SL == value) return;
        parameters[voice].operators[op_index].SL = value;
        Operator* const oprtr =  &voices[voice].operators[OPERATOR_INDEXES[op_index]];
        oprtr->sl_rr = (oprtr->sl_rr & 0x0f) | ((value & 0x0f) << 4);
        set_sl_rr(oprtr, oprtr->sl_rr);
    }

    /// @brief Set the 2nd decay rate (D2) register for the given voice and operator.
    ///
    /// @param voice the voice to set the 2nd decay rate (D2) register of (in [0, 6])
    /// @param op_index the operator to set the 2nd decay rate (D2) register of (in [0, 3])
    /// @param value the rate of decay for the 2nd decay stage of the envelope generator
    ///
    inline void setD2(uint8_t voice, uint8_t op_index, uint8_t value) {
        // TODO: replace with check on oprtr->sr
        if (parameters[voice].operators[op_index].D2 == value) return;
        parameters[voice].operators[op_index].D2 = value;
        set_sr(&voices[voice].operators[OPERATOR_INDEXES[op_index]], value);
    }

    /// @brief Set the release rate (RR) register for the given voice and operator.
    ///
    /// @param voice the voice to set the release rate (RR) register of (in [0, 6])
    /// @param op_index the operator to set the release rate (RR) register of (in [0, 3])
    /// @param value the rate of release of the envelope generator after key-off
    ///
    inline void setRR(uint8_t voice, uint8_t op_index, uint8_t value) {
        // TODO: replace with check on oprtr->sl_rr
        if (parameters[voice].operators[op_index].RR == value) return;
        parameters[voice].operators[op_index].RR = value;
        Operator* const oprtr =  &voices[voice].operators[OPERATOR_INDEXES[op_index]];
        oprtr->sl_rr = (oprtr->sl_rr & 0xf0) | (value & 0x0f);
        set_sl_rr(oprtr, oprtr->sl_rr);
    }

    /// @brief Set the total level (TL) register for the given voice and operator.
    ///
    /// @param voice the voice to set the total level (TL) register of (in [0, 6])
    /// @param op_index the operator to set the total level (TL) register of (in [0, 3])
    /// @param value the total amplitude of envelope generator
    ///
    inline void setTL(uint8_t voice, uint8_t op_index, uint8_t value) {
        // TODO: replace with check on oprtr->tl
        if (parameters[voice].operators[op_index].TL == value) return;
        parameters[voice].operators[op_index].TL = value;
        set_tl(&voices[voice].operators[OPERATOR_INDEXES[op_index]], value);
    }

    /// @brief Set the multiplier (MUL) register for the given voice and operator.
    ///
    /// @param voice the voice to set the multiplier (MUL) register of (in [0, 6])
    /// @param op_index the operator to set the multiplier  (MUL)register of (in [0, 3])
    /// @param value the value of the FM phase multiplier
    ///
    inline void setMUL(uint8_t voice, uint8_t op_index, uint8_t value) {
        // TODO: replace with check on oprtr->mul
        if (parameters[voice].operators[op_index].MUL == value) return;
        parameters[voice].operators[op_index].MUL = value;
        voices[voice].operators[OPERATOR_INDEXES[op_index]].mul = (value & 0x0f) ? (value & 0x0f) * 2 : 1;
        voices[voice].operators[Op1].phase_increment = -1;
    }

    /// @brief Set the detune (DET) register for the given voice and operator.
    ///
    /// @param voice the voice to set the detune (DET) register of (in [0, 6])
    /// @param op_index the operator to set the detune (DET) register of (in [0, 3])
    /// @param value the the level of detuning for the FM operator
    ///
    inline void setDET(uint8_t voice, uint8_t op_index, uint8_t value) {
        // TODO: replace with check on oprtr->DT
        if (parameters[voice].operators[op_index].DET == value) return;
        parameters[voice].operators[op_index].DET = value;
        voices[voice].operators[OPERATOR_INDEXES[op_index]].DT = engine.state.dt_tab[value & 7];
        voices[voice].operators[Op1].phase_increment = -1;
    }

    /// @brief Set the rate-scale (RS) register for the given voice and operator.
    ///
    /// @param voice the voice to set the rate-scale (RS) register of (in [0, 6])
    /// @param op_index the operator to set the rate-scale (RS) register of (in [0, 3])
    /// @param value the amount of rate-scale applied to the FM operator
    ///
    inline void setRS(uint8_t voice, uint8_t op_index, uint8_t value) {
        // TODO: replace with check on oprtr->ar_ksr
        if (parameters[voice].operators[op_index].RS == value) return;
        parameters[voice].operators[op_index].RS = value;
        Operator* const oprtr = &voices[voice].operators[OPERATOR_INDEXES[op_index]];
        oprtr->ar_ksr = (oprtr->ar_ksr & 0x1F) | ((value & 0x03) << 6);
        set_ar_ksr(&voices[voice], oprtr, oprtr->ar_ksr);
    }

    /// @brief Set the amplitude modulation (AM) register for the given voice and operator.
    ///
    /// @param voice the voice to set the amplitude modulation (AM) register of (in [0, 6])
    /// @param op_index the operator to set the amplitude modulation (AM) register of (in [0, 3])
    /// @param value the true to enable amplitude modulation from the LFO, false to disable it
    ///
    inline void setAM(uint8_t voice, uint8_t op_index, uint8_t value) {
        // TODO: replace with check on oprtr->AMmask
        if (parameters[voice].operators[op_index].AM == value) return;
        parameters[voice].operators[op_index].AM = value;
        Operator* const oprtr = &voices[voice].operators[OPERATOR_INDEXES[op_index]];
        oprtr->AMmask = (value) ? ~0 : 0;
    }

    // -----------------------------------------------------------------------
    // MARK: Emulator output
    // -----------------------------------------------------------------------

    /// @brief Return the output from the left lane of the mix output.
    ///
    /// @returns the left lane of the mix output
    ///
    inline int16_t getOutputLeft() { return stereo_output[0]; }

    /// @brief Return the output from the right lane of the mix output.
    ///
    /// @returns the right lane of the mix output
    ///
    inline int16_t getOutputRight() { return stereo_output[1]; }

    /// @brief Return the voltage from the left lane of the mix output.
    ///
    /// @returns the voltage of the left lane of the mix output
    ///
    inline float getVoltageLeft() {
        return static_cast<float>(stereo_output[0]) / std::numeric_limits<int16_t>::max();
    }

    /// @brief Return the voltage from the right lane of the mix output.
    ///
    /// @returns the voltage of the right lane of the mix output
    ///
    inline float getVoltageRight() {
        return static_cast<float>(stereo_output[1]) / std::numeric_limits<int16_t>::max();
    }
};

#endif  // DSP_YAMAHA_YM2612_HPP_
