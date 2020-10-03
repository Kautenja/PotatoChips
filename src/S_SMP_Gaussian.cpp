// A low-pass gate module based on the S-SMP chip from Nintendo SNES.
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
#include "dsp/sony_s_dsp_gaussian.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A low-pass gate module based on the S-SMP chip from Nintendo SNES.
struct ChipS_SMP_Gaussian : Module {
 private:
    /// the Sony S-DSP sound chip emulator
    Sony_S_DSP_Gaussian apu[2][PORT_MAX_CHANNELS];

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FILTER, 2),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_AUDIO, 2),
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
    ChipS_SMP_Gaussian() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(PARAM_FILTER + 0, 0, 1, 0, "Filter Mode 1", "");
        configParam(PARAM_FILTER + 1, 0, 1, 0, "Filter Mode 2", "");
    }

 protected:
    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    inline void process(const ProcessArgs &args) final {
        // get the number of polyphonic channels (defaults to 1 for monophonic).
        // also set the channels on the output ports based on the number of
        // channels
        unsigned channels = 1;
        for (unsigned port = 0; port < inputs.size(); port++)
            channels = std::max(inputs[port].getChannels(), static_cast<int>(channels));
        // set the number of polyphony channels for output ports
        for (unsigned port = 0; port < outputs.size(); port++)
            outputs[port].setChannels(channels);
        // process audio samples on the chip engine.
        for (unsigned i = 0; i < 2; i++) {  // iterate over the stereo pair
            for (unsigned channel = 0; channel < channels; channel++) {
                // set filter parameters
                apu[i][channel].setFilter1(params[PARAM_FILTER + 0].getValue());
                apu[i][channel].setFilter2(params[PARAM_FILTER + 1].getValue());
                // pass signal through the filter to get the output voltage
                float sample = apu[i][channel].run((1 << 8) * inputs[INPUT_AUDIO + i].getVoltage(channel) / 10.f);
                outputs[OUTPUT_AUDIO + i].setVoltage(10.f * sample / std::numeric_limits<int16_t>::max(), channel);
            }
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// @brief The panel widget for S-SMP-Gaussian.
struct ChipS_SMP_GaussianWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit ChipS_SMP_GaussianWidget(ChipS_SMP_Gaussian *module) {
        setModule(module);
        static constexpr auto panel = "res/S-SMP-Gaussian.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // Switches
        addParam(createParam<CKSS>(Vec(50, 30), module, ChipS_SMP_Gaussian::PARAM_FILTER + 0));
        addParam(createParam<CKSS>(Vec(50, 60), module, ChipS_SMP_Gaussian::PARAM_FILTER + 1));

        // Inputs
        addInput(createInput<PJ301MPort>(Vec(55, 290), module, ChipS_SMP_Gaussian::INPUT_AUDIO + 0));
        addInput(createInput<PJ301MPort>(Vec(105, 290), module, ChipS_SMP_Gaussian::INPUT_AUDIO + 1));
        // Outputs
        addOutput(createOutput<PJ301MPort>(Vec(55, 325), module, ChipS_SMP_Gaussian::OUTPUT_AUDIO + 0));
        addOutput(createOutput<PJ301MPort>(Vec(105, 325), module, ChipS_SMP_Gaussian::OUTPUT_AUDIO + 1));
    }
};

/// the global instance of the model
rack::Model *modelChipS_SMP_Gaussian = createModel<ChipS_SMP_Gaussian, ChipS_SMP_GaussianWidget>("S_SMP_Gaussian");
