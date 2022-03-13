// An echo effect module based on the S-SMP chip from Nintendo SNES.
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
#include "dsp/math.hpp"
#include "dsp/trigger.hpp"
#include "dsp/sony_s_dsp/echo.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// @brief An echo effect module based on the S-SMP chip from Nintendo SNES.
struct SuperEcho : Module {
 private:
    /// the Sony S-DSP echo effect emulator
    SonyS_DSP::Echo apu[PORT_MAX_CHANNELS];

    /// a VU meter for measuring the input audio levels
    rack::dsp::VuMeter2 inputVUMeter[SonyS_DSP::StereoSample::CHANNELS];
    /// a VU meter for measuring the output audio levels
    rack::dsp::VuMeter2 outputVUMeter[SonyS_DSP::StereoSample::CHANNELS];
    /// a light divider for updating the LEDs every 512 processing steps
    Trigger::Divider lightDivider;

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        PARAM_DELAY,
        PARAM_FEEDBACK,
        ENUMS(PARAM_MIX, SonyS_DSP::StereoSample::CHANNELS),
        ENUMS(PARAM_FIR_COEFFICIENT, SonyS_DSP::Echo::FIR_COEFFICIENT_COUNT),
        ENUMS(PARAM_FIR_COEFFICIENT_ATT, SonyS_DSP::Echo::FIR_COEFFICIENT_COUNT),
        ENUMS(PARAM_GAIN, SonyS_DSP::StereoSample::CHANNELS),
        PARAM_BYPASS,
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_AUDIO, SonyS_DSP::StereoSample::CHANNELS),
        INPUT_DELAY,
        INPUT_FEEDBACK,
        ENUMS(INPUT_MIX, SonyS_DSP::StereoSample::CHANNELS),
        ENUMS(INPUT_FIR_COEFFICIENT, SonyS_DSP::Echo::FIR_COEFFICIENT_COUNT),
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_AUDIO, SonyS_DSP::StereoSample::CHANNELS),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(LIGHT_VU_INPUT,        3 * SonyS_DSP::StereoSample::CHANNELS),
        ENUMS(LIGHT_VU_OUTPUT,       3 * SonyS_DSP::StereoSample::CHANNELS),
        ENUMS(LIGHT_FIR_COEFFICIENT, 3 * SonyS_DSP::Echo::FIR_COEFFICIENT_COUNT),
        NUM_LIGHTS
    };

    /// @brief Initialize a new S-DSP Chip module.
    SuperEcho() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned coeff = 0; coeff < SonyS_DSP::Echo::FIR_COEFFICIENT_COUNT; coeff++) {
            configParam(PARAM_FIR_COEFFICIENT     + coeff, -128, 127, apu[0].getFIR(coeff), "FIR Coefficient " + std::to_string(coeff + 1));
            configParam(PARAM_FIR_COEFFICIENT_ATT + coeff, -1.f, 1.f, 0, "FIR Coefficient " + std::to_string(coeff + 1) + " CV Attenuverter");
            configInput(INPUT_FIR_COEFFICIENT + coeff, "FIR Coefficient " + std::to_string(coeff + 1));
        }
        configParam(PARAM_DELAY, 0, SonyS_DSP::Echo::DELAY_LEVELS, 0, "Echo Delay", " ms", 0, SonyS_DSP::Echo::MILLISECONDS_PER_DELAY_LEVEL);
        configParam(PARAM_FEEDBACK, -128, 127, 0, "Echo Feedback");
        configParam(PARAM_GAIN + 0, 0, Math::decibels2amplitude(6.f), 1, "Input Gain (Left)", " dB", -10, 20);
        configParam(PARAM_GAIN + 1, 0, Math::decibels2amplitude(6.f), 1, "Input Gain (Right)", " dB", -10, 20);
        configParam(PARAM_MIX + 0, -128, 127, 0, "Echo Mix (Left)");
        configParam(PARAM_MIX + 1, -128, 127, 0, "Echo Mix (Right)");

        getParamQuantity(PARAM_DELAY)->snapEnabled = true;
        getParamQuantity(PARAM_FEEDBACK)->snapEnabled = true;
        getParamQuantity(PARAM_MIX + 0)->snapEnabled = true;
        getParamQuantity(PARAM_MIX + 1)->snapEnabled = true;

        configParam<BooleanParamQuantity>(PARAM_BYPASS, 0, 1, 0, "Bypass");
        configInput(INPUT_DELAY, "Delay");
        configInput(INPUT_FEEDBACK, "Feedback");
        configInput(INPUT_AUDIO + 0, "Audio (Left)");
        configInput(INPUT_AUDIO + 1, "Audio (Right)");
        configInput(INPUT_MIX + 0, "Mix (Left)");
        configInput(INPUT_MIX + 1, "Mix (Right)");
        configOutput(OUTPUT_AUDIO + 0, "Audio (Left)");
        configOutput(OUTPUT_AUDIO + 1, "Audio (Right)");
        lightDivider.setDivision(512);
    }

 protected:
    /// @brief Return the value of the delay parameter from the panel.
    ///
    /// @param channel the polyphonic channel to get the delay parameter for
    /// @returns the 8-bit delay parameter after applying CV modulations
    ///
    inline uint8_t getDelay(const unsigned& channel) {
        const float param = params[PARAM_DELAY].getValue();
        const float cv = Math::Eurorack::fromDC(inputs[INPUT_DELAY].getVoltage(channel));
        const float mod = SonyS_DSP::Echo::DELAY_LEVELS * cv;
        static constexpr float MAX = SonyS_DSP::Echo::DELAY_LEVELS;
        return Math::clip(param + mod, 0.f, MAX);
    }

    /// @brief Return the value of the feedback parameter from the panel.
    ///
    /// @param channel the feedback channel to get the delay parameter for
    /// @returns the 8-bit feedback parameter after applying CV modulations
    ///
    inline int8_t getFeedback(const unsigned& channel) {
        const float param = params[PARAM_FEEDBACK].getValue();
        const float cv = Math::Eurorack::fromDC(inputs[INPUT_FEEDBACK].getVoltage(channel));
        const float mod = std::numeric_limits<int8_t>::max() * cv;
        static constexpr float MIN = std::numeric_limits<int8_t>::min();
        static constexpr float MAX = std::numeric_limits<int8_t>::max();
        return Math::clip(param + mod, MIN, MAX);
    }

    /// @brief Return the value of the mix parameter from the panel.
    ///
    /// @param channel the polyphonic channel to get the mix parameter for
    /// @param lane the stereo delay lane to get the mix level parameter for
    /// @returns the 8-bit mix parameter after applying CV modulations
    ///
    inline int8_t getMix(const unsigned& channel, const unsigned& lane) {
        const float param = params[PARAM_MIX + lane].getValue();
        const float normal = lane ? inputs[INPUT_MIX + lane - 1].getVoltage(channel) : 0.f;
        const float voltage = inputs[INPUT_MIX + lane].getNormalVoltage(normal, channel);
        const float mod = std::numeric_limits<int8_t>::max() * Math::Eurorack::fromDC(voltage);
        static constexpr float MIN = std::numeric_limits<int8_t>::min();
        static constexpr float MAX = std::numeric_limits<int8_t>::max();
        return Math::clip(param + mod, MIN, MAX);
    }

    /// @brief Return the value of the FIR filter parameter from the panel.
    ///
    /// @param channel the polyphonic channel to get the FIR coefficient for
    /// @param index the index of the FIR filter coefficient to get
    /// @returns the 8-bit FIR filter parameter for coefficient at given index
    ///
    inline int8_t getFIRCoefficient(const unsigned& channel, const unsigned& index) {
        const float input = normalChain(&inputs[INPUT_FIR_COEFFICIENT], index, channel, 0.f);
        const float att = params[PARAM_FIR_COEFFICIENT_ATT + index].getValue();
        const float mod = att * std::numeric_limits<int8_t>::max() * Math::Eurorack::fromDC(input);
        const float param = params[PARAM_FIR_COEFFICIENT + index].getValue();
        static constexpr float MIN = std::numeric_limits<int8_t>::min();
        static constexpr float MAX = std::numeric_limits<int8_t>::max();
        return Math::clip(param + mod, MIN, MAX);
    }

    /// @brief Return the value of the stereo input from the panel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to get the audio input for
    /// @param lane the stereo delay lane to get the input voltage for
    /// @returns the 8-bit stereo input for the given lane
    ///
    inline int16_t getInput(const ProcessArgs& args, const unsigned& channel, const unsigned& lane) {
        const float normal = lane ? inputs[INPUT_AUDIO + lane - 1].getVoltage(channel) : 0.f;
        const float gain = params[PARAM_GAIN + lane].getValue();
        const float input = gain * Math::Eurorack::fromAC(inputs[INPUT_AUDIO + lane].getNormalVoltage(normal, channel));
        inputVUMeter[lane].process(args.sampleTime, input);
        static constexpr float MAX = std::numeric_limits<int16_t>::max();
        return MAX * Math::clip(input, -1.f, 1.f);
    }

    /// @brief Return the clean value of the stereo input from the panel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to get the audio input for
    /// @param lane the stereo delay lane to get the input voltage for
    /// @returns the 8-bit stereo input for the given lane
    ///
    inline void bypassChannel(const ProcessArgs& args, const unsigned& channel, const unsigned& lane) {
        // update the FIR Coefficients (so the lights still respond in bypass)
        for (unsigned i = 0; i < SonyS_DSP::Echo::FIR_COEFFICIENT_COUNT; i++)
            apu[channel].setFIR(i, getFIRCoefficient(channel, i));
        // get the normal voltage from the left/right pair
        const float gain = params[PARAM_GAIN + lane].getValue();
        const float normal = lane ? inputs[INPUT_AUDIO + lane - 1].getVoltage(channel) : 0.f;
        const float voltage = gain * inputs[INPUT_AUDIO + lane].getNormalVoltage(normal, channel);
        // process the input on the VU meter
        inputVUMeter[lane].process(args.sampleTime, Math::Eurorack::fromAC(voltage));
        outputVUMeter[lane].process(args.sampleTime, Math::Eurorack::fromAC(voltage));
        outputs[OUTPUT_AUDIO + lane].setVoltage(voltage, channel);
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processChannel(const ProcessArgs& args, const unsigned& channel) {
        // update the FIR Coefficients
        for (unsigned i = 0; i < SonyS_DSP::Echo::FIR_COEFFICIENT_COUNT; i++)
            apu[channel].setFIR(i, getFIRCoefficient(channel, i));
        // update the delay parameters
        apu[channel].setDelay(getDelay(channel));
        apu[channel].setFeedback(getFeedback(channel));
        apu[channel].setMixLeft(getMix(channel, SonyS_DSP::StereoSample::LEFT));
        apu[channel].setMixRight(getMix(channel, SonyS_DSP::StereoSample::RIGHT));
        // run a stereo sample through the echo buffer + filter
        auto output = apu[channel].run(
            getInput(args, channel, SonyS_DSP::StereoSample::LEFT),
            getInput(args, channel, SonyS_DSP::StereoSample::RIGHT)
        );
        // write the stereo output to the ports
        for (unsigned i = 0; i < SonyS_DSP::StereoSample::CHANNELS; i++) {
            // get the sample in [0, 1] (clipped by the finite precision of the
            // emulation)
            const float sample = output.samples[i] / static_cast<float>(std::numeric_limits<int16_t>::max());
            // approximate the VU meter by scaling the sample slightly
            outputVUMeter[i].process(args.sampleTime, 1.2 * sample);
            // set the output
            outputs[OUTPUT_AUDIO + i].setVoltage(5.f * sample, channel);
        }
    }

    /// @brief Process the inputs and outputs to/from the module.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    inline void process(const ProcessArgs& args) final {
        // get the number of polyphonic channels (defaults to 1 for monophonic).
        // also set the channels on the output ports based on the number of
        // channels
        unsigned channels = 1;
        for (unsigned port = 0; port < NUM_INPUTS; port++)
            channels = std::max(inputs[port].getChannels(), static_cast<int>(channels));
        // set the number of polyphony channels for output ports
        for (unsigned port = 0; port < NUM_OUTPUTS; port++)
            outputs[port].setChannels(channels);
        if (params[PARAM_BYPASS].getValue()) {  // bypass the chip emulator
            for (unsigned channel = 0; channel < channels; channel++) {
                for (unsigned i = 0; i < SonyS_DSP::StereoSample::CHANNELS; i++) {
                    bypassChannel(args, channel, i);
                    apu[channel].run(0, 0);
                }
            }
        } else {  // process audio samples on the chip engine.
            for (unsigned channel = 0; channel < channels; channel++)
                processChannel(args, channel);
        }
        if (lightDivider.process()) {  // update the LEDs on the panel
            setVULight3(inputVUMeter[0], &lights[LIGHT_VU_INPUT]);
            setVULight3(inputVUMeter[1], &lights[LIGHT_VU_INPUT + 3]);
            setVULight3(outputVUMeter[0], &lights[LIGHT_VU_OUTPUT]);
            setVULight3(outputVUMeter[1], &lights[LIGHT_VU_OUTPUT + 3]);
            // CV indicators for the FIR filter
            const float sample_time = lightDivider.getDivision() * args.sampleTime;
            for (unsigned param = 0; param < SonyS_DSP::Echo::FIR_COEFFICIENT_COUNT; param++) {
                // get the scaled CV (it's already normalled)
                float value = 0.f;
                if (channels > 1) {  // polyphonic (average)
                    for (unsigned c = 0; c < channels; c++)
                        value += inputs[INPUT_FIR_COEFFICIENT + param].getVoltage(c);
                    value = params[PARAM_FIR_COEFFICIENT_ATT + param].getValue() * value / channels;
                } else {  // monophonic
                    value = inputs[INPUT_FIR_COEFFICIENT + param].getVoltage() * params[PARAM_FIR_COEFFICIENT_ATT + param].getValue();
                }
                if (value > 0) {  // green for positive voltage
                    lights[LIGHT_FIR_COEFFICIENT + 3 * param + 0].setSmoothBrightness(0, sample_time);
                    lights[LIGHT_FIR_COEFFICIENT + 3 * param + 1].setSmoothBrightness(Math::Eurorack::fromDC(value), sample_time);
                    lights[LIGHT_FIR_COEFFICIENT + 3 * param + 2].setSmoothBrightness(0, sample_time);
                } else {  // red for negative voltage
                    lights[LIGHT_FIR_COEFFICIENT + 3 * param + 0].setSmoothBrightness(-Math::Eurorack::fromDC(value), sample_time);
                    lights[LIGHT_FIR_COEFFICIENT + 3 * param + 1].setSmoothBrightness(0, sample_time);
                    lights[LIGHT_FIR_COEFFICIENT + 3 * param + 2].setSmoothBrightness(0, sample_time);
                }
            }
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for SPC700.
struct SuperEchoWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit SuperEchoWidget(SuperEcho *module) {
        setModule(module);
        static constexpr auto panel = "res/SuperEcho.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // Panel Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // bypass switch
        addParam(createParam<CKSS>(Vec(15, 25), module, SuperEcho::PARAM_BYPASS));
        for (unsigned i = 0; i < SonyS_DSP::StereoSample::CHANNELS; i++) {
            // Echo Parameter (0 = delay, 1 = Feedback)
            addParam(createParam<Trimpot>(Vec(13 + 39 * i, 77), module, SuperEcho::PARAM_DELAY + i));
            addInput(createInput<PJ301MPort>(Vec(10 + 39 * i, 112), module, SuperEcho::INPUT_DELAY + i));
            // Echo Mix
            addParam(createParam<Trimpot>(Vec(13 + 39 * i, 163), module, SuperEcho::PARAM_MIX + i));
            addInput(createInput<PJ301MPort>(Vec(10 + 39 * i, 198), module, SuperEcho::INPUT_MIX + i));
            // Stereo Input Ports
            addChild(createLight<MediumLight<RedGreenBlueLight>>(Vec(3 + 39 * i, 236), module, SuperEcho::LIGHT_VU_INPUT + 3 * i));
            addInput(createInput<PJ301MPort>(Vec(10 + 39 * i, 243), module, SuperEcho::INPUT_AUDIO + i));
            addParam(createParam<Trimpot>(Vec(13 + 39 * i, 278), module, SuperEcho::PARAM_GAIN + i));
            // Stereo Output Ports
            addChild(createLight<MediumLight<RedGreenBlueLight>>(Vec(3 + 39 * i, 311), module, SuperEcho::LIGHT_VU_OUTPUT + 3 * i));
            addOutput(createOutput<PJ301MPort>(Vec(10 + 39 * i, 323), module, SuperEcho::OUTPUT_AUDIO + i));
        }
        // FIR Coefficients
        for (unsigned i = 0; i < SonyS_DSP::Echo::FIR_COEFFICIENT_COUNT; i++) {
            addInput(createInput<PJ301MPort>(Vec(84, 28 + i * 43), module, SuperEcho::INPUT_FIR_COEFFICIENT + i));
            addParam(createParam<Trimpot>(Vec(117, 30 + i * 43), module, SuperEcho::PARAM_FIR_COEFFICIENT_ATT + i));
            auto param = createLightParam<LEDLightSliderHorizontal<RedGreenBlueLight>>(Vec(147, 29 + i * 43), module, SuperEcho::PARAM_FIR_COEFFICIENT + i, SuperEcho::LIGHT_FIR_COEFFICIENT + 3 * i);
            param->snap = true;
            addParam(param);
        }
    }
};

/// the global instance of the model
rack::Model *modelSuperEcho = createModel<SuperEcho, SuperEchoWidget>("SuperEcho");
