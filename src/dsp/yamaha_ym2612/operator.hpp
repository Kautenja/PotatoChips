// An abstraction of a single operator from the Yamaha YM2612.
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

#ifndef DSP_YAMAHA_YM2612_OPERATOR_HPP_
#define DSP_YAMAHA_YM2612_OPERATOR_HPP_

#include "../exceptions.hpp"
#include "tables.hpp"

/// @brief Yamaha YM2612 emulation components.
namespace YamahaYM2612 {

/// @brief The global data for all FM operators.
struct OperatorContext {
    /// frequency base
    float freqbase = 0;

    /// there are 2048 FNUMs that can be generated using FNUM/BLK registers
    /// but LFO works with one more bit of a precision so we really need 4096
    /// elements. fnumber->increment counter
    uint32_t fnum_table[4096];
    /// maximal phase increment (used for phase overflow)
    uint32_t fnum_max = 0;

    /// DETune table
    int32_t dt_table[8][32];

    /// global envelope generator counter
    uint32_t eg_cnt = 0;
    /// global envelope generator counter works at frequency = chipclock/144/3
    uint32_t eg_timer = 0;
    /// step of eg_timer
    uint32_t eg_timer_add = 0;
    /// envelope generator timer overflows every 3 samples (on real chip)
    uint32_t eg_timer_overflow = 0;

    /// current LFO phase (out of 128)
    uint8_t lfo_cnt = 0;
    /// current LFO phase runs at LFO frequency
    uint32_t lfo_timer = 0;
    /// step of lfo_timer
    uint32_t lfo_timer_add = 0;
    /// LFO timer overflows every N samples (depends on LFO frequency)
    uint32_t lfo_timer_overflow = 0;
    /// current LFO AM step
    uint32_t lfo_AM_step = 0;
    /// current LFO PM step
    uint32_t lfo_PM_step = 0;

    /// @brief Reset the operator state to it's initial values.
    inline void reset() {
        eg_timer = 0;
        eg_cnt = 0;
        lfo_timer = 0;
        lfo_cnt = 0;
        lfo_AM_step = 126;
        lfo_PM_step = 0;
        set_lfo(0);
    }

    /// @brief Set the sample rate based on the source clock rate.
    ///
    /// @param sample_rate the number of samples per second
    /// @param clock_rate the number of source clock cycles per second
    ///
    void set_sample_rate(float sample_rate, float clock_rate) {
        if (sample_rate == 0) throw Exception("sample_rate must be above 0");
        if (clock_rate == 0) throw Exception("clock_rate must be above 0");

        // frequency base
        freqbase = clock_rate / sample_rate;
        // TODO: why is it necessary to scale these increments by a factor of 1/16
        //       to get the correct timings from the EG and LFO?
        // EG timer increment (updates every 3 samples)
        eg_timer_add = (1 << EG_SH) * freqbase / 16;
        eg_timer_overflow = 3 * (1 << EG_SH) / 16;
        // LFO timer increment (updates every 16 samples)
        lfo_timer_add = (1 << LFO_SH) * freqbase / 16;

        // DeTune table
        for (int d = 0; d <= 3; d++) {
            for (int i = 0; i <= 31; i++) {
                // -10 because chip works with 10.10 fixed point, while we use 16.16
                float rate = ((float) DT_TABLE[d * 32 + i]) * freqbase * (1 << (FREQ_SH - 10));
                dt_table[d][i] = (int32_t) rate;
                dt_table[d + 4][i] = -dt_table[d][i];
            }
        }
        // there are 2048 FNUMs that can be generated using FNUM/BLK registers
        // but LFO works with one more bit of a precision so we really need 4096
        // elements. calculate fnumber -> increment counter table
        for (int i = 0; i < 4096; i++) {
            // freq table for octave 7
            // phase increment counter = 20bit
            // the correct formula is
            //     F-Number = (144 * fnote * 2^20 / M) / 2^(B-1)
            // where sample clock is: M / 144
            // this means the increment value for one clock sample is
            //     FNUM * 2^(B-1) = FNUM * 64
            // for octave 7
            // we also need to handle the ratio between the chip frequency and
            // the emulated frequency (can be 1.0)
            // NOTE:
            // -10 because chip works with 10.10 fixed point, while we use 16.16
            fnum_table[i] = (uint32_t)((float) i * 32 * freqbase * (1 << (FREQ_SH - 10)));
        }
        // maximal frequency is required for Phase overflow calculation, register
        // size is 17 bits (Nemesis)
        fnum_max = (uint32_t)((float) 0x20000 * freqbase * (1 << (FREQ_SH - 10)));
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
    inline void set_lfo(uint8_t value) {
        lfo_timer_overflow = LFO_SAMPLES_PER_STEP[value & 7] << LFO_SH;
    }

    /// @brief Advance LFO to next sample.
    inline void advance_lfo() {
        if (lfo_timer_overflow) {  // LFO enabled
            // increment LFO timer
            lfo_timer += lfo_timer_add;
            // when LFO is enabled, one level will last for
            // 108, 77, 71, 67, 62, 44, 8 or 5 samples
            while (lfo_timer >= lfo_timer_overflow) {
                lfo_timer -= lfo_timer_overflow;
                // There are 128 LFO steps
                lfo_cnt = (lfo_cnt + 1) & 127;
                // triangle (inverted)
                // AM: from 126 to 0 step -2, 0 to 126 step +2
                if (lfo_cnt<64)
                    lfo_AM_step = (lfo_cnt ^ 63) << 1;
                else
                    lfo_AM_step = (lfo_cnt & 63) << 1;
                // PM works with 4 times slower clock
                lfo_PM_step = lfo_cnt >> 2;
            }
        }
    }
};

/// @brief A single FM operator
struct Operator {
 public:
    /// the maximal value that an operator can output (signed 14-bit)
    static constexpr int32_t OUTPUT_MAX = 8191;
    /// the minimal value that an operator can output (signed 14-bit)
    static constexpr int32_t OUTPUT_MIN = -8192;

