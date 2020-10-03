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
        PARAM_FILTER,
        ENUMS(PARAM_GAIN, 2),
        ENUMS(PARAM_VOLUME, 2),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        INPUT_FILTER,
        ENUMS(INPUT_VOLUME, 2),
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
        configParam(PARAM_FILTER, 0, 3, 2, "Filter Coefficients");
        configParam(PARAM_GAIN + 0, 0.f, 2 * M_SQRT2, M_SQRT2 / 2, "Gain (Left Channel)", " dB", -10, 40);
        configParam(PARAM_GAIN + 1, 0.f, 2 * M_SQRT2, M_SQRT2 / 2, "Gain (Right Channel) ", " dB", -10, 40);
        configParam(PARAM_VOLUME + 0, -128, 127, 60, "Volume (Left Channel)");
        configParam(PARAM_VOLUME + 1, -128, 127, 60, "Volume (Right Channel)");
    }

 protected:
    /// @brief Get the filter parameter for the index and polyphony channel.
    ///
    /// @param index the index of the filter parameter to get
    /// @param channel the polyphony channel to get the filter parameter of
    /// @returns the filter parameter at given index for given channel
    ///
    inline int8_t getFilter(bool index, unsigned channel) {
        const int8_t param = params[PARAM_FILTER].getValue();
        return 0x1 & (param >> (1 - index));
    }

    /// @brief Get the volume level for the given lane and polyphony channel.
    ///
    /// @param lane the processing lane to get the volume level of
    /// @param channel the polyphony channel to get the volume of
    /// @returns the volume of the gate for given lane and channel
    ///
    inline int8_t getVolume(unsigned lane, unsigned channel) {
        const auto param = params[PARAM_VOLUME + lane].getValue();
        const auto cv = inputs[INPUT_VOLUME + lane].isConnected() ?
            inputs[INPUT_VOLUME + lane].getVoltage(channel) / 10.f : 1.f;
        return math::clamp(cv * param, -128.f, 127.f);
    }

    /// @brief Get the input signal for the given lane and polyphony channel.
    ///
    /// @param lane the processing lane to get the input signal of
    /// @param channel the polyphony channel to get the input signal of
    /// @returns the input signal for the given lane and polyphony channel
    ///
    inline int16_t getInput(unsigned lane, unsigned channel) {
        const auto gain = std::pow(params[PARAM_GAIN + lane].getValue(), 2.f);
        const auto cv = inputs[INPUT_AUDIO + lane].getVoltage(channel);
        return std::numeric_limits<uint8_t>::max() * gain * cv / 10.f;
    }

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
        for (unsigned lane = 0; lane < 2; lane++) {
            for (unsigned channel = 0; channel < channels; channel++) {
                apu[lane][channel].setFilter1(getFilter(0, channel));
                apu[lane][channel].setFilter2(getFilter(1, channel));
                apu[lane][channel].setVolume(getVolume(lane, channel));
                float sample = apu[lane][channel].run(getInput(lane, channel));
                sample = sample / std::numeric_limits<int16_t>::max();
                outputs[OUTPUT_AUDIO + lane].setVoltage(10.f * sample, channel);
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
        static constexpr auto panel = "res/S-SMP-Gauss.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // Filter Mode
        Knob* filter = createParam<Rogan3PBlue>(Vec(37, 35), module, ChipS_SMP_Gaussian::PARAM_FILTER);
        filter->snap = true;
        addParam(filter);
        for (unsigned i = 0; i < 2; i++) {
            // Stereo Input Ports
            addInput(createInput<PJ301MPort>(Vec(25 + 44 * i, 117), module, ChipS_SMP_Gaussian::INPUT_AUDIO + i));
            // Gain
            addParam(createParam<Trimpot>(Vec(27 + 44 * i, 165), module, ChipS_SMP_Gaussian::PARAM_GAIN + i));
            // Volume (Knob)
            auto volumeIdx = ChipS_SMP_Gaussian::PARAM_VOLUME + i;
            auto echoPos = Vec(20 + 44 * i, 221);
            Knob* volume;
            if (i)  // i == 1 -> right lane -> red knob
                volume = createParam<Rogan2PRed>(echoPos, module, volumeIdx);
            else  // i == 0 -> left lane -> white knob
                volume = createParam<Rogan2PWhite>(echoPos, module, volumeIdx);
            volume->snap = true;
            addParam(volume);
            // Volume (Port)
            addInput(createInput<PJ301MPort>(Vec(25 + 44 * i, 270), module, ChipS_SMP_Gaussian::INPUT_VOLUME + i));
            // Stereo Output Ports
            addOutput(createOutput<PJ301MPort>(Vec(25 + 44 * i, 324), module, ChipS_SMP_Gaussian::OUTPUT_AUDIO + i));
        }
    }
};

/// the global instance of the model
rack::Model *modelChipS_SMP_Gaussian = createModel<ChipS_SMP_Gaussian, ChipS_SMP_GaussianWidget>("S_SMP_Gauss");
