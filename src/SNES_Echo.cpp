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
#include "dsp/sony_s_dsp_echo.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Sony S-DSP chip (from Nintendo SNES) emulator module.
struct ChipSNES_Echo : Module {
 private:
    /// the Sony S-DSP echo effect emulator
    Sony_S_DSP_Echo apu;

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        PARAM_ECHO_DELAY,
        PARAM_ECHO_FEEDBACK,
        ENUMS(PARAM_MIX_ECHO, 2),
        ENUMS(PARAM_FIR_COEFFICIENT, Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_AUDIO, 2),
        INPUT_ECHO_DELAY,
        INPUT_ECHO_FEEDBACK,
        ENUMS(INPUT_MIX_ECHO, 2),
        ENUMS(INPUT_FIR_COEFFICIENT, Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT),
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_AUDIO, 2),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        NUM_LIGHTS
    };

    /// @brief Initialize a new S-DSP Chip module.
    ChipSNES_Echo() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned coeff = 0; coeff < Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT; coeff++) {
            configParam(PARAM_FIR_COEFFICIENT  + coeff, -128, 127, apu.getFIR(coeff), "FIR Coefficient " + std::to_string(coeff + 1));
        }
        configParam(PARAM_ECHO_DELAY, 0, Sony_S_DSP_Echo::DELAY_LEVELS, 0, "Echo Delay", "ms", 0, Sony_S_DSP_Echo::MILLISECONDS_PER_DELAY_LEVEL);
        configParam(PARAM_ECHO_FEEDBACK, -128, 127, 0, "Echo Feedback");
        configParam(PARAM_MIX_ECHO + 0, -128, 127, 0, "Echo Mix (Left Channel)");
        configParam(PARAM_MIX_ECHO + 1, -128, 127, 0, "Echo Mix (Right Channel)");
    }

 protected:
    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    inline void process(const ProcessArgs &args) final {
        // delay parameters
        apu.setDelay(params[PARAM_ECHO_DELAY].getValue());
        apu.setFeedback(params[PARAM_ECHO_FEEDBACK].getValue());
        apu.setMixLeft(params[PARAM_MIX_ECHO + 0].getValue());
        apu.setMixRight(params[PARAM_MIX_ECHO + 1].getValue());
        // FIR Coefficients
        for (unsigned i = 0; i < Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT; i++)
            apu.setFIR(i, params[PARAM_FIR_COEFFICIENT + i].getValue());
        // run a stereo sample through the echo
        int16_t sample[2] = {0, 0};
        apu.run(
            std::numeric_limits<int16_t>::max() * inputs[INPUT_AUDIO + 0].getVoltage() / 5.f,
            std::numeric_limits<int16_t>::max() * inputs[INPUT_AUDIO + 1].getVoltage() / 5.f,
            sample
        );
        // write the stereo output to the ports
        outputs[OUTPUT_AUDIO + 0].setVoltage(5.f * sample[0] / std::numeric_limits<int16_t>::max());
        outputs[OUTPUT_AUDIO + 1].setVoltage(5.f * sample[1] / std::numeric_limits<int16_t>::max());
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for SPC700.
struct ChipSNES_EchoWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit ChipSNES_EchoWidget(ChipSNES_Echo *module) {
        setModule(module);
        static constexpr auto panel = "res/S-SMP-Echo.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // Panel Screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        for (unsigned i = 0; i < 2; i++) {
            // Echo Parameter (0 = delay, 1 = Feedback)
            auto echoParam = createParam<Rogan2PBlue>(Vec(20 + 44 * i, 51), module, ChipSNES_Echo::PARAM_ECHO_DELAY + i);
            echoParam->snap = true;
            addParam(echoParam);
            addInput(createInput<PJ301MPort>(Vec(25 + 44 * i, 100), module, ChipSNES_Echo::INPUT_ECHO_DELAY + i));
            // Echo Mix
            auto echoIdx = ChipSNES_Echo::PARAM_MIX_ECHO + i;
            auto echoPos = Vec(20 + 44 * i, 163);
            Knob* echoMix;
            if (i)  // i == 2 -> right channel -> red knob
                echoMix = createParam<Rogan2PRed>(echoPos, module, echoIdx);
            else  // i == 0 -> left channel -> white knob
                echoMix = createParam<Rogan2PWhite>(echoPos, module, echoIdx);
            echoMix->snap = true;
            addParam(echoMix);
            addInput(createInput<PJ301MPort>(Vec(25 + 44 * i, 212), module, ChipSNES_Echo::INPUT_MIX_ECHO + i));
            // Stereo Input Ports
            addInput(createInput<PJ301MPort>(Vec(25 + 44 * i, 269), module, ChipSNES_Echo::INPUT_AUDIO + i));
            // Stereo Output Ports
            addOutput(createOutput<PJ301MPort>(Vec(25 + 44 * i, 324), module, ChipSNES_Echo::OUTPUT_AUDIO + i));
        }
        // FIR Coefficients
        for (unsigned i = 0; i < Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT; i++) {
            addInput(createInput<PJ301MPort>(Vec(120, 28 + i * 43), module, ChipSNES_Echo::INPUT_FIR_COEFFICIENT + i));
            auto param = createParam<Rogan1PGreen>(Vec(162, 25 + i * 43), module, ChipSNES_Echo::PARAM_FIR_COEFFICIENT + i);
            param->snap = true;
            addParam(param);
        }
    }
};

/// the global instance of the model
rack::Model *modelChipSNES_Echo = createModel<ChipSNES_Echo, ChipSNES_EchoWidget>("SNES_Echo");