 private:
    /// attack rate
    uint32_t ar = 0;
    /// total level: TL << 3
    uint32_t tl = 0;
    /// decay rate
    uint32_t d1r = 0;
    /// sustain level:SL_TABLE[SL]
    uint32_t sl = 0;
    /// sustain rate
    uint32_t d2r = 0;
    /// release rate
    uint32_t rr = 0;

    /// detune :dt_table[DT]
    const int32_t* DT = 0;
    /// multiple :ML_TABLE[ML]
    uint32_t mul = 0;

    /// phase counter
    uint32_t phase = 0;
    /// phase step
    int32_t phase_increment = -1;

    /// envelope counter
    int32_t volume = 0;
    /// current output from EG circuit (without AM from LFO)
    uint32_t vol_out = 0;

    /// key scale rate :3-KSR
    uint8_t KSR = 0;
    /// key scale rate :kcode>>(3-KSR)
    uint8_t ksr = 0;

    /// fnum, blk : adjusted to sample rate
    uint32_t fc = 0;
    /// current blk / fnum value for this slot
    uint32_t block_fnum = 0;
    /// key code :
    uint8_t kcode = FREQUENCY_KEYCODE_TABLE[0];

    /// The stages of the envelope generator.
    enum EnvelopeStage {
        /// the silent/off stage, i.e., 0 output
        SILENT = 0,
        /// the release stage, i.e., falling to 0 after note-off from any stage
        RELEASE = 1,
        /// the sustain stage, i.e., holding until note-off after the decay stage
        /// ends
        SUSTAIN = 2,
        /// the decay stage, i.e., falling to sustain level after the attack stage
        /// reaches the total level
        DECAY = 3,
        /// the attack stage, i.e., rising from 0 to the total level
        ATTACK = 4
    } env_stage = SILENT;

    /// attack stage
    uint8_t eg_sh_ar = 0;
    /// attack stage
    uint8_t eg_sel_ar = 0;
    /// decay stage
    uint8_t eg_sh_d1r = 0;
    /// decay stage
    uint8_t eg_sel_d1r = 0;
    /// sustain stage
    uint8_t eg_sh_d2r = 0;
    /// sustain stage
    uint8_t eg_sel_d2r = 0;
    /// release stage
    uint8_t eg_sh_rr = 0;
    /// release stage
    uint8_t eg_sel_rr = 0;

    /// SSG-EG waveform
    uint8_t ssg = 0;
    /// whether SSG-EG is enabled
    bool ssg_enabled = false;
    /// SSG-EG negated output
    uint8_t ssgn = 0;

 public:
    /// whether the gate for the envelope generator is open
    bool is_gate_open = false;
    /// whether amplitude modulation is enabled for the operator
    bool is_amplitude_mod_on = false;

