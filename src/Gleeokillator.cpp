// A SunSoft FME7 chip emulator module.
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
#include "engine/chip_module.hpp"
#include "dsp/sunsoft_fme7.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// @brief A SunSoft FME7 chip emulator module.
struct Gleeokillator : ChipModule<SunSoftFME7> {
 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ, SunSoftFME7::OSC_COUNT),
        ENUMS(PARAM_FM, SunSoftFME7::OSC_COUNT),
        ENUMS(PARAM_LEVEL, SunSoftFME7::OSC_COUNT),
        NUM_PARAMS
    };
    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT, SunSoftFME7::OSC_COUNT),
        ENUMS(INPUT_FM, SunSoftFME7::OSC_COUNT),
        ENUMS(INPUT_LEVEL, SunSoftFME7::OSC_COUNT),
        NUM_INPUTS
    };
    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_OSCILLATOR, SunSoftFME7::OSC_COUNT),
        NUM_OUTPUTS
    };
    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(LIGHTS_LEVEL, 3 * SunSoftFME7::OSC_COUNT),
        NUM_LIGHTS
    };

    /// @brief Initialize a new FME7 Chip module.
    Gleeokillator() {
        normal_outputs = true;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        // set the output buffer for each individual voice
        for (unsigned oscillator = 0; oscillator < SunSoftFME7::OSC_COUNT; oscillator++) {
            // get the oscillator name starting with ACII code 65 (A)
            auto name = "Tone " + std::string(1, static_cast<char>(65 + oscillator));
            configParam(PARAM_FREQ  + oscillator,  -4.5f, 4.5f, 0.f,  name + " Frequency", " Hz", 2,   dsp::FREQ_C4);
            configParam(INPUT_FM    + oscillator,  -1.f,  1.f,  0.f,  name + " FM");
            configParam(PARAM_LEVEL + oscillator,   0,   15,    7,    name + " Level");
        }
    }

 protected:
    /// @brief Return the frequency for the given oscillator.
    ///
    /// @param oscillator the index of the oscillator to get the frequency of
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 12-bit frequency in a 16-bit container
    ///
    inline uint16_t getFrequency(unsigned oscillator, unsigned channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ12BIT_MIN = 4;
        // the maximal value for the frequency register
        static constexpr float FREQ12BIT_MAX = 4067;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + oscillator].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 0V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normalPitch = oscillator ? inputs[INPUT_VOCT + oscillator - 1].getVoltage(channel) : 0.f;
        const auto pitchCV = inputs[INPUT_VOCT + oscillator].getNormalVoltage(normalPitch, channel);
        inputs[INPUT_VOCT + oscillator].setVoltage(pitchCV, channel);
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
        // and clamp within [0, 20000] Hz
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = buffers[channel][oscillator].get_clock_rate() / (CLOCK_DIVISION * freq);
        return rack::clamp(freq, FREQ12BIT_MIN, FREQ12BIT_MAX);
    }

    /// @brief Return the volume parameter for the given oscillator.
    ///
    /// @param oscillator the oscillator to get the volume parameter for
    /// @param channel the polyphonic channel to return the volume for
    /// @returns the volume parameter for the given oscillator. This includes
    /// the value of the knob and any CV modulation.
    ///
    inline uint8_t getVolume(unsigned oscillator, unsigned channel) {
        // the minimal value for the volume width register
        static constexpr float MIN = 0;
        // the maximal value for the volume width register
        static constexpr float MAX = 15;
        // get the level from the parameter knob
        auto level = params[PARAM_LEVEL + oscillator].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 10V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normal = oscillator ? inputs[INPUT_LEVEL + oscillator - 1].getVoltage(channel) : 10.f;
        const auto mod = inputs[INPUT_LEVEL + oscillator].getNormalVoltage(normal, channel);
        inputs[INPUT_LEVEL + oscillator].setVoltage(mod, channel);
        // apply the control mod to the level. Normal to a constant
        // 10V source instead of checking if the cable is connected
        level = roundf(level * mod / 10.f);
        // get the 8-bit attenuation by inverting the level and clipping
        // to the legal bounds of the parameter
        return rack::clamp(level, MIN, MAX);
    }


    /// @brief Process the audio rate inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the audio inputs to
    ///
    inline void processAudio(const ProcessArgs &args, unsigned channel) final {
        for (unsigned oscillator = 0; oscillator < SunSoftFME7::OSC_COUNT; oscillator++) {
            // frequency. there are two frequency registers per voice.
            // shift the index left 1 instead of multiplying by 2
            auto freq = getFrequency(oscillator, channel);
            uint8_t lo =  freq & 0b11111111;
            apu[channel].write(SunSoftFME7::PULSE_A_LO + (oscillator << 1), lo);
            uint8_t hi = (freq & 0b0000111100000000) >> 8;
            apu[channel].write(SunSoftFME7::PULSE_A_HI + (oscillator << 1), hi);
        }
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processCV(const ProcessArgs &args, unsigned channel) final {
        for (unsigned oscillator = 0; oscillator < SunSoftFME7::OSC_COUNT; oscillator++) {
            // level
            apu[channel].write(SunSoftFME7::PULSE_A_ENV + oscillator, 0x10 | getVolume(oscillator, channel));
        }
    }

    /// @brief Process the lights on the module.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channels the number of active polyphonic channels
    ///
    inline void processLights(const ProcessArgs &args, unsigned channels) final {
        for (unsigned voice = 0; voice < SunSoftFME7::OSC_COUNT; voice++) {
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
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for Gleeokillator.
struct GleeokillatorWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit GleeokillatorWidget(Gleeokillator *module) {
        setModule(module);
        static constexpr auto panel = "res/Gleeokillator.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        // addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        // addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        for (unsigned i = 0; i < SunSoftFME7::OSC_COUNT; i++) {
            // Frequency
            addParam(createParam<Trimpot>(     Vec(15 + 35 * i, 45),  module, Gleeokillator::PARAM_FREQ        + i));
            addInput(createInput<PJ301MPort>(  Vec(13 + 35 * i, 85),  module, Gleeokillator::INPUT_VOCT        + i));
            // FM
            addInput(createInput<PJ301MPort>(  Vec(13 + 35 * i, 129), module, Gleeokillator::INPUT_FM          + i));
            addParam(createParam<Trimpot>(     Vec(15 + 35 * i, 173), module, Gleeokillator::PARAM_FM          + i));
            // Level
            addParam(createSnapParam<Trimpot>( Vec(15 + 35 * i, 221), module, Gleeokillator::PARAM_LEVEL       + i));
            addInput(createInput<PJ301MPort>(  Vec(13 + 35 * i, 263), module, Gleeokillator::INPUT_LEVEL       + i));
            addChild(createLight<MediumLight<RedGreenBlueLight>>(Vec(17 + 35 * i, 297), module, Gleeokillator::LIGHTS_LEVEL + 3 * i));
            // Output
            addOutput(createOutput<PJ301MPort>(Vec(13 + 35 * i, 324), module, Gleeokillator::OUTPUT_OSCILLATOR + i));
        }
    }
};

/// the global instance of the model
Model *modelGleeokillator = createModel<Gleeokillator, GleeokillatorWidget>("FME7");
