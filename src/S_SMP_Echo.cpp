// An echo effect module based on the S-SMP chip from Nintendo SNES.
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

/// An echo effect module based on the S-SMP chip from Nintendo SNES.
struct ChipS_SMP_Echo : Module {
 private:
    /// the Sony S-DSP echo effect emulator
    Sony_S_DSP_Echo apu;

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        PARAM_DELAY,
        PARAM_FEEDBACK,
        ENUMS(PARAM_MIX, Sony_S_DSP_Echo::BufferSample::CHANNELS),
        ENUMS(PARAM_FIR_COEFFICIENT, Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_AUDIO, Sony_S_DSP_Echo::BufferSample::CHANNELS),
        INPUT_DELAY,
        INPUT_FEEDBACK,
        ENUMS(INPUT_MIX, Sony_S_DSP_Echo::BufferSample::CHANNELS),
        ENUMS(INPUT_FIR_COEFFICIENT, Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT),
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_AUDIO, Sony_S_DSP_Echo::BufferSample::CHANNELS),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        NUM_LIGHTS
    };

    /// @brief Initialize a new S-DSP Chip module.
    ChipS_SMP_Echo() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned coeff = 0; coeff < Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT; coeff++)
            configParam(PARAM_FIR_COEFFICIENT  + coeff, -128, 127, apu.getFIR(coeff), "FIR Coefficient " + std::to_string(coeff + 1));
        configParam(PARAM_DELAY, 0, Sony_S_DSP_Echo::DELAY_LEVELS, 0, "Echo Delay", "ms", 0, Sony_S_DSP_Echo::MILLISECONDS_PER_DELAY_LEVEL);
        configParam(PARAM_FEEDBACK, -128, 127, 0, "Echo Feedback");
        configParam(PARAM_MIX + 0, -128, 127, 0, "Echo Mix (Left Channel)");
        configParam(PARAM_MIX + 1, -128, 127, 0, "Echo Mix (Right Channel)");
    }

 protected:
    /// @brief Return the value of the delay parameter from the panel.
    ///
    /// @returns the 8-bit delay parameter after applying CV modulations
    ///
    inline uint8_t getDelay() {
        const float param = params[PARAM_DELAY].getValue();
        const float cv = inputs[INPUT_DELAY].getVoltage() / 10.f;
        const float mod = Sony_S_DSP_Echo::DELAY_LEVELS * cv;
        const float MAX = static_cast<float>(Sony_S_DSP_Echo::DELAY_LEVELS);
        return clamp(param + mod, 0.f, MAX);
    }

    /// @brief Return the value of the feedback parameter from the panel.
    ///
    /// @returns the 8-bit feedback parameter after applying CV modulations
    ///
    inline int8_t getFeedback() {
        const float param = params[PARAM_FEEDBACK].getValue();
        const float cv = inputs[INPUT_FEEDBACK].getVoltage() / 10.f;
        const float mod = std::numeric_limits<int8_t>::max() * cv;
        static constexpr float MIN = std::numeric_limits<int8_t>::min();
        static constexpr float MAX = std::numeric_limits<int8_t>::max();
        return clamp(param + mod, MIN, MAX);
    }

    /// @brief Return the value of the mix parameter from the panel.
    ///
    /// @param channel the channel to get the mix level parameter for
    /// @returns the 8-bit mix parameter after applying CV modulations
    ///
    inline int8_t getMix(unsigned channel) {
        const float param = params[PARAM_MIX + channel].getValue();
        const float cv = inputs[INPUT_MIX + channel].getVoltage() / 10.f;
        const float mod = std::numeric_limits<int8_t>::max() * cv;
        static constexpr float MIN = std::numeric_limits<int8_t>::min();
        static constexpr float MAX = std::numeric_limits<int8_t>::max();
        return clamp(param + mod, MIN, MAX);
    }

    /// @brief Return the value of the FIR filter parameter from the panel.
    ///
    /// @param index the index of the FIR filter coefficient to get
    /// @returns the 8-bit FIR filter parameter for coefficient at given index
    ///
    inline int8_t getFIRCoefficient(unsigned index) {
        const float param = params[PARAM_FIR_COEFFICIENT + index].getValue();
        const float cv = inputs[INPUT_FIR_COEFFICIENT + index].getVoltage() / 10.f;
        const float mod = std::numeric_limits<int8_t>::max() * cv;
        static constexpr float MIN = std::numeric_limits<int8_t>::min();
        static constexpr float MAX = std::numeric_limits<int8_t>::max();
        return clamp(param + mod, MIN, MAX);
    }

    /// @brief Return the value of the stereo input from the panel.
    ///
    /// @param channel the channel to get the input voltage for
    /// @returns the 8-bit stereo input for the given channel
    ///
    inline int16_t getInput(unsigned channel) {
        static constexpr float MAX = std::numeric_limits<int16_t>::max();
        return MAX * inputs[INPUT_AUDIO + channel].getVoltage() / 5.f;
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    inline void process(const ProcessArgs &args) final {
        // update the delay parameters
        apu.setDelay(getDelay());
        apu.setFeedback(getFeedback());
        apu.setMixLeft(getMix(Sony_S_DSP_Echo::BufferSample::LEFT));
        apu.setMixRight(getMix(Sony_S_DSP_Echo::BufferSample::RIGHT));
        // update the FIR Coefficients
        for (unsigned i = 0; i < Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT; i++)
            apu.setFIR(i, getFIRCoefficient(i));
        // run a stereo sample through the echo buffer + filter
        auto output = apu.run(
            getInput(Sony_S_DSP_Echo::BufferSample::LEFT),
            getInput(Sony_S_DSP_Echo::BufferSample::RIGHT)
        );
        // write the stereo output to the ports
        for (unsigned i = 0; i < Sony_S_DSP_Echo::BufferSample::CHANNELS; i++) {
            // get the 16-bit sample as a floating point number in [0, 1]
            const auto sample = output.samples[i] / std::numeric_limits<int16_t>::max();
            // scale the [0.f, 1.f] 16-bit representation to a [0, 5]V signal
            outputs[OUTPUT_AUDIO + i].setVoltage(5.f * sample);
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for SPC700.
struct ChipS_SMP_EchoWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit ChipS_SMP_EchoWidget(ChipS_SMP_Echo *module) {
        setModule(module);
        static constexpr auto panel = "res/S-SMP-Echo.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // Panel Screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        for (unsigned i = 0; i < Sony_S_DSP_Echo::BufferSample::CHANNELS; i++) {
            // Echo Parameter (0 = delay, 1 = Feedback)
            auto echoParam = createParam<Rogan2PBlue>(Vec(20 + 44 * i, 51), module, ChipS_SMP_Echo::PARAM_DELAY + i);
            echoParam->snap = true;
            addParam(echoParam);
            addInput(createInput<PJ301MPort>(Vec(25 + 44 * i, 100), module, ChipS_SMP_Echo::INPUT_DELAY + i));
            // Echo Mix
            auto echoIdx = ChipS_SMP_Echo::PARAM_MIX + i;
            auto echoPos = Vec(20 + 44 * i, 163);
            Knob* echoMix;
            if (i)  // i == 1 -> right channel -> red knob
                echoMix = createParam<Rogan2PRed>(echoPos, module, echoIdx);
            else  // i == 0 -> left channel -> white knob
                echoMix = createParam<Rogan2PWhite>(echoPos, module, echoIdx);
            echoMix->snap = true;
            addParam(echoMix);
            addInput(createInput<PJ301MPort>(Vec(25 + 44 * i, 212), module, ChipS_SMP_Echo::INPUT_MIX + i));
            // Stereo Input Ports
            addInput(createInput<PJ301MPort>(Vec(25 + 44 * i, 269), module, ChipS_SMP_Echo::INPUT_AUDIO + i));
            // Stereo Output Ports
            addOutput(createOutput<PJ301MPort>(Vec(25 + 44 * i, 324), module, ChipS_SMP_Echo::OUTPUT_AUDIO + i));
        }
        // FIR Coefficients
        for (unsigned i = 0; i < Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT; i++) {
            addInput(createInput<PJ301MPort>(Vec(120, 28 + i * 43), module, ChipS_SMP_Echo::INPUT_FIR_COEFFICIENT + i));
            auto param = createParam<Rogan1PGreen>(Vec(162, 25 + i * 43), module, ChipS_SMP_Echo::PARAM_FIR_COEFFICIENT + i);
            param->snap = true;
            addParam(param);
        }
    }
};

/// the global instance of the model
rack::Model *modelChipS_SMP_Echo = createModel<ChipS_SMP_Echo, ChipS_SMP_EchoWidget>("S_SMP_Echo");