    /// @brief Reset the operator to its initial / default value.
    ///
    /// @param state the global operator state to use (for detune table values)
    /// @details
    /// `state` should be `reset()` before calls to this function.
    ///
    inline void reset(const OperatorContext& state) {
        env_stage = SILENT;
        volume = MAX_ATT_INDEX;
        vol_out = MAX_ATT_INDEX;
        DT = state.dt_table[0];
        mul = 1;
        fc = 0;
        kcode = FREQUENCY_KEYCODE_TABLE[0];
        block_fnum = 0;
        is_gate_open = false;
        is_amplitude_mod_on = false;
        set_rs(0);
        set_ar(0);
        set_tl(0);
        set_dr(0);
        set_sl(0);
        set_sr(0);
        set_rr(0);
        set_ssg_enabled(false);
        set_ssg_mode(0);
    }

    // -----------------------------------------------------------------------
    // MARK: Parameter Setters
    // -----------------------------------------------------------------------

    /// @brief Set the key-on flag for the given operator.
    ///
    /// @param is_gate_open true if the gate is open, false otherwise
    /// @param prevent_clicks true to prevent clicks from the operator
    /// @details
    /// Preventing clicks is not authentic functionality, but may be preferred.
    ///
    inline void set_gate(bool is_gate_open, bool prevent_clicks = false) {
        if (this->is_gate_open == is_gate_open) return;
        this->is_gate_open = is_gate_open;
        if (is_gate_open) {  // reset the phase and set envelope to attack
            // reset the phase if preventing clicks has not been enabled
            if (!prevent_clicks) phase = 0;
            ssgn = (ssg & SSG_INV) >> 1;
            env_stage = ATTACK;
        } else {  // set the envelope to the release stage
            if (env_stage != SILENT) env_stage = RELEASE;
        }
    }

    /// @brief Set the frequency of the voice.
    ///
    /// @param frequency the frequency value measured in Hz
    /// @returns true if the new frequency differs from the old frequency
    ///
    inline bool set_frequency(OperatorContext& state, float frequency) {
        // Shift the frequency to the base octave and calculate the octave to
        // play. The base octave is defined as a 10-bit number in [0, 1023].
        int octave = 2;
        for (; frequency >= 1024; octave++) frequency /= 2;
        // TODO: why is this arbitrary shift necessary to tune to C4?
        // NOTE: shift calculated by producing C4 note from a ground truth
        //       oscillator and comparing the output from YM2612 via division:
        //       1.458166333006277
        frequency = frequency / 1.458;
        // cast the shifted frequency to a 16-bit container
        const uint16_t freq16bit = frequency;

        // key-scale code
        kcode = (octave << 2) | FREQUENCY_KEYCODE_TABLE[(freq16bit >> 7) & 0xf];
        // phase increment counter
        uint32_t old_fc = fc;
        fc = state.fnum_table[freq16bit * 2] >> (7 - octave);
        // store fnum in clear form for LFO PM calculations
        block_fnum = (octave << 11) | freq16bit;
        // update the phase increment if the frequency changed
        return old_fc != fc;
    }

    /// @brief Set the 5-bit attack rate.
    ///
    /// @param value the value for the attack rate (AR)
    ///
    inline void set_ar(uint8_t value) {
        ar = (value & 0x1f) ? 32 + ((value & 0x1f) << 1) : 0;
        // refresh Attack rate
        if (ar + ksr < 32 + 62) {
            eg_sh_ar = ENV_RATE_SHIFT[ar + ksr];
            eg_sel_ar = ENV_RATE_SELECT[ar + ksr];
        } else {
            eg_sh_ar = 0;
            eg_sel_ar = 17 * ENV_RATE_STEPS;
        }
    }

    /// @brief Set the 7-bit total level.
    ///
    /// @param value the value for the total level (TL)
    ///
    inline void set_tl(uint8_t value) { tl = (value & 0x7f) << (ENV_BITS - 7); }

    /// @brief Set the decay 1 rate, i.e., decay rate.
    ///
    /// @param value the value for the decay 1 rate (D1R)
    ///
    inline void set_dr(uint8_t value) {
        d1r = (value & 0x1f) ? 32 + ((value & 0x1f) << 1) : 0;
        eg_sh_d1r = ENV_RATE_SHIFT[d1r + ksr];
        eg_sel_d1r = ENV_RATE_SELECT[d1r + ksr];
    }

    /// @brief Set the sustain level rate.
    ///
    /// @param value the value to index from the sustain level table
    ///
    inline void set_sl(uint8_t value) { sl = SL_TABLE[value]; }

