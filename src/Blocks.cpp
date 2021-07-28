// A Linear Feedback Shift Register (LFSR) module.
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
#include "dsp/mi_edges/wavetable.hpp"
#include "dsp/trigger/threshold.hpp"
#include "dsp/trigger/divider.hpp"
#include "dsp/math.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// @brief The digital oscillator from the Mutable Instruments Edges module.
struct Blocks : rack::Module {
    static constexpr std::size_t NUM_VOICES = 4;

    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ,  NUM_VOICES),
        ENUMS(PARAM_FM,    NUM_VOICES),
        ENUMS(PARAM_LEVEL, NUM_VOICES),
        ENUMS(PARAM_SHAPE, NUM_VOICES),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_FREQ,   NUM_VOICES),
        ENUMS(INPUT_FM,     NUM_VOICES),
        ENUMS(INPUT_LEVEL,  NUM_VOICES),
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_AUDIO, NUM_VOICES),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(LIGHTS_LEVEL, 3 * NUM_VOICES),
        ENUMS(LIGHTS_SHAPE, 3 * NUM_VOICES),
        NUM_LIGHTS
    };

    /// the digital oscillator instance (1 for each channel)
    Oscillator::MutableIntstrumentsEdges::DigitalOscillator oscillator[PORT_MAX_CHANNELS][NUM_VOICES];

    /// whether to normal outputs into a mix
    bool normal_outputs = true;

    /// whether to hard clips outputs in the mix
    bool hard_clip = true;

    /// a VU meter for measuring the output audio level from the emulator
    rack::dsp::VuMeter2 vuMeter[NUM_VOICES];

    /// a trigger for handling presses to the change mode button
    Trigger::Threshold triggers[NUM_VOICES];

    /// a clock divider for running CV acquisition slower than audio rate
    Trigger::Divider cvDivider;

    /// a clock divider for running LED updates slower than audio rate
    Trigger::Divider lightDivider;

    /// Initialize a new Blocks oscillator module.
    Blocks() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned i = 0; i < NUM_VOICES; i++) {
            std::string name = "Voice " + std::to_string(i + 1) + " ";
            configParam(PARAM_FREQ + i, -2.5f, 2.5f, 0.f, name + "Frequency", " Hz", 2, dsp::FREQ_C4);
            configParam(PARAM_FM + i, -1.f, 1.f, 0.f, name + "FM");
            configParam(PARAM_LEVEL + i, 0, 255, 255, name + "Level");
            configParam<TriggerParamQuantity>(PARAM_SHAPE + i, 0, 1, 0, name + "Shape");
        }
        // set the division of the CV and LED frame dividers
        cvDivider.setDivision(16);
        lightDivider.setDivision(512);
    }

    /// @brief Respond to the module being randomized by the engine.
    inline void onRandomize() override {
        for (unsigned voice = 0; voice < NUM_VOICES; voice++) {
            const auto shape = static_cast<Oscillator::MutableIntstrumentsEdges::DigitalOscillator::Shape>(random::u32() % 6);
            for (unsigned channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
                oscillator[channel][voice].setShape(shape);
            }
        }
    }

    /// @brief Respond to the module being reset by the engine.
    void onReset() override {
        // reset the CV and light divider clocks
        cvDivider.reset();
        lightDivider.reset();
        // reset the audio processing unit for all poly channels
        for (unsigned channel = 0; channel < PORT_MAX_CHANNELS; channel++)
            for (unsigned voice = 0; voice < NUM_VOICES; voice++)
                oscillator[channel][voice].reset();
    }

    /// @brief Return a JSON representation of this module's state
    ///
    /// @returns a new JSON object with this object's serialized state data
    ///
    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        for (unsigned voice = 0; voice < NUM_VOICES; voice++) {
            const auto key = "shape" + std::to_string(voice + 1);
            const auto value = static_cast<int>(oscillator[0][voice].getShape());
            json_object_set_new(rootJ, key.c_str(), json_integer(value));
        }
        return rootJ;
    }

    /// @brief Return the object to the given serialized state.
    ///
    /// @returns a JSON object with object serialized state data to restore
    ///
    void dataFromJson(json_t* rootJ) override {
        for (unsigned voice = 0; voice < NUM_VOICES; voice++) {
            const auto key = "shape" + std::to_string(voice + 1);
            json_t* shapeObject = json_object_get(rootJ, key.c_str());
            if (shapeObject) {
                for (unsigned channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
                    auto shape = static_cast<Oscillator::MutableIntstrumentsEdges::DigitalOscillator::Shape>(json_integer_value(shapeObject));
                    oscillator[channel][voice].setShape(shape);
                }
            }
        }
    }

    /// @brief Get the frequency from the panel controls.
    ///
    /// @param oscillator the oscillator to return the frequency for
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the floating frequency value from the panel
    ///
    float getFrequency(const unsigned& oscillator, const unsigned& channel) {
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + oscillator].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 0V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normalPitch = oscillator ? inputs[INPUT_FREQ + oscillator - 1].getVoltage(channel) : 0.f;
        const auto pitchCV = inputs[INPUT_FREQ + oscillator].getNormalVoltage(normalPitch, channel);
        inputs[INPUT_FREQ + oscillator].setVoltage(pitchCV, channel);
        pitch += pitchCV;
        // get the attenuverter parameter value
        const auto att = params[PARAM_FM + oscillator].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 5V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normalMod = oscillator ? inputs[INPUT_FM + oscillator - 1].getVoltage(channel) : 5.f;
        const auto mod = inputs[INPUT_FM + oscillator].getNormalVoltage(normalMod, channel);
        inputs[INPUT_FM + oscillator].setVoltage(mod, channel);
        pitch += att * mod / 5.f;
        // convert the pitch to frequency based on standard exponential scale
        return Math::clip(rack::dsp::FREQ_C4 * powf(2.0, pitch), 0.0f, 20000.0f);
    }

    /// @brief Return the volume level from the panel controls.
    ///
    /// @param oscillator the oscillator to return the volume level of
    /// @param channel the polyphony channel of the given oscillator
    /// @returns the volume level of the given oscillator
    ///
    inline float getVolume(const unsigned& oscillator, const unsigned& channel) {
        // get the level from the parameter knob
        auto level = params[PARAM_LEVEL + oscillator].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 10V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normal = oscillator ? inputs[INPUT_LEVEL + oscillator - 1].getVoltage(channel) : 10.f;
        const auto voltage = inputs[INPUT_LEVEL + oscillator].getNormalVoltage(normal, channel);
        inputs[INPUT_LEVEL + oscillator].setVoltage(voltage, channel);
        // apply the control voltage to the level. Normal to a constant
        // 10V source instead of checking if the cable is connected
        level = roundf(level * Math::Eurorack::fromDC(voltage));
        // get the 8-bit attenuation by inverting the level and clipping
        // to the legal bounds of the parameter
        return Math::clip(level / 255.f, 0.f, 1.f);
    }

    /// @brief Process the lights on the module.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channels the number of active polyphonic channels
    ///
    inline void processLights(const ProcessArgs& args, const unsigned& channels) {
        for (unsigned voice = 0; voice < NUM_VOICES; voice++) {
            // get the global brightness scale from -12 to 3
            auto brightness = vuMeter[voice].getBrightness(-12, 3);
            // set the red light based on total brightness and
            // brightness from 0dB to 3dB
            lights[LIGHTS_LEVEL + voice * 3 + 0].setBrightness(brightness * vuMeter[voice].getBrightness(0, 3));
            // set the red light based on inverted total brightness and
            // brightness from -12dB to 0dB
            lights[LIGHTS_LEVEL + voice * 3 + 1].setBrightness((1 - brightness) * vuMeter[voice].getBrightness(-12, 0));
            // set the blue light to off
            lights[LIGHTS_LEVEL + voice * 3 + 2].setBrightness(0);
            // set the envelope mode light in RGB order
            const auto shape = static_cast<int>(oscillator[0][voice].getShape()) + 1;
            auto deltaTime = args.sampleTime * lightDivider.getDivision();
            bool red = shape & 0x4;
            lights[LIGHTS_SHAPE + 3 * voice + 0].setSmoothBrightness(red, deltaTime);
            bool green = shape & 0x2;
            lights[LIGHTS_SHAPE + 3 * voice + 1].setSmoothBrightness(green, deltaTime);
            bool blue = shape & 0x1;
            lights[LIGHTS_SHAPE + 3 * voice + 2].setSmoothBrightness(blue, deltaTime);
        }
    }

    /// @brief Process a sample.
    void process(const ProcessArgs &args) override {
        // get the number of polyphonic channels (defaults to 1 for monophonic).
        // also set the channels on the output ports based on the number of
        // channels
        unsigned channels = 1;
        for (unsigned port = 0; port < inputs.size(); port++)
            channels = std::max(inputs[port].getChannels(), static_cast<int>(channels));
        // set the number of polyphony channels for output ports
        for (unsigned port = 0; port < outputs.size(); port++)
            outputs[port].setChannels(channels);
        // Set the shape parameter for each voice
        for (unsigned voice = 0; voice < NUM_VOICES; voice++) {
            if (triggers[voice].process(params[PARAM_SHAPE + voice].getValue())) {
                for (unsigned channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
                    oscillator[channel][voice].cycleShape();
                }
            }
        }
        // process audio samples on the DSP engines.
        for (unsigned channel = 0; channel < channels; channel++) {
            for (unsigned voice = 0; voice < NUM_VOICES; voice++) {
                oscillator[channel][voice].setFrequency(getFrequency(voice, channel));
                oscillator[channel][voice].process(args.sampleTime);
                const auto gain = getVolume(voice, channel);
                auto output = gain * oscillator[channel][voice].getValue();
                if (normal_outputs) {  // mix outputs from previous voices
                    const bool shouldNormal = voice && !outputs[voice - 1].isConnected();
                    const float lastOutput = shouldNormal ? outputs[voice - 1].getVoltage(channel) : 0.f;
                    output += Math::Eurorack::fromAC(lastOutput);
                }
                vuMeter[voice].process(args.sampleTime / channels, output);
                if (hard_clip) output = Math::clip(output, -1.f, 1.f);
                outputs[OUTPUT_AUDIO + voice].setVoltage(Math::Eurorack::toAC(output), channel);
            }
        }
        processLights(args, channels);
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The widget structure that lays out the panel of the module and the UI menus.
struct BlocksWidget : rack::ModuleWidget {
    explicit BlocksWidget(Blocks *module) {
        setModule(module);
        static const auto panel = "res/Blocks.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // parameter knobs, inputs, outputs for each voice
        for (unsigned i = 0; i < Blocks::NUM_VOICES; i++) {
            addParam(createParam<Trimpot>(Vec(12 + 35 * i, 32), module, Blocks::PARAM_FREQ + i));
            addInput(createInput<PJ301MPort>(Vec(10 + 35 * i, 71), module, Blocks::INPUT_FREQ + i));
            addInput(createInput<PJ301MPort>(Vec(10 + 35 * i, 99), module, Blocks::INPUT_FM + i));
            addParam(createParam<Trimpot>(Vec(12 + 35 * i, 144), module, Blocks::PARAM_FM + i));
            addParam(createSnapParam<Trimpot>(Vec(12 + 35 * i, 170), module, Blocks::PARAM_LEVEL + i));
            addInput(createInput<PJ301MPort>(Vec(10 + 35 * i, 210), module, Blocks::INPUT_LEVEL + i));
            addChild(createLight<LargeLight<RedGreenBlueLight>>(Vec(14 + 35 * i, 246), module, Blocks::LIGHTS_SHAPE + 3 * i));
            addParam(createParam<TL1105>(Vec(14 + 35 * i, 282), module, Blocks::PARAM_SHAPE + i));
            addChild(createLight<SmallLight<RedGreenBlueLight>>(Vec(29 + 35 * i, 319), module, Blocks::LIGHTS_LEVEL + 3 * i));
            addOutput(createOutput<PJ301MPort>(Vec(10 + 35 * i, 324), module, Blocks::OUTPUT_AUDIO + i));
        }
    }

    void appendContextMenu(Menu* menu) override {
        // get a pointer to the module
        Blocks* const module = dynamic_cast<Blocks*>(this->module);

        // string representations of the envelope modes
        static const std::string LABELS[6] = {
            "Sine",
            "Triangle",
            "NES Triangle",
            "Sample+Hold",
            "LFSR Long",
            "LFSR Short",
        };

        /// @brief a structure for holding changes to the oscillator shape.
        struct ShapeValueItem : MenuItem {
            /// the module to update
            Blocks* module;
            /// the voice to update the shape of
            unsigned voice = 0;
            /// the selected shape for this menu item
            Oscillator::MutableIntstrumentsEdges::DigitalOscillator::Shape shape;

            /// Respond to an action update to this item
            void onAction(const event::Action& e) override {
                for (unsigned channel = 0; channel < PORT_MAX_CHANNELS; channel ++)
                    module->oscillator[channel][voice].setShape(shape);
            }
        };

        /// @brief An individual oscillator shape menu item.
        struct ShapeItem : MenuItem {
            /// the module to update
            Blocks* module;
            /// the voice to update the shape of
            unsigned voice = 0;

            /// @brief Create a child menu with selections for oscillator shapes.
            Menu* createChildMenu() override {
                Menu* menu = new Menu;
                for (int i = 0; i < 6; i++) {
                    auto shape = static_cast<Oscillator::MutableIntstrumentsEdges::DigitalOscillator::Shape>(i);
                    ShapeValueItem* item = new ShapeValueItem;
                    item->text = LABELS[i];
                    item->rightText = CHECKMARK(module->oscillator[0][voice].getShape() == shape);
                    item->module = module;
                    item->voice = voice;
                    item->shape = shape;
                    menu->addChild(item);
                }
                return menu;
            }
        };

        // Iterate over each voice and add a menu item for the
        for (unsigned voice = 0; voice < Blocks::NUM_VOICES; voice++) {
            ShapeItem* shapeItem = new ShapeItem;
            shapeItem->text = "Oscillator " + std::to_string(voice + 1) + " Shape";
            shapeItem->rightText = RIGHT_ARROW;
            shapeItem->module = module;
            shapeItem->voice = voice;
            menu->addChild(shapeItem);
        }
    }
};

/// the global instance of the model
Model *modelBlocks = createModel<Blocks, BlocksWidget>("Blocks");
