// A low-pass gate module based on the S-SMP chip from Nintendo SNES.
// Copyright 2020 Christian Kauten
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
#include "dsp/sony_s_dsp/gaussian_interpolation_filter.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A low-pass gate module based on the S-SMP chip from Nintendo SNES.
struct SuperLPG : Module {
    /// the number of processing lanes on the module
    static constexpr unsigned LANES = 2;

    /// the mode the filter is in
    uint8_t filterMode = 0;

    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        PARAM_FILTER,
        ENUMS(PARAM_GAIN, LANES),
        ENUMS(PARAM_VOLUME, LANES),
        ENUMS(PARAM_FREQ, LANES),
        PARAM_BYPASS,
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        INPUT_FILTER,
        ENUMS(INPUT_VOLUME, LANES),
        ENUMS(INPUT_AUDIO, LANES),
        ENUMS(INPUT_VOCT, LANES),
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_AUDIO, LANES),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(LIGHT_VU_INPUT,  3 * LANES),
        ENUMS(LIGHT_VU_OUTPUT, 3 * LANES),
        ENUMS(LIGHTS_FILTER,   3),
        NUM_LIGHTS
    };

    /// @brief Initialize a new S-SMP(Gauss) Chip module.
    SuperLPG() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam<TriggerParamQuantity>(PARAM_FILTER, 0, 1, 0, "Filter Coefficients");
        configParam(PARAM_GAIN + 0, 0, M_SQRT2, 1, "Gain (Left Channel)", " dB", -10, 40);
        configParam(PARAM_GAIN + 1, 0, M_SQRT2, 1, "Gain (Right Channel)", " dB", -10, 40);
        configParam(PARAM_VOLUME + 0, -128, 127, 60, "Volume (Left Channel)");
        configParam(PARAM_VOLUME + 1, -128, 127, 60, "Volume (Right Channel)");
        configParam(PARAM_FREQ + 0, -5, 5, 0, "Frequency (Left Channel)",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -5, 5, 0, "Frequency (Right Channel)", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam<BooleanParamQuantity>(PARAM_BYPASS, 0, 1, 0, "Bypass");
        lightDivider.setDivision(512);
    }

    /// @brief Respond to the module being reset by the engine.
    inline void onReset() override {
        filterMode = 0;
    }

    /// @brief Respond to the module being randomized by the engine.
    inline void onRandomize() override {
        filterMode = random::u32() % SonyS_DSP::GaussianInterpolationFilter::FILTER_MODES;
    }

    /// @brief Return a JSON representation of this module's state
    ///
    /// @returns a new JSON object with this object's serialized state data
    ///
    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "filterMode", json_integer(filterMode));
        return rootJ;
    }

    /// @brief Return the object to the given serialized state.
    ///
    /// @returns a JSON object with object serialized state data to restore
    ///
    void dataFromJson(json_t* rootJ) override {
        json_t* filterModeObject = json_object_get(rootJ, "filterMode");
        if (filterModeObject) filterMode = json_integer_value(filterModeObject);
    }

 protected:
    /// the Sony S-DSP sound chip emulator
    SonyS_DSP::GaussianInterpolationFilter apu[LANES][PORT_MAX_CHANNELS];

    /// a loudness compensation multiplier for the filter mode
    float loudnessCompensation = 1.f;

    /// a trigger for handling presses to the filter mode button
    rack::dsp::SchmittTrigger filterModeTrigger;

    /// a clock divider for running LED updates slower than audio rate
    rack::dsp::ClockDivider lightDivider;

    /// a VU meter for measuring the input audio levels
    rack::dsp::VuMeter2 inputVUMeter[LANES];
    /// a VU meter for measuring the output audio levels
    rack::dsp::VuMeter2 outputVUMeter[LANES];

    /// @brief Get the input signal frequency.
    ///
    /// @param lane the processing lane to get the input signal of
    /// @param channel the polyphony channel to get the input signal of
    /// @returns the input signal for the given lane and polyphony channel
    ///
    inline uint16_t getFrequency(unsigned lane, unsigned channel) {
        float param = params[PARAM_FREQ + lane].getValue();
        const auto normal = lane ? inputs[INPUT_VOCT + lane - 1].getVoltage(channel) : 0.f;
        param += inputs[INPUT_VOCT + lane].getNormalVoltage(normal, channel);
        float frequency = rack::dsp::FREQ_C4 * powf(2.0, param);
        frequency = rack::clamp(frequency, 0.0f, 20000.0f);
        return SonyS_DSP::get_pitch(frequency);
    }

    /// @brief Get the volume level.
    ///
    /// @param lane the processing lane to get the volume level of
    /// @param channel the polyphony channel to get the volume of
    /// @returns the volume of the gate for given lane and channel
    ///
    inline int8_t getVolume(unsigned lane, unsigned channel) {
        const auto normal = lane ? inputs[INPUT_VOLUME + lane - 1].getVoltage(channel) : 10.f;
        const float voltage = inputs[INPUT_VOLUME + lane].getNormalVoltage(normal, channel);
        inputs[INPUT_VOLUME + lane].setVoltage(voltage, channel);
        const auto param = params[PARAM_VOLUME + lane].getValue();
        return math::clamp(param * voltage / 10.f, -128.f, 127.f);
    }

    /// @brief Get the input signal.
    ///
    /// @param lane the processing lane to get the input signal of
    /// @param channel the polyphony channel to get the input signal of
    /// @returns the input signal for the given lane and polyphony channel
    ///
    inline float getInput(unsigned lane, unsigned channel) {
        const auto normal = lane ? inputs[INPUT_AUDIO + lane - 1].getVoltage(channel) : 0.f;
        const auto voltage = inputs[INPUT_AUDIO + lane].getNormalVoltage(normal, channel);
        const auto gain = std::pow(params[PARAM_GAIN + lane].getValue(), 2.f);
        const auto input = gain * voltage;
        // process the input on the VU meter
        inputVUMeter[lane].process(APP->engine->getSampleTime(), input / 5.f);
        // "quantize" the floating point value into an 8-bit container
        return input;
    }

    /// @brief Get the input signal.
    ///
    /// @param lane the processing lane to get the input signal of
    /// @param channel the polyphony channel to get the input signal of
    /// @returns the input signal for the given lane and polyphony channel
    ///
    inline int8_t getInputFinite(unsigned lane, unsigned channel) {
        return std::numeric_limits<int8_t>::max() * math::clamp(getInput(lane, channel) / 5.f, -1.f, 1.f);
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param lane the processing lane to get the input signal of
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processChannel(unsigned lane, unsigned channel) {
        apu[lane][channel].setFrequency(getFrequency(lane, channel));
        apu[lane][channel].setFilter(3 - filterMode);
        apu[lane][channel].setVolume(getVolume(lane, channel));
        float sample = apu[lane][channel].run(getInputFinite(lane, channel));
        sample = loudnessCompensation * sample / (1 << 14);
        outputVUMeter[lane].process(APP->engine->getSampleTime(), sample);
        const auto voltage = 5.f * sample;
        // clamp to a real-world realistic boundary of 8V
        outputs[OUTPUT_AUDIO + lane].setVoltage(math::clamp(voltage, -8.f, 8.f), channel);
    }

    /// @brief Set the given VU meter light based on given VU meter.
    ///
    /// @param vuMeter the VU meter to get the data from
    /// @param light the light to update from the VU meter data
    ///
    inline void setVULight(rack::dsp::VuMeter2& vuMeter, rack::engine::Light* light) {
        // get the global brightness scale from -12 to 3
        auto brightness = vuMeter.getBrightness(-12, 3);
        // set the red light based on total brightness and
        // brightness from 0dB to 3dB
        (light + 0)->setBrightness(brightness * vuMeter.getBrightness(0, 3));
        // set the red light based on inverted total brightness and
        // brightness from -12dB to 0dB
        (light + 1)->setBrightness((1 - brightness) * vuMeter.getBrightness(-12, 0));
        // set the blue light to off
        (light + 2)->setBrightness(0);
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    void process(const ProcessArgs &args) final {
        // get the number of polyphonic channels (defaults to 1 for monophonic).
        // also set the channels on the output ports based on the number of
        // channels
        unsigned channels = 1;
        for (unsigned port = 0; port < NUM_INPUTS; port++)
            channels = std::max(inputs[port].getChannels(), static_cast<int>(channels));
        // set the number of polyphony channels for output ports
        for (unsigned port = 0; port < NUM_OUTPUTS; port++)
            outputs[port].setChannels(channels);
        // detect presses to the trigger and cycle the filter mode
        if (filterModeTrigger.process(params[PARAM_FILTER].getValue())) {
            // update the filter mode and cycle around the maximal value
            filterMode = (filterMode + 1) % SonyS_DSP::GaussianInterpolationFilter::FILTER_MODES;
            // update loudness compensation based on the power 2 of the mode.
            // Because the reciprocal of the filterMode is used on the emulator,
            // this has the effect of making low modes get more compensation.
            loudnessCompensation = pow(2, filterMode);
        }
        if (params[PARAM_BYPASS].getValue()) {  // bypass the chip emulator
            for (unsigned lane = 0; lane < LANES; lane++) {
                for (unsigned channel = 0; channel < channels; channel++) {
                    auto input = getInput(lane, channel);
                    outputVUMeter[lane].process(args.sampleTime, input / 5.f);
                    outputs[OUTPUT_AUDIO + lane].setVoltage(input, channel);
                }
            }
        } else {  // process audio samples on the chip engine.
            for (unsigned lane = 0; lane < LANES; lane++) {
                for (unsigned channel = 0; channel < channels; channel++)
                    processChannel(lane, channel);
            }
        }
        if (lightDivider.process()) {
            setVULight(inputVUMeter[0], &lights[LIGHT_VU_INPUT]);
            setVULight(inputVUMeter[1], &lights[LIGHT_VU_INPUT + 3]);
            setVULight(outputVUMeter[0], &lights[LIGHT_VU_OUTPUT]);
            setVULight(outputVUMeter[1], &lights[LIGHT_VU_OUTPUT + 3]);
            // set the envelope mode light in RGB order with the color code:
            // Red   <- filterMode == 0 -> Loud
            // Green <- filterMode == 1 -> Weird
            // Blue  <- filterMode == 2 -> Quiet
            // Black <- filterMode == 3 -> Barely Audible
            auto deltaTime = args.sampleTime * lightDivider.getDivision();
            bool red = filterMode == 0;
            lights[LIGHTS_FILTER + 0].setSmoothBrightness(red, deltaTime);
            bool green = filterMode == 1;
            lights[LIGHTS_FILTER + 1].setSmoothBrightness(green, deltaTime);
            bool blue = filterMode == 2;
            lights[LIGHTS_FILTER + 2].setSmoothBrightness(blue, deltaTime);
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// @brief The panel widget for S-SMP-Gauss.
struct SuperLPGWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit SuperLPGWidget(SuperLPG *module) {
        setModule(module);
        static constexpr auto panel = "res/S-SMP-Gauss-Light.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // Bypass
        addParam(createParam<CKSS>(Vec(15, 25), module, SuperLPG::PARAM_BYPASS));
        // Filter Mode
        addParam(createParam<TL1105>(Vec(45, 55), module, SuperLPG::PARAM_FILTER));
        addChild(createLight<MediumLight<RedGreenBlueLight>>(Vec(45, 70), module, SuperLPG::LIGHTS_FILTER));
        for (unsigned i = 0; i < SuperLPG::LANES; i++) {
            // Frequency
            addParam(createParam<Trimpot>(Vec(27 + 44 * i, 15), module, SuperLPG::PARAM_FREQ + i));
            addInput(createInput<PJ301MPort>(Vec(25 + 44 * i, 30), module, SuperLPG::INPUT_VOCT + i));
            // Stereo Input Ports
            addChild(createLight<MediumLight<RedGreenBlueLight>>(Vec(15 + 44 * i, 100), module, SuperLPG::LIGHT_VU_INPUT + 3 * i));
            addInput(createInput<PJ301MPort>(Vec(25 + 44 * i, 117), module, SuperLPG::INPUT_AUDIO + i));
            // Input Gain
            addParam(createParam<Trimpot>(Vec(27 + 44 * i, 165), module, SuperLPG::PARAM_GAIN + i));
            // Volume
            auto volumeIdx = SuperLPG::PARAM_VOLUME + i;
            auto echoPos = Vec(20 + 44 * i, 221);
            Knob* volume;
            if (i)  // i == 1 -> right lane -> red knob
                volume = createParam<Rogan2PRed>(echoPos, module, volumeIdx);
            else  // i == 0 -> left lane -> white knob
                volume = createParam<Rogan2PWhite>(echoPos, module, volumeIdx);
            volume->snap = true;
            addParam(volume);
            addInput(createInput<PJ301MPort>(Vec(25 + 44 * i, 270), module, SuperLPG::INPUT_VOLUME + i));
            // Stereo Output Ports
            addChild(createLight<MediumLight<RedGreenBlueLight>>(Vec(15 + 44 * i, 310), module, SuperLPG::LIGHT_VU_OUTPUT + 3 * i));
            addOutput(createOutput<PJ301MPort>(Vec(25 + 44 * i, 324), module, SuperLPG::OUTPUT_AUDIO + i));
        }
    }

    void appendContextMenu(Menu* menu) override {
        // get a pointer to the module
        SuperLPG* const module = dynamic_cast<SuperLPG*>(this->module);

        /// a structure for holding changes to the model items
        struct FilterModeItem : MenuItem {
            /// the module to update
            SuperLPG* module;

            /// the currently selected envelope mode
            int filterMode;

            /// Response to an action update to this item
            void onAction(const event::Action& e) override {
                module->filterMode = filterMode;
            }
        };

        // add the envelope mode selection item to the menu
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Filter Mode"));
        for (int i = 0; i < SonyS_DSP::GaussianInterpolationFilter::FILTER_MODES; i++) {
            // get the label for the filter mode. the reciprocal of the
            // filterMode is what is set in the module, so use the reciprocal
            // here to get the correct label from the class function
            auto label = SonyS_DSP::GaussianInterpolationFilter::getFilterLabel(3 - i);
            auto item = createMenuItem<FilterModeItem>(label, CHECKMARK(module->filterMode == i));
            item->module = module;
            item->filterMode = i;
            menu->addChild(item);
        }
    }
};

/// the global instance of the model
rack::Model *modelSuperLPG = createModel<SuperLPG, SuperLPGWidget>("SuperLPG");