    /// @brief Set the decay 2 rate, i.e., sustain rate.
    ///
    /// @param value the value for the decay 2 rate (D2R)
    ///
    inline void set_sr(uint8_t value) {
        d2r = (value & 0x1f) ? 32 + ((value & 0x1f) << 1) : 0;
        eg_sh_d2r = ENV_RATE_SHIFT[d2r + ksr];
        eg_sel_d2r = ENV_RATE_SELECT[d2r + ksr];
    }

    /// @brief Set the release rate.
    ///
    /// @param value the value for the release rate (RR)
    ///
    inline void set_rr(uint8_t value) {
        rr = 34 + (value << 2);
        eg_sh_rr = ENV_RATE_SHIFT[rr + ksr];
        eg_sel_rr = ENV_RATE_SELECT[rr + ksr];
    }

    /// @brief Set the 2-bit rate scale.
    ///
    /// @param value the value for the rate scale (RS)
    /// @returns true if the phase increments need to be recalculated, i.e.,
    /// true if the new value differs from the old value
    ///
    inline bool set_rs(uint8_t value) {
        uint8_t old_KSR = KSR;
        KSR = 3 - (value & 3);
        // refresh Attack rate
        if (ar + ksr < 32 + 62) {
            eg_sh_ar = ENV_RATE_SHIFT[ar + ksr];
            eg_sel_ar = ENV_RATE_SELECT[ar + ksr];
        } else {
            eg_sh_ar = 0;
            eg_sel_ar = 17 * ENV_RATE_STEPS;
        }
        return KSR != old_KSR;
    }


    /// @brief set whether the SSG mode is enabled or not
    ///
    /// @param enabled true to enable SSG mode, false to disable it
    ///
    inline void set_ssg_enabled(bool enabled = false) {
        if (ssg_enabled == enabled) return;
        ssg_enabled = enabled;
        // recalculate EG output
        if (ssg_enabled && (ssgn ^ (ssg & SSG_INV)) && (env_stage > RELEASE))
            vol_out = ((uint32_t) (0x200 - volume) & MAX_ATT_INDEX) + tl;
        else
            vol_out = (uint32_t) volume + tl;
    }

    /// Bit-mask for the various SSG modes
    enum SSG_Modes {
        SSG_HOLD = 0x01,
        SSG_ALT =   0x02,
        SSG_INV =   0x04
    };

    // TODO: fix why some modes aren't working right.
    /// @brief set the SSG register to a new value.
    ///
    /// @param value the value for the looping envelope generator register (SSG)
    /// @details
    ///
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
    inline void set_ssg_mode(uint8_t value = 0) {
        value = value & 7;
        if (ssg == value) return;
        ssg = value;
        // recalculate EG output
        if (ssg_enabled && (ssgn ^ (ssg & SSG_INV)) && (env_stage > RELEASE))
            vol_out = ((uint32_t) (0x200 - volume) & MAX_ATT_INDEX) + tl;
        else
            vol_out = (uint32_t) volume + tl;
    }

    /// @brief set the rate multiplier to a new value.
    ///
    /// @param value the value for the rate multiplier \f$\in [0, 15]\f$
    /// @returns true if the phase increments need to be recalculated, i.e.,
    /// true if the new value differs from the old value
    ///
    inline bool set_multiplier(uint8_t value = 1) {
        uint8_t old_multiplier = mul;
        // calculate the new MUL register value
        mul = (value & 0x0f) ? (value & 0x0f) * 2 : 1;
        return mul != old_multiplier;
    }

    /// @brief set the rate detune register to a new value.
    ///
    /// @param state the global emulation context the operator is running in
    /// @param value the value for the detune register \f$\in [0, 7]\f$
    /// @returns true if the phase increments need to be recalculated, i.e.,
    /// true if the new value differs from the old value
    ///
    inline bool set_detune(const OperatorContext& state, uint8_t value = 4) {
        const int32_t* old_DETUNE = DT;
        DT = state.dt_table[value & 7];
        return DT != old_DETUNE;
    }

    // -----------------------------------------------------------------------
    // MARK: Voice Interface
    // -----------------------------------------------------------------------

