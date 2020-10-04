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
    rack::dsp::BooleanTrigger trigger;

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
        // Gate input
        const auto gate = inputs[INPUT_GATE].getVoltage();
        const bool keyOn = trigger.process(rescale(gate, 0.f, 2.f, 0.f, 1.f));
        // Enveloper generator output
        auto sample = apu.run(keyOn, trigger.state);
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
        static constexpr auto panel = "res/S-SMP.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // Gate
        addInput(createInput<PJ301MPort>(Vec(185, 40), module, ChipS_SMP_ADSR::INPUT_GATE));
        // Amplitude
        addInput(createInput<PJ301MPort>(Vec(220, 40), module, ChipS_SMP_ADSR::INPUT_AMPLITUDE));
        auto amplitude = createParam<Rogan2PWhite>(Vec(250, 35), module, ChipS_SMP_ADSR::PARAM_AMPLITUDE);
        amplitude->snap = true;
        addParam(amplitude);
        // Attack
        addInput(createInput<PJ301MPort>(Vec(390, 40), module, ChipS_SMP_ADSR::INPUT_ATTACK));
        auto attack = createParam<Rogan2PGreen>(Vec(420, 35), module, ChipS_SMP_ADSR::PARAM_ATTACK);
        attack->snap = true;
        addParam(attack);
        // Decay
        addInput(createInput<PJ301MPort>(Vec(460, 40), module, ChipS_SMP_ADSR::INPUT_DECAY));
        auto decay = createParam<Rogan2PBlue>(Vec(490, 35), module, ChipS_SMP_ADSR::PARAM_DECAY);
        decay->snap = true;
        addParam(decay);
        // Sustain Level
        addInput(createInput<PJ301MPort>(Vec(530, 40), module, ChipS_SMP_ADSR::INPUT_SUSTAIN_LEVEL));
        auto sustainLevel = createParam<Rogan2PRed>(Vec(560, 35), module, ChipS_SMP_ADSR::PARAM_SUSTAIN_LEVEL);
        sustainLevel->snap = true;
        addParam(sustainLevel);
        // Sustain Rate
        addInput(createInput<PJ301MPort>(Vec(600, 40), module, ChipS_SMP_ADSR::INPUT_SUSTAIN_RATE));
        auto sustainRate = createParam<Rogan2PWhite>(Vec(630, 35), module, ChipS_SMP_ADSR::PARAM_SUSTAIN_RATE);
        sustainRate->snap = true;
        addParam(sustainRate);
        // Output
        addOutput(createOutput<PJ301MPort>(Vec(700, 40), module, ChipS_SMP_ADSR::OUTPUT_ENVELOPE));
    }
};

/// the global instance of the model
rack::Model *modelChipS_SMP_ADSR = createModel<ChipS_SMP_ADSR, ChipS_SMP_ADSRWidget>("S_SMP_ADSR");
