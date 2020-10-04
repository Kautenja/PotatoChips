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
    /// the number of processing lanes on the module
    static constexpr unsigned LANES = 2;

 private:
    /// the Sony S-DSP ADSR enveloper generator emulator
    Sony_S_DSP_ADSR apu;
    /// a trigger for handling input gate signals
    rack::dsp::BooleanTrigger trigger[2];

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_AMPLITUDE,     LANES),
        ENUMS(PARAM_ATTACK,        LANES),
        ENUMS(PARAM_DECAY,         LANES),
        ENUMS(PARAM_SUSTAIN_LEVEL, LANES),
        ENUMS(PARAM_SUSTAIN_RATE,  LANES),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_GATE,          LANES),
        ENUMS(INPUT_RETRIG,        LANES),
        ENUMS(INPUT_AMPLITUDE,     LANES),
        ENUMS(INPUT_ATTACK,        LANES),
        ENUMS(INPUT_DECAY,         LANES),
        ENUMS(INPUT_SUSTAIN_LEVEL, LANES),
        ENUMS(INPUT_SUSTAIN_RATE,  LANES),
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_ENVELOPE, LANES),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds { NUM_LIGHTS };

    /// @brief Initialize a new S-DSP Chip module.
    ChipS_SMP_ADSR() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned lane = 0; lane < LANES; lane++) {
            configParam(PARAM_AMPLITUDE     + lane, -128, 127, 127, "Amplitude");
            configParam(PARAM_ATTACK        + lane,    0,  15,   0, "Attack");
            configParam(PARAM_DECAY         + lane,    0,   7,   0, "Decay");
            configParam(PARAM_SUSTAIN_LEVEL + lane,    0,   7,   0, "Sustain Level");
            configParam(PARAM_SUSTAIN_RATE  + lane,    0,  31,   0, "Sustain Rate");
        }
    }

 protected:
    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    inline void process(const ProcessArgs &args) final {
        // ADSR parameters
        apu.setAttack(params[PARAM_ATTACK].getValue());
        apu.setDecay(params[PARAM_DECAY].getValue());
        apu.setSustainRate(params[PARAM_SUSTAIN_RATE].getValue());
        apu.setSustainLevel(params[PARAM_SUSTAIN_LEVEL].getValue());
        apu.setAmplitude(params[PARAM_AMPLITUDE].getValue());
        // Gate + Retrig input
        const bool keyOn = trigger[0].process(rescale(inputs[INPUT_GATE].getVoltage(), 0.f, 2.f, 0.f, 1.f));
        const bool retrig = trigger[1].process(rescale(inputs[INPUT_RETRIG].getVoltage(), 0.f, 2.f, 0.f, 1.f));
        // Enveloper generator output
        auto sample = apu.run(keyOn || retrig, trigger[0].state);
        outputs[OUTPUT_ENVELOPE].setVoltage(10.f * sample / 128.f);
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
        static constexpr auto panel = "res/S-SMP-ADSR.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        for (unsigned i = 0; i < ChipS_SMP_ADSR::LANES; i++) {
            // Gate, Retrig, Output
            addInput(createInput<PJ301MPort>(Vec(20, 45 + 169 * i), module, ChipS_SMP_ADSR::INPUT_GATE + i));
            addInput(createInput<PJ301MPort>(Vec(20, 100 + 169 * i), module, ChipS_SMP_ADSR::INPUT_RETRIG + i));
            addOutput(createOutput<PJ301MPort>(Vec(20, 156 + 169 * i), module, ChipS_SMP_ADSR::OUTPUT_ENVELOPE + i));
            // Amplitude
            auto amplitude = createParam<BefacoSlidePot>(Vec(66, 22 + 169 * i), module, ChipS_SMP_ADSR::PARAM_AMPLITUDE + i);
            amplitude->snap = true;
            addParam(amplitude);
            addInput(createInput<PJ301MPort>(Vec(61, 157 + 169 * i), module, ChipS_SMP_ADSR::INPUT_AMPLITUDE + i));
            // Attack
            auto attack = createParam<BefacoSlidePot>(Vec(100, 22 + 169 * i), module, ChipS_SMP_ADSR::PARAM_ATTACK + i);
            attack->snap = true;
            addParam(attack);
            addInput(createInput<PJ301MPort>(Vec(95, 157 + 169 * i), module, ChipS_SMP_ADSR::INPUT_ATTACK + i));
            // Decay
            auto decay = createParam<BefacoSlidePot>(Vec(134, 22 + 169 * i), module, ChipS_SMP_ADSR::PARAM_DECAY + i);
            decay->snap = true;
            addParam(decay);
            addInput(createInput<PJ301MPort>(Vec(129, 157 + 169 * i), module, ChipS_SMP_ADSR::INPUT_DECAY + i));
            // Sustain Level
            auto sustainLevel = createParam<BefacoSlidePot>(Vec(168, 22 + 169 * i), module, ChipS_SMP_ADSR::PARAM_SUSTAIN_LEVEL + i);
            sustainLevel->snap = true;
            addParam(sustainLevel);
            addInput(createInput<PJ301MPort>(Vec(163, 157 + 169 * i), module, ChipS_SMP_ADSR::INPUT_SUSTAIN_LEVEL + i));
            // Sustain Rate
            auto sustainRate = createParam<BefacoSlidePot>(Vec(202, 22 + 169 * i), module, ChipS_SMP_ADSR::PARAM_SUSTAIN_RATE + i);
            sustainRate->snap = true;
            addParam(sustainRate);
            addInput(createInput<PJ301MPort>(Vec(197, 157 + 169 * i), module, ChipS_SMP_ADSR::INPUT_SUSTAIN_RATE + i));
        }
    }
};

/// the global instance of the model
rack::Model *modelChipS_SMP_ADSR = createModel<ChipS_SMP_ADSR, ChipS_SMP_ADSRWidget>("S_SMP_ADSR");