    /// @brief SSG-EG update process.
    ///
    /// @details
    /// The behavior is based upon Nemesis tests on real hardware. This is
    /// actually executed before each sample.
    ///
    inline void update_ssg_envelope_generator() {
        // detect SSG-EG transition. this is not required during release phase
        // as the attenuation has been forced to MAX and output invert flag is
        // not used. If an Attack Phase is programmed, inversion can occur on
        // each sample.
        if (ssg_enabled && (volume >= 0x200) && (env_stage > RELEASE)) {
            if (ssg & SSG_HOLD) {  // bit 0 = hold SSG-EG
                // set inversion flag
                if (ssg & SSG_ALT) ssgn = SSG_INV;
                // force attenuation level during decay phases
                if ((env_stage != ATTACK) && !(ssgn ^ (ssg & SSG_INV)))
                    volume = MAX_ATT_INDEX;
            } else {  // loop SSG-EG
                // toggle output inversion flag or reset Phase Generator
                if (ssg & SSG_ALT)
                    ssgn ^= SSG_INV;
                else
                    phase = 0;
                // same as Key ON
                if (env_stage != ATTACK) {
                    if ((ar + ksr) < 32 + 62) {  // attacking
                        env_stage = (volume <= MIN_ATT_INDEX) ?
                            ((sl == MIN_ATT_INDEX) ? SUSTAIN : DECAY) : ATTACK;
                    } else {  // Attack Rate @ max -> jump to next stage
                        volume = MIN_ATT_INDEX;
                        env_stage = (sl == MIN_ATT_INDEX) ? SUSTAIN : DECAY;
                    }
                }
            }
            // recalculate EG output
            if (ssgn ^ (ssg & SSG_INV))
                vol_out = ((uint32_t) (0x200 - volume) & MAX_ATT_INDEX) + tl;
            else
                vol_out = (uint32_t) volume + tl;
        }
    }

    /// Update the envelope generator for the operator.
    ///
    /// @param eg_cnt the counter for the envelope generator
    ///
    inline void update_envelope_generator(uint32_t eg_cnt) {
        unsigned int swap_flag = 0;
        switch (env_stage) {
        case SILENT:  // not running
            break;
        case ATTACK:  // attack stage
            if (!(eg_cnt & ((1 << eg_sh_ar) - 1))) {
                volume += (~volume * (ENV_INCREMENT_TABLE[eg_sel_ar + ((eg_cnt >> eg_sh_ar) & 7)])) >> 4;
                if (volume <= MIN_ATT_INDEX) {
                    volume = MIN_ATT_INDEX;
                    env_stage = DECAY;
                }
            }
            break;
        case DECAY:  // decay stage
            if (!(eg_cnt & ((1 << eg_sh_d1r) - 1))) {
                volume += (ssg_enabled ? 4 : 1) * ENV_INCREMENT_TABLE[eg_sel_d1r + ((eg_cnt >> eg_sh_d1r) & 7)];
                if (volume >= static_cast<int32_t>(sl))
                    env_stage = SUSTAIN;
            }
            break;
        case SUSTAIN:  // sustain stage
            if (ssg_enabled) {  // SSG EG type envelope selected
                if (!(eg_cnt & ((1 << eg_sh_d2r) - 1))) {
                    volume += 4 * ENV_INCREMENT_TABLE[eg_sel_d2r + ((eg_cnt >> eg_sh_d2r) & 7)];
                    if (volume >= ENV_QUIET) {
                        volume = MAX_ATT_INDEX;
                        if (ssg & SSG_HOLD) {  // bit 0 = hold
                            if (ssgn & SSG_HOLD) {  // have we swapped once ???
                                // yes, so do nothing, just hold current level
                            } else {  // bit 1 = alternate
                                swap_flag = (ssg & SSG_ALT) | 1;
                            }
                        } else {  // same as KEY-ON operation
                            // restart of the Phase Generator should be here
                            phase = 0;
                            // stage -> Attack
                            volume = 511;
                            env_stage = ATTACK;
                            // bit 1 = alternate
                            swap_flag = (ssg & SSG_ALT);
                        }
                    }
                }
            } else {
                if (!(eg_cnt & ((1 << eg_sh_d2r) - 1))) {
                    volume += ENV_INCREMENT_TABLE[eg_sel_d2r + ((eg_cnt >> eg_sh_d2r) & 7)];
                    if (volume >= MAX_ATT_INDEX) {
                        volume = MAX_ATT_INDEX;
                        // do not change env_stage (verified on real chip)
                    }
                }
            }
            break;
        case RELEASE:  // release stage
            if (!(eg_cnt & ((1 << eg_sh_rr) - 1))) {
                // SSG-EG affects Release stage also (Nemesis)
                volume += ENV_INCREMENT_TABLE[eg_sel_rr + ((eg_cnt >> eg_sh_rr) & 7)];
                if (volume >= MAX_ATT_INDEX) {
                    volume = MAX_ATT_INDEX;
                    env_stage = SILENT;
                }
            }
            break;
        }
        // get the output volume from the slot
        unsigned int out = static_cast<uint32_t>(volume);
        // negate output (changes come from alternate bit, init comes from
        // attack bit)
        if (ssg_enabled && (ssgn & SSG_ALT) && (env_stage > RELEASE))
            out ^= MAX_ATT_INDEX;
        // we need to store the result here because we are going to change
        // ssgn in next instruction
        vol_out = out + tl;
        // reverse oprtr inversion flag
        ssgn ^= swap_flag;
    }

