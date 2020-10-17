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
 private:
    /// registers
    uint8_t registers[512];
    /// OPN engine state
    EngineState engine;
    /// channel state
    Voice voices[6];
    /// address line A1
    uint8_t addr_A1;

    /// the value of the global LFO parameter
    uint8_t LFO = 0;
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
            /// the SSG mode for the operator
            uint8_t SSG = 0;
        } operators[4];
    } channels[6];

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
        engine.type = TYPE_YM2612;
        engine.state.clock = clock_rate;
        engine.state.rate = sample_rate;
        reset();
    }

    /// @brief Set the sample rate the a new value.
    ///
    /// @param clock_rate the underlying clock rate of the system
    /// @param sample_rate the rate to draw samples from the emulator at
    ///
    void setSampleRate(double clock_rate, double sample_rate) {
        engine.state.clock = clock_rate;
        engine.state.rate = sample_rate;
        set_prescaler(&engine);
    }

    /// @brief Reset the emulator to its initial state
    void reset() {
        // clear instance variables
        memset(registers, 0, sizeof registers);
        LFO = stereo_output[0] = stereo_output[1] = 0;
        // set the frequency scaling parameters of the engine emulator
        set_prescaler(&engine);
        // mode 0 , timer reset
        write_mode(&engine, 0x27, 0x30);
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

        write_mode(&engine, 0x27, 0x30);
        write_mode(&engine, 0x26, 0x00);
        write_mode(&engine, 0x25, 0x00);
        write_mode(&engine, 0x24, 0x00);

        reset_voices(&(engine.state), &voices[0], 6);

        for (int i = 0xb6; i >= 0xb4; i--) {
            write_register(&engine, i, 0xc0);
            write_register(&engine, i | 0x100, 0xc0);
        }

        for (int i = 0xb2; i >= 0x30; i--) {
            write_register(&engine, i, 0);
            write_register(&engine, i | 0x100, 0);
        }

        for (int c = 0; c < 6; c++) setST(c, 3);
    }

    /// @brief Run a step on the emulator
    void step() {
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
        update_ssg_eg_channel(&(voices[0].operators[Op1]));
        update_ssg_eg_channel(&(voices[1].operators[Op1]));
        update_ssg_eg_channel(&(voices[2].operators[Op1]));
        update_ssg_eg_channel(&(voices[3].operators[Op1]));
        update_ssg_eg_channel(&(voices[4].operators[Op1]));
        update_ssg_eg_channel(&(voices[5].operators[Op1]));
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
            advance_eg_channel(&engine, &(voices[0].operators[Op1]));
            advance_eg_channel(&engine, &(voices[1].operators[Op1]));
            advance_eg_channel(&engine, &(voices[2].operators[Op1]));
            advance_eg_channel(&engine, &(voices[3].operators[Op1]));
            advance_eg_channel(&engine, &(voices[4].operators[Op1]));
            advance_eg_channel(&engine, &(voices[5].operators[Op1]));
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
        // 6-channels mixing
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

    /// @brief Write data to a register on the chip.
    ///
    /// @param address the address of the register to write data to
    /// @param data the value of the data to write to the register
    ///
    void write(uint8_t address, uint8_t data) {
        switch (address & 3) {
        case 0:  // address port 0
            engine.state.address = data;
            addr_A1 = 0;
            break;
        case 1:  // data port 0
            // verified on real YM2608
            if (addr_A1 != 0) break;
            // get the address from the latch and write the data
            address = engine.state.address;
            registers[address] = data;
            if ((address & 0xf0) == 0x20)  // 0x20-0x2f Mode
                write_mode(&engine, address, data);
            else
                write_register(&engine, address, data);
            break;
        case 2:  // address port 1
            engine.state.address = data;
            addr_A1 = 1;
            break;
        case 3:  // data port 1
            // verified on real YM2608
            if (addr_A1 != 1) break;
            // get the address from the latch and right to the given register
            address = engine.state.address;
            registers[address | 0x100] = data;
            write_register(&engine, address | 0x100, data);
            break;
        }
    }

    /// @brief Set part of a 16-bit register to a given 8-bit value.
    ///
    /// @param part the part of the register space to access,
    ///        0=latch1, 1=data1, 2=latch2, 3=data2
    /// @param reg the address of the register to write data to
    /// @param data the value of the data to write to the register
    ///
    /// @details
    ///
    /// ## [Memory map](http://www.smspower.org/maxim/Documents/YM2612#reg27)
    ///
    /// | REG  | Bit 7           | Bit 6 | Bit 5            | Bit 4   | Bit 3      | Bit 2          | Bit 1        | Bit 0  |
    /// |:-----|:----------------|:------|:-----------------|:--------|:-----------|:---------------|:-------------|:-------|
    /// | 22H  |                 |       |                  |         | LFO enable | LFO frequency  |              |        |
    /// | 24H  | Timer A MSBs    |       |                  |         |            |                |              |        |
    /// | 25H  |                 |       |                  |         |            |                | Timer A LSBs |        |
    /// | 26H  | Timer B         |       |                  |         |            |                |              |        |
    /// | 27H  | Ch3 mode        |       | Reset B          | Reset A | Enable B   | Enable A       | Load B       | Load A |
    /// | 28H  | Operator        |       |                  |         |            | Channel        |              |        |
    /// | 29H  |                 |       |                  |         |            |                |              |        |
    /// | 2AH  | DAC             |       |                  |         |            |                |              |        |
    /// | 2BH  | DAC en          |       |                  |         |            |                |              |        |
    /// |      |                 |       |                  |         |            |                |              |        |
    /// | 30H+ |                 | DT1   |                  |         | MUL        |                |              |        |
    /// | 40H+ |                 | TL    |                  |         |            |                |              |        |
    /// | 50H+ | RS              |       |                  | AR      |            |                |              |        |
    /// | 60H+ | AM              |       |                  | D1R     |            |                |              |        |
    /// | 70H+ |                 |       |                  | D2R     |            |                |              |        |
    /// | 80H+ | D1L             |       |                  |         | RR         |                |              |        |
    /// | 90H+ |                 |       |                  |         | SSG-EG     |                |              |        |
    /// |      |                 |       |                  |         |            |                |              |        |
    /// | A0H+ | Freq. LSB       |       |                  |         |            |                |              |        |
    /// | A4H+ |                 |       | Block            |         |            | Freq. MSB      |              |        |
    /// | A8H+ | Ch3 suppl. freq.|       |                  |         |            |                |              |        |
    /// | ACH+ |                 |       | Ch3 suppl. block |         |            | Ch3 suppl freq |              |        |
    /// | B0H+ |                 |       | Feedback         |         |            | Algorithm      |              |        |
    /// | B4H+ | L               | R     | AMS              |         |            | FMS            |              |        |
    ///
    inline void setREG(uint8_t part, uint8_t reg, uint8_t data) {
        write(part << 1, reg);
        write((part << 1) + 1, data);
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
        // don't set the value if it hasn't changed
        if (LFO == value) return;
        // update the local LFO value
        LFO = value;
        // set the LFO on the engine emulator
        setREG(0, 0x22, ((value > 0) << 3) | (value & 7));
    }

    // -----------------------------------------------------------------------
    // MARK: Global control for each channel
    // -----------------------------------------------------------------------

    /// @brief Set the frequency for the given channel.
    ///
    /// @param channel the voice on the chip to set the frequency for
    /// @param frequency the frequency value measured in Hz
    ///
    inline void setFREQ(uint8_t channel, float frequency) {
        // shift the frequency to the base octave and calculate the octave to play.
        // the base octave is defined as a 10-bit number, i.e., in [0, 1023]
        int octave = 2;
        for (; frequency >= 1024; octave++) frequency /= 2;
        // NOTE: arbitrary shift calculated by producing note from a ground truth
        //       oscillator and comparing the output from YM2612 via division.
        //       1.458166333006277
        // TODO: why is this arbitrary shift necessary to tune to C4?
        frequency = frequency / 1.458;
        // cast the shifted frequency to a 16-bit container
        const uint16_t freq16bit = frequency;
        // write the low and high portions of the frequency to the register
        const auto freqHigh = ((freq16bit >> 8) & 0x07) | ((octave & 0x07) << 3);
        setREG(getVoicePart(channel), getVoiceOffset(0xA4, channel), freqHigh);
        const auto freqLow = freq16bit & 0xff;
        setREG(getVoicePart(channel), getVoiceOffset(0xA0, channel), freqLow);
    }

    /// @brief Set the gate for the given channel.
    ///
    /// @param channel the voice on the chip to set the gate for
    /// @param value the boolean value of the gate signal
    ///
    inline void setGATE(uint8_t channel, uint8_t value) {
        // set the gate register based on the value. False = x00 and True = 0xF0
        setREG(0, 0x28, (static_cast<bool>(value) * 0xF0) + ((channel / 3) * 4 + channel % 3));
    }

    /// @brief Set the algorithm (AL) register for the given channel.
    ///
    /// @param channel the channel to set the algorithm register of
    /// @param value the selected FM algorithm in [0, 7]
    ///
    inline void setAL(uint8_t channel, uint8_t value) {
        if (channels[channel].AL == value) return;
        channels[channel].AL = value;
        voices[channel].FB_ALG = (voices[channel].FB_ALG & 0x38) | (value & 7);
        setREG(getVoicePart(channel), getVoiceOffset(0xB0, channel), voices[channel].FB_ALG);
    }

    /// @brief Set the feedback (FB) register for the given channel.
    ///
    /// @param channel the channel to set the feedback register of
    /// @param value the amount of feedback for operator 1
    ///
    inline void setFB(uint8_t channel, uint8_t value) {
        if (channels[channel].FB == value) return;
        channels[channel].FB = value;
        voices[channel].FB_ALG = (voices[channel].FB_ALG & 7)| ((value & 7) << 3);
        setREG(getVoicePart(channel), getVoiceOffset(0xB0, channel), voices[channel].FB_ALG);
    }

    /// @brief Set the state (ST) register for the given channel.
    ///
    /// @param channel the channel to set the state register of
    /// @param value the value of the state register
    ///
    inline void setST(uint8_t channel, uint8_t value) {
        voices[channel].LR_AMS_FMS = (voices[channel].LR_AMS_FMS & 0x3F)| ((value & 3) << 6);
        setREG(getVoicePart(channel), getVoiceOffset(0xB4, channel), voices[channel].LR_AMS_FMS);
    }

    /// @brief Set the AM sensitivity (AMS) register for the given channel.
    ///
    /// @param channel the channel to set the AM sensitivity register of
    /// @param value the amount of amplitude modulation (AM) sensitivity
    ///
    inline void setAMS(uint8_t channel, uint8_t value) {
        if (channels[channel].AMS == value) return;
        channels[channel].AMS = value;
        voices[channel].LR_AMS_FMS = (voices[channel].LR_AMS_FMS & 0xCF)| ((value & 3) << 4);
        setREG(getVoicePart(channel), getVoiceOffset(0xB4, channel), voices[channel].LR_AMS_FMS);
    }

    /// @brief Set the FM sensitivity (FMS) register for the given channel.
    ///
    /// @param channel the channel to set the FM sensitivity register of
    /// @param value the amount of frequency modulation (FM) sensitivity
    ///
    inline void setFMS(uint8_t channel, uint8_t value) {
        if (channels[channel].FMS == value) return;
        channels[channel].FMS = value;
        voices[channel].LR_AMS_FMS = (voices[channel].LR_AMS_FMS & 0xF8)| (value & 7);
        setREG(getVoicePart(channel), getVoiceOffset(0xB4, channel), voices[channel].LR_AMS_FMS);
    }

    // -----------------------------------------------------------------------
    // MARK: Operator control for each channel
    // -----------------------------------------------------------------------

    /// @brief Set the SSG-envelope register for the given channel and operator.
    ///
    /// @param channel the channel to set the SSG-EG register of (in [0, 6])
    /// @param slot the operator to set the SSG-EG register of (in [0, 3])
    /// @param is_on whether the looping envelope generator should be turned on
    /// @param mode the mode for the looping generator to run in (in [0, 7])
    /// @details
    /// The mode can be any of the following:
    ///
    /// Table: SSG-EG LFO Patterns
    /// +-------+-------------+
    /// | AtAlH | LFO Pattern |
    /// +=======+=============+
    /// | 0 0 0 |  \\\\       |
    /// +-------+-------------+
    /// | 0 0 1 |  \___       |
    /// +-------+-------------+
    /// | 0 1 0 |  \/\/       |
    /// +-------+-------------+
    /// |       |   ___       |
    /// | 0 1 1 |  \          |
    /// +-------+-------------+
    /// | 1 0 0 |  ////       |
    /// +-------+-------------+
    /// |       |   ___       |
    /// | 1 0 1 |  /          |
    /// +-------+-------------+
    /// | 1 1 0 |  /\/\       |
    /// +-------+-------------+
    /// | 1 1 1 |  /___       |
    /// +-------+-------------+
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
    inline void setSSG(uint8_t channel, uint8_t slot, bool is_on, uint8_t mode) {
        const uint8_t value = (is_on << 3) | (mode & 7);
        if (channels[channel].operators[slot].SSG == value) return;
        channels[channel].operators[slot].SSG = value;
        // TODO: slot here needs mapped to the order 1 3 2 4
        setREG(getVoicePart(channel), getVoiceOffset(0x90 + (slot << 2), channel), value);
    }

    /// @brief Set the attack rate (AR) register for the given channel and operator.
    ///
    /// @param channel the channel to set the attack rate (AR) register of (in [0, 6])
    /// @param slot the operator to set the attack rate (AR) register of (in [0, 3])
    /// @param value the rate of the attack stage of the envelope generator
    ///
    inline void setAR(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].AR == value) return;
        channels[channel].operators[slot].AR = value;
        Operator *s = &voices[channel].operators[slots_idx[slot]];
        s->ar_ksr = (s->ar_ksr & 0xC0) | (value & 0x1f);
        set_ar_ksr(&voices[channel], s, s->ar_ksr);
    }

    /// @brief Set the 1st decay rate (D1) register for the given channel and operator.
    ///
    /// @param channel the channel to set the 1st decay rate (D1) register of (in [0, 6])
    /// @param slot the operator to set the 1st decay rate (D1) register of (in [0, 3])
    /// @param value the rate of decay for the 1st decay stage of the envelope generator
    ///
    inline void setD1(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].D1 == value) return;
        channels[channel].operators[slot].D1 = value;
        Operator *s = &voices[channel].operators[slots_idx[slot]];
        s->dr = (s->dr & 0x80) | (value & 0x1F);
        set_dr(s, s->dr);
    }

    /// @brief Set the sustain level (SL) register for the given channel and operator.
    ///
    /// @param channel the channel to set the sustain level (SL) register of (in [0, 6])
    /// @param slot the operator to set the sustain level (SL) register of (in [0, 3])
    /// @param value the amplitude level at which the 2nd decay stage of the envelope generator begins
    ///
    inline void setSL(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].SL == value) return;
        channels[channel].operators[slot].SL = value;
        Operator *s =  &voices[channel].operators[slots_idx[slot]];
        s->sl_rr = (s->sl_rr & 0x0f) | ((value & 0x0f) << 4);
        set_sl_rr(s, s->sl_rr);
    }

    /// @brief Set the 2nd decay rate (D2) register for the given channel and operator.
    ///
    /// @param channel the channel to set the 2nd decay rate (D2) register of (in [0, 6])
    /// @param slot the operator to set the 2nd decay rate (D2) register of (in [0, 3])
    /// @param value the rate of decay for the 2nd decay stage of the envelope generator
    ///
    inline void setD2(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].D2 == value) return;
        channels[channel].operators[slot].D2 = value;
        set_sr(&voices[channel].operators[slots_idx[slot]], value);
    }

    /// @brief Set the release rate (RR) register for the given channel and operator.
    ///
    /// @param channel the channel to set the release rate (RR) register of (in [0, 6])
    /// @param slot the operator to set the release rate (RR) register of (in [0, 3])
    /// @param value the rate of release of the envelope generator after key-off
    ///
    inline void setRR(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].RR == value) return;
        channels[channel].operators[slot].RR = value;
        Operator *s =  &voices[channel].operators[slots_idx[slot]];
        s->sl_rr = (s->sl_rr & 0xf0) | (value & 0x0f);
        set_sl_rr(s, s->sl_rr);
    }

    /// @brief Set the total level (TL) register for the given channel and operator.
    ///
    /// @param channel the channel to set the total level (TL) register of (in [0, 6])
    /// @param slot the operator to set the total level (TL) register of (in [0, 3])
    /// @param value the total amplitude of envelope generator
    ///
    inline void setTL(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].TL == value) return;
        channels[channel].operators[slot].TL = value;
        set_tl(&voices[channel], &voices[channel].operators[slots_idx[slot]], value);
    }

    /// @brief Set the multiplier (MUL) register for the given channel and operator.
    ///
    /// @param channel the channel to set the multiplier (MUL) register of (in [0, 6])
    /// @param slot the operator to set the multiplier  (MUL)register of (in [0, 3])
    /// @param value the value of the FM phase multiplier
    ///
    inline void setMUL(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].MUL == value) return;
        channels[channel].operators[slot].MUL = value;
        voices[channel].operators[slots_idx[slot]].mul = (value & 0x0f) ? (value & 0x0f) * 2 : 1;
        voices[channel].operators[Op1].phase_increment = -1;
    }

    /// @brief Set the detune (DET) register for the given channel and operator.
    ///
    /// @param channel the channel to set the detune (DET) register of (in [0, 6])
    /// @param slot the operator to set the detune (DET) register of (in [0, 3])
    /// @param value the the level of detuning for the FM operator
    ///
    inline void setDET(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].DET == value) return;
        channels[channel].operators[slot].DET = value;
        voices[channel].operators[slots_idx[slot]].DT  = engine.state.dt_tab[(value)&7];
        voices[channel].operators[Op1].phase_increment = -1;
    }

    /// @brief Set the rate-scale (RS) register for the given channel and operator.
    ///
    /// @param channel the channel to set the rate-scale (RS) register of (in [0, 6])
    /// @param slot the operator to set the rate-scale (RS) register of (in [0, 3])
    /// @param value the amount of rate-scale applied to the FM operator
    ///
    inline void setRS(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].RS == value) return;
        channels[channel].operators[slot].RS = value;
        Operator *s = &voices[channel].operators[slots_idx[slot]];
        s->ar_ksr = (s->ar_ksr & 0x1F) | ((value & 0x03) << 6);
        set_ar_ksr(&voices[channel], s, s->ar_ksr);
    }

    /// @brief Set the amplitude modulation (AM) register for the given channel and operator.
    ///
    /// @param channel the channel to set the amplitude modulation (AM) register of (in [0, 6])
    /// @param slot the operator to set the amplitude modulation (AM) register of (in [0, 3])
    /// @param value the true to enable amplitude modulation from the LFO, false to disable it
    ///
    inline void setAM(uint8_t channel, uint8_t slot, uint8_t value) {
        if (channels[channel].operators[slot].AM == value) return;
        channels[channel].operators[slot].AM = value;
        Operator *s = &voices[channel].operators[slots_idx[slot]];
        s->AMmask = (value) ? ~0 : 0;
    }

    // -----------------------------------------------------------------------
    // MARK: Emulator output
    // -----------------------------------------------------------------------

    /// @brief Return the output from the left channel of the mix output.
    ///
    /// @returns the left channel of the mix output
    ///
    inline int16_t getOutputLeft() { return stereo_output[0]; }

    /// @brief Return the output from the right channel of the mix output.
    ///
    /// @returns the right channel of the mix output
    ///
    inline int16_t getOutputRight() { return stereo_output[1]; }

    /// @brief Return the voltage from the left channel of the mix output.
    ///
    /// @returns the voltage of the left channel of the mix output
    ///
    inline float getVoltageLeft() {
        return static_cast<float>(stereo_output[0]) / std::numeric_limits<int16_t>::max();
    }

    /// @brief Return the voltage from the right channel of the mix output.
    ///
    /// @returns the voltage of the right channel of the mix output
    ///
    inline float getVoltageRight() {
        return static_cast<float>(stereo_output[1]) / std::numeric_limits<int16_t>::max();
    }
};

#endif  // DSP_YAMAHA_YM2612_HPP_
