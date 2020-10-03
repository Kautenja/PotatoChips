// A Sony SPC700 chip (from Nintendo SNES) emulator module.
// Copyright 2020 Christian Kauten
//
// Author: Christian Kauten (kautenja@auburn.edu)
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

#include "plugin.hpp"
#include "componentlibrary.hpp"
#include "dsp/sony_s_dsp_adsr.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Sony S-DSP chip (from Nintendo SNES) emulator module.
struct ChipS_SMP_ADSR : Module {
 private:
    /// the RAM for the S-DSP chip (64KB = 16-bit address space)
    uint8_t ram[Sony_S_DSP_ADSR::SIZE_OF_RAM];
    /// the Sony S-DSP sound chip emulator
    Sony_S_DSP_ADSR apu{ram};

    /// triggers for handling gate inputs for the voices
    rack::dsp::BooleanTrigger gateTriggers[Sony_S_DSP_ADSR::VOICE_COUNT][2];

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ,          Sony_S_DSP_ADSR::VOICE_COUNT),  // TODO: remove
        ENUMS(PARAM_PM_ENABLE,     Sony_S_DSP_ADSR::VOICE_COUNT),  // TODO: remove
        ENUMS(PARAM_NOISE_ENABLE,  Sony_S_DSP_ADSR::VOICE_COUNT),  // TODO: remove
        PARAM_NOISE_FREQ,  // TODO: remove
        ENUMS(PARAM_AMPLITUDE,     Sony_S_DSP_ADSR::VOICE_COUNT),
        ENUMS(PARAM_VOLUME_R,      Sony_S_DSP_ADSR::VOICE_COUNT),
        ENUMS(PARAM_ATTACK,        Sony_S_DSP_ADSR::VOICE_COUNT),
        ENUMS(PARAM_DECAY,         Sony_S_DSP_ADSR::VOICE_COUNT),
        ENUMS(PARAM_SUSTAIN_LEVEL, Sony_S_DSP_ADSR::VOICE_COUNT),
        ENUMS(PARAM_SUSTAIN_RATE,  Sony_S_DSP_ADSR::VOICE_COUNT),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT,          Sony_S_DSP_ADSR::VOICE_COUNT),  // TODO: remove
        ENUMS(INPUT_FM,            Sony_S_DSP_ADSR::VOICE_COUNT),  // TODO: remove
        ENUMS(INPUT_PM_ENABLE,     Sony_S_DSP_ADSR::VOICE_COUNT),  // TODO: remove
        ENUMS(INPUT_NOISE_ENABLE,  Sony_S_DSP_ADSR::VOICE_COUNT),  // TODO: remove
        INPUT_NOISE_FM,  // TODO: remove
        ENUMS(INPUT_GATE,          Sony_S_DSP_ADSR::VOICE_COUNT),
        ENUMS(INPUT_AMPLITUDE,     Sony_S_DSP_ADSR::VOICE_COUNT),
        ENUMS(INPUT_VOLUME_R,      Sony_S_DSP_ADSR::VOICE_COUNT),
        ENUMS(INPUT_ATTACK,        Sony_S_DSP_ADSR::VOICE_COUNT),
        ENUMS(INPUT_DECAY,         Sony_S_DSP_ADSR::VOICE_COUNT),
        ENUMS(INPUT_SUSTAIN_LEVEL, Sony_S_DSP_ADSR::VOICE_COUNT),
        ENUMS(INPUT_SUSTAIN_RATE,  Sony_S_DSP_ADSR::VOICE_COUNT),
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_AUDIO, 2), // TODO: remove
        ENUMS(OUTPUT_ENVELOPE, Sony_S_DSP_ADSR::VOICE_COUNT),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        NUM_LIGHTS
    };

    /// @brief Initialize a new S-DSP Chip module.
    ChipS_SMP_ADSR() {
        // setup parameters
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned osc = 0; osc < Sony_S_DSP_ADSR::VOICE_COUNT; osc++) {
            auto osc_name = "Voice " + std::to_string(osc + 1) + " ";
            configParam(PARAM_AMPLITUDE     + osc, -128, 127, 127, osc_name + "Amplitude");
            configParam(PARAM_ATTACK        + osc,    0,  15,   0, osc_name + "Attack");
            configParam(PARAM_DECAY         + osc,    0,   7,   0, osc_name + "Decay");
            configParam(PARAM_SUSTAIN_LEVEL + osc,    0,   7,   0, osc_name + "Sustain Level");
            configParam(PARAM_SUSTAIN_RATE  + osc,    0,  31,   0, osc_name + "Sustain Rate");
        }
        // TODO: remove. clear the shared RAM between the CPU and the S-DSP
        memset(ram, 0, sizeof ram);
        // reset the S-DSP emulator
        apu.reset();
    }

 protected:
    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    inline void process(const ProcessArgs &args) final {
        // -------------------------------------------------------------------
        // MARK: Gate input
        // -------------------------------------------------------------------
        // create bit-masks for the key-on and key-off state of each voice
        uint8_t key_on = 0;
        uint8_t key_off = 0;
        // iterate over the voices to detect key-on and key-off events
        for (unsigned voice = 0; voice < Sony_S_DSP_ADSR::VOICE_COUNT; voice++) {
            // get the voltage from the gate input port
            const auto gate = inputs[INPUT_GATE + voice].getVoltage();
            // process the voltage to detect key-on events
            key_on = key_on | (gateTriggers[voice][0].process(rescale(gate, 0.f, 2.f, 0.f, 1.f)) << voice);
            // process the inverted voltage to detect key-of events
            key_off = key_off | (gateTriggers[voice][1].process(rescale(10.f - gate, 0.f, 2.f, 0.f, 1.f)) << voice);
        }
        if (key_on) {  // a key-on event occurred from the gate input
            // write key off to enable all voices
            apu.write(Sony_S_DSP_ADSR::KEY_OFF, 0);
            // write the key-on value to the register
            apu.write(Sony_S_DSP_ADSR::KEY_ON, key_on);
        }
        if (key_off)  // a key-off event occurred from the gate input
            apu.write(Sony_S_DSP_ADSR::KEY_OFF, key_off);

        // -------------------------------------------------------------------
        // TODO: remove
        apu.write(Sony_S_DSP_ADSR::MAIN_VOLUME_LEFT,  127);
        apu.write(Sony_S_DSP_ADSR::MAIN_VOLUME_RIGHT, 127);
        apu.write(Sony_S_DSP_ADSR::FLAGS,             0);
        apu.write(Sony_S_DSP_ADSR::ECHO_FEEDBACK,     0);
        apu.write(Sony_S_DSP_ADSR::ECHO_DELAY,        0);
        apu.write(Sony_S_DSP_ADSR::ECHO_ENABLE,       0);
        apu.write(Sony_S_DSP_ADSR::NOISE_ENABLE,      0);
        apu.write(Sony_S_DSP_ADSR::PITCH_MODULATION,  0);
        apu.write(Sony_S_DSP_ADSR::ECHO_VOLUME_LEFT,  0);
        apu.write(Sony_S_DSP_ADSR::ECHO_VOLUME_RIGHT, 0);
        // -------------------------------------------------------------------

        for (unsigned voice = 0; voice < Sony_S_DSP_ADSR::VOICE_COUNT; voice++) {
            // shift the voice index over a nibble to get the bit mask for the
            // logical OR operator
            auto mask = voice << 4;

            // ---------------------------------------------------------------
            // MARK: Gain (Custom ADSR override)
            // ---------------------------------------------------------------
            // TODO: GAIN can be used to implement custom envelopes in your
            // program. There are 5 modes GAIN uses.
            // DIRECT
            //
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x7   |  0  |               PARAMETER                 |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // INCREASE (LINEAR)
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x7   |  1  |  1  |  0  |          PARAMETER          |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // INCREASE (BENT LINE)
            //
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x7   |  1  |  1  |  1  |          PARAMETER          |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // DECREASE (LINEAR)
            //
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x7   |  1  |  0  |  0  |          PARAMETER          |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // DECREASE (EXPONENTIAL)
            //
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x7   |  1  |  0  |  1  |          PARAMETER          |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // Direct: The value of GAIN is set to PARAMETER.
            //
            // Increase (Linear):
            //     GAIN slides to 1 with additions of 1/64.
            //
            // Increase (Bent Line):
            //     GAIN slides up with additions of 1/64 until it reaches 3/4,
            //     then it slides up to 1 with additions of 1/256.
            //
            // Decrease (Linear):
            //     GAIN slides down to 0 with subtractions of 1/64.
            //
            // Decrease (Exponential):
            //     GAIN slides down exponentially by getting multiplied by
            //     255/256.
            //
            // Table 2.3 Gain Parameters (Increate 0 -> 1 / Decrease 1 -> 0):
            // Parameter Value Increase Linear Increase Bentline   Decrease Linear Decrease Exponential
            // 00  INFINITE    INFINITE    INFINITE    INFINITE
            // 01  4.1s    7.2s    4.1s    38s
            // 02  3.1s    5.4s    3.1s    28s
            // 03  2.6s    4.6s    2.6s    24s
            // 04  2.0s    3.5s    2.0s    19s
            // 05  1.5s    2.6s    1.5s    14s
            // 06  1.3s    2.3s    1.3s    12s
            // 07  1.0s    1.8s    1.0s    9.4s
            // 08  770ms   1.3s    770ms   7.1s
            // 09  640ms   1.1s    640ms   5.9s
            // 0A  510ms   900ms   510ms   4.7s
            // 0B  380ms   670ms   380ms   3.5s
            // 0C  320ms   560ms   320ms   2.9s
            // 0D  260ms   450ms   260ms   2.4s
            // 0E  190ms   340ms   190ms   1.8s
            // 0F  160ms   280ms   160ms   1.5s
            // 10  130ms   220ms   130ms   1.2s
            // 11  96ms    170ms   96ms    880ms
            // 12  80ms    140ms   80ms    740ms
            // 13  64ms    110ms   64ms    590ms
            // 14  48ms    84ms    48ms    440ms
            // 15  40ms    70ms    40ms    370ms
            // 16  32ms    56ms    32ms    290ms
            // 17  24ms    42ms    24ms    220ms
            // 18  20ms    35ms    20ms    180ms
            // 19  16ms    28ms    16ms    150ms
            // 1A  12ms    21ms    12ms    110ms
            // 1B  10ms    18ms    10ms    92ms
            // 1C  8ms 14ms    8ms 74ms
            // 1D  6ms 11ms    6ms 55ms
            // 1E  4ms 7ms 4ms 37ms
            // 1F  2ms 3.5ms   2ms 18ms
            //
            // apu.write(mask | Sony_S_DSP_ADSR::GAIN, 64);
            // ---------------------------------------------------------------
            // MARK: ADSR
            // ---------------------------------------------------------------
            // the ADSR1 register is set from the attack and decay values
            auto attack = (uint8_t) params[PARAM_ATTACK + voice].getValue();
            auto decay = (uint8_t) params[PARAM_DECAY + voice].getValue();
            // the high bit of the ADSR1 register is set to enable the ADSR
            auto adsr1 = 0b10000000 | (decay << 4) | attack;
            apu.write(mask | Sony_S_DSP_ADSR::ADSR_1, adsr1);
            // the ADSR2 register is set from the sustain level and rate
            auto sustainLevel = (uint8_t) params[PARAM_SUSTAIN_LEVEL + voice].getValue();
            auto sustainRate = (uint8_t) params[PARAM_SUSTAIN_RATE + voice].getValue();
            auto adsr2 = (sustainLevel << 5) | sustainRate;
            apu.write(mask | Sony_S_DSP_ADSR::ADSR_2, adsr2);
            // ADSR output: 7-bit unsigned value (max 0x7F)
            float envelope = apu.read(mask | Sony_S_DSP_ADSR::ENVELOPE_OUT) / 127.f;
            outputs[OUTPUT_ENVELOPE + voice].setVoltage(10.f * envelope);
            // ADSR amplitude
            apu.write(mask | Sony_S_DSP_ADSR::VOLUME_LEFT,  params[PARAM_AMPLITUDE + voice].getValue());
        }
        apu.run(nullptr);
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for SPC700.
struct ChipS_SMP_ADSRWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit ChipS_SMP_ADSRWidget(ChipS_SMP_ADSR *module) {
        setModule(module);
        static constexpr auto panel = "res/S-SMP.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // individual oscillator controls
        for (unsigned i = 0; i < Sony_S_DSP_ADSR::VOICE_COUNT; i++) {
            // Gate
            addInput(createInput<PJ301MPort>(Vec(185, 40 + i * 41), module, ChipS_SMP_ADSR::INPUT_GATE + i));
            // Amplitude
            addInput(createInput<PJ301MPort>(Vec(220, 40 + i * 41), module, ChipS_SMP_ADSR::INPUT_AMPLITUDE + i));
            auto amplitude = createParam<Rogan2PWhite>(Vec(250, 35 + i * 41), module, ChipS_SMP_ADSR::PARAM_AMPLITUDE + i);
            amplitude->snap = true;
            addParam(amplitude);
            // ADSR - Attack
            addInput(createInput<PJ301MPort>(Vec(390, 40 + i * 41), module, ChipS_SMP_ADSR::INPUT_ATTACK + i));
            auto attack = createParam<Rogan2PGreen>(Vec(420, 35 + i * 41), module, ChipS_SMP_ADSR::PARAM_ATTACK + i);
            attack->snap = true;
            addParam(attack);
            // ADSR - Decay
            addInput(createInput<PJ301MPort>(Vec(460, 40 + i * 41), module, ChipS_SMP_ADSR::INPUT_DECAY + i));
            auto decay = createParam<Rogan2PBlue>(Vec(490, 35 + i * 41), module, ChipS_SMP_ADSR::PARAM_DECAY + i);
            decay->snap = true;
            addParam(decay);
            // ADSR - Sustain Level
            addInput(createInput<PJ301MPort>(Vec(530, 40 + i * 41), module, ChipS_SMP_ADSR::INPUT_SUSTAIN_LEVEL + i));
            auto sustainLevel = createParam<Rogan2PRed>(Vec(560, 35 + i * 41), module, ChipS_SMP_ADSR::PARAM_SUSTAIN_LEVEL + i);
            sustainLevel->snap = true;
            addParam(sustainLevel);
            // ADSR - Sustain Rate
            addInput(createInput<PJ301MPort>(Vec(600, 40 + i * 41), module, ChipS_SMP_ADSR::INPUT_SUSTAIN_RATE + i));
            auto sustainRate = createParam<Rogan2PWhite>(Vec(630, 35 + i * 41), module, ChipS_SMP_ADSR::PARAM_SUSTAIN_RATE + i);
            sustainRate->snap = true;
            addParam(sustainRate);
            // ADSR - Output
            addOutput(createOutput<PJ301MPort>(Vec(700, 40 + i * 41), module, ChipS_SMP_ADSR::OUTPUT_ENVELOPE + i));
        }
    }
};

/// the global instance of the model
rack::Model *modelChipS_SMP_ADSR = createModel<ChipS_SMP_ADSR, ChipS_SMP_ADSRWidget>("S_SMP_ADSR");