    /// @brief Update phase increment and envelope generator
    inline void refresh_phase_and_envelope(uint32_t fnum_max) {
        fc += DT[kcode];
        // (frequency) phase increment counter
        phase_increment = (fc * mul) >> 1;
        if (ksr != kcode >> KSR) {
            ksr = kcode >> KSR;
            // calculate envelope generator rates
            if ((ar + ksr) < 32 + 62) {
                eg_sh_ar = ENV_RATE_SHIFT[ar + ksr];
                eg_sel_ar = ENV_RATE_SELECT[ar + ksr];
            } else {
                eg_sh_ar = 0;
                eg_sel_ar = 17 * ENV_RATE_STEPS;
            }
            // set the shift
            eg_sh_d1r = ENV_RATE_SHIFT[d1r + ksr];
            eg_sh_d2r = ENV_RATE_SHIFT[d2r + ksr];
            eg_sh_rr = ENV_RATE_SHIFT[rr + ksr];
            // set the selector
            eg_sel_d1r = ENV_RATE_SELECT[d1r + ksr];
            eg_sel_d2r = ENV_RATE_SELECT[d2r + ksr];
            eg_sel_rr = ENV_RATE_SELECT[rr + ksr];
        }
    }

    /// @brief Get the envelope volume based on amplitude modulation level.
    ///
    /// @param amplitude_modulation the amount of amplitude modulation to apply
    /// @details
    /// `amplitude_modulation` is only applied to the envelope if
    /// `this->is_amplitude_mod_on` is set to `true`.
    ///
    inline uint32_t get_envelope(uint32_t amplitude_modulation) const {
        return vol_out + is_amplitude_mod_on * amplitude_modulation;
    }

    /// @brief Return the value of operator (1) given envelope and PM.
    ///
    /// @param env the value of the operator's envelope (after AM is applied)
    /// @param pm the amount of phase modulation for the operator
    /// @details
    /// the `pm` parameter for operators 2, 3, and 4 (BUT NOT 1) should be
    /// shifted left by 15 bits before being passed in. Operator 1 should be
    /// shift left by the setting of its `FB` (feedback) parameter.
    ///
    inline int32_t calculate_output(uint32_t env, int32_t pm) const {
        const uint32_t p = (env << 3) + SIN_TABLE[
            (((int32_t)((phase & ~FREQ_MASK) + pm)) >> FREQ_SH) & SIN_MASK
        ];
        if (p >= TL_TABLE_LENGTH) return 0;
        return TL_TABLE[p];
    }

    /// @brief Update the phase of the operator.
    ///
    /// @param state the context the operator is running in
    /// @param pms the amount of pitch modulation to apply
    ///
    inline void update_phase_counters(const OperatorContext& state, uint8_t pms) {
        const uint32_t fnum_lfo = ((block_fnum & 0x7f0) >> 4) * 32 * 8;
        const int32_t lfo_fnum_offset = LFO_PM_TABLE[fnum_lfo + pms + state.lfo_PM_step];
        if (pms && lfo_fnum_offset) {  // update the phase using the LFO
            uint32_t fnum = 2 * block_fnum + lfo_fnum_offset;
            const uint8_t blk = (fnum & 0x7000) >> 12;
            fnum = fnum & 0xfff;
            const int phase_increment_counter = state.fnum_table[fnum] >> (7 - blk);
            const int keyscale_code = (blk << 2) | FREQUENCY_KEYCODE_TABLE[fnum >> 8];
            // detects frequency overflow (credits to Nemesis)
            int finc = phase_increment_counter + DT[keyscale_code];
            if (finc < 0) finc += state.fnum_max;
            phase += (finc * mul) >> 1;
        } else {  // no LFO phase modulation
            phase += phase_increment;
        }
    }
};

}; // namespace YamahaYM2612

#endif  // DSP_YAMAHA_YM2612_OPERATOR_HPP_
