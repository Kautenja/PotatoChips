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
    Sony_S_DSP_Gaussian apu;

    /// triggers for handling gate inputs for the voices
    rack::dsp::BooleanTrigger gateTriggers[8][2];

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ,          8),  // TODO: remove
        ENUMS(PARAM_PM_ENABLE,     8),  // TODO: remove
        ENUMS(PARAM_NOISE_ENABLE,  8),  // TODO: remove
        PARAM_NOISE_FREQ,  // TODO: remove
        ENUMS(PARAM_VOLUME_L,      8),
        ENUMS(PARAM_VOLUME_R,      8),
        ENUMS(PARAM_ATTACK,        8),  // TODO: remove
        ENUMS(PARAM_DECAY,         8),  // TODO: remove
        ENUMS(PARAM_SUSTAIN_LEVEL, 8),  // TODO: remove
        ENUMS(PARAM_SUSTAIN_RATE,  8),  // TODO: remove
        ENUMS(PARAM_ECHO_ENABLE,   8),  // TODO: remove
        PARAM_ECHO_DELAY,  // TODO: remove
        PARAM_ECHO_FEEDBACK,  // TODO: remove
        ENUMS(PARAM_VOLUME_ECHO, 2),  // TODO: remove
        ENUMS(PARAM_VOLUME_MAIN, 2),
        ENUMS(PARAM_FIR_COEFFICIENT, 8),  // TODO: remove
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT,          8),  // TODO: remove
        ENUMS(INPUT_FM,            8),  // TODO: remove
        ENUMS(INPUT_PM_ENABLE,     8),  // TODO: remove
        ENUMS(INPUT_NOISE_ENABLE,  8),  // TODO: remove
        INPUT_NOISE_FM,  // TODO: remove
        ENUMS(INPUT_GATE,          8),  // TODO: remove
        ENUMS(INPUT_VOLUME_L,      8),
        ENUMS(INPUT_VOLUME_R,      8),
        ENUMS(INPUT_ATTACK,        8),  // TODO: remove
        ENUMS(INPUT_DECAY,         8),  // TODO: remove
        ENUMS(INPUT_SUSTAIN_LEVEL, 8),  // TODO: remove
        ENUMS(INPUT_SUSTAIN_RATE,  8),  // TODO: remove
        ENUMS(INPUT_ECHO_ENABLE,   8),  // TODO: remove
        INPUT_ECHO_DELAY,  // TODO: remove
        INPUT_ECHO_FEEDBACK,  // TODO: remove
        ENUMS(INPUT_VOLUME_ECHO, 2),  // TODO: remove
        ENUMS(INPUT_VOLUME_MAIN, 2),
        ENUMS(INPUT_FIR_COEFFICIENT, 8),
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
        // setup parameters
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned osc = 0; osc < 8; osc++) {
            auto osc_name = "Voice " + std::to_string(osc + 1);
            configParam(PARAM_FREQ          + osc, -4.f, 4.f, 2.f, osc_name + " Frequency", " Hz", 2, dsp::FREQ_C4);
            configParam(PARAM_VOLUME_L      + osc, -128, 127, 127, osc_name + " Volume (Left)");
            configParam(PARAM_VOLUME_R      + osc, -128, 127, 127, osc_name + " Volume (Right)");
            configParam(PARAM_ATTACK        + osc,    0,  15,   0, osc_name + " Envelope Attack");
            configParam(PARAM_DECAY         + osc,    0,   7,   0, osc_name + " Envelope Decay");
            configParam(PARAM_SUSTAIN_LEVEL + osc,    0,   7,   0, osc_name + " Envelope Sustain Level");
            configParam(PARAM_SUSTAIN_RATE  + osc,    0,  31,   0, osc_name + " Envelope Sustain Rate");
            configParam(PARAM_NOISE_ENABLE  + osc,    0,   1,   0, osc_name + " Noise Enable");
            configParam(PARAM_ECHO_ENABLE   + osc,    0,   1,   1, osc_name + " Echo Enable");
            if (osc > 0) {  // voice 0 does not have phase modulation
                osc_name = "Voice " + std::to_string(osc) + " -> " + osc_name;
                configParam(PARAM_PM_ENABLE + osc, 0, 1, 0, osc_name + " Phase Modulation Enable");
            }
        }
        for (unsigned coeff = 0; coeff < 8; coeff++) {
            // the first FIR coefficient defaults to 0x7f = 127 and the other
            // coefficients are 0 by default
            configParam(PARAM_FIR_COEFFICIENT  + coeff, -128, 127, (coeff ? 0 : 127), "FIR Coefficient " + std::to_string(coeff + 1));
        }
        configParam(PARAM_NOISE_FREQ,         0,  31,  16, "Noise Frequency");
        configParam(PARAM_ECHO_DELAY,         0,  15,   0, "Echo Delay", "ms", 0, 16);
        configParam(PARAM_ECHO_FEEDBACK,   -128, 127,   0, "Echo Feedback");
        configParam(PARAM_VOLUME_ECHO + 0, -128, 127, 127, "Echo Volume (Left)");
        configParam(PARAM_VOLUME_ECHO + 1, -128, 127, 127, "Echo Volume (Right)");
        configParam(PARAM_VOLUME_MAIN + 0, -128, 127, 127, "Main Volume (Left)");
        configParam(PARAM_VOLUME_MAIN + 1, -128, 127, 127, "Main Volume (Right)");
    }

 protected:
    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    inline void process(const ProcessArgs &args) final {
        short sample[2] = {0, 0};
        auto audioInput = (1 << 8) * inputs[INPUT_GATE].getVoltage() / 10.f;
        apu.run(audioInput, sample);
        outputs[OUTPUT_AUDIO + 0].setVoltage(5.f * sample[0] / std::numeric_limits<int16_t>::max());
        outputs[OUTPUT_AUDIO + 1].setVoltage(5.f * sample[1] / std::numeric_limits<int16_t>::max());
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for SPC700.
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
        // individual oscillator controls
        for (unsigned i = 0; i < 8; i++)
            addInput(createInput<PJ301MPort>(Vec(15, 40 + i * 41), module, ChipS_SMP_Gaussian::INPUT_GATE + i));
        // Mixer & Output - Left Channel
        auto volumeLeft = createParam<Rogan2PWhite>(Vec(45, 230), module, ChipS_SMP_Gaussian::PARAM_VOLUME_MAIN + 0);
        volumeLeft->snap = true;
        addParam(volumeLeft);
        addInput(createInput<PJ301MPort>(Vec(55, 280), module, ChipS_SMP_Gaussian::INPUT_VOLUME_MAIN + 0));
        addOutput(createOutput<PJ301MPort>(Vec(55, 325), module, ChipS_SMP_Gaussian::OUTPUT_AUDIO + 0));
        // Mixer & Output - Right Channel
        auto volumeRight = createParam<Rogan2PRed>(Vec(95, 230), module, ChipS_SMP_Gaussian::PARAM_VOLUME_MAIN + 1);
        volumeRight->snap = true;
        addParam(volumeRight);
        addInput(createInput<PJ301MPort>(Vec(105, 280), module, ChipS_SMP_Gaussian::INPUT_VOLUME_MAIN + 1));
        addOutput(createOutput<PJ301MPort>(Vec(105, 325), module, ChipS_SMP_Gaussian::OUTPUT_AUDIO + 1));
    }
};

/// the global instance of the model
rack::Model *modelChipS_SMP_Gaussian = createModel<ChipS_SMP_Gaussian, ChipS_SMP_GaussianWidget>("S_SMP_Gaussian");
