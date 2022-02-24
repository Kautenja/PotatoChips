// A Atari POKEY Chip module.
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
#include "dsp/atari_pokey.hpp"
#include "engine/chip_module.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Atari POKEY chip emulator module.
struct PotKeys : ChipModule<AtariPOKEY> {
 private:
    /// triggers for handling inputs to the control ports
    Trigger::Threshold controlTriggers[PORT_MAX_CHANNELS][AtariPOKEY::CTL_FLAGS];

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ, AtariPOKEY::OSC_COUNT),
        ENUMS(PARAM_FM, AtariPOKEY::OSC_COUNT),
        ENUMS(PARAM_NOISE, AtariPOKEY::OSC_COUNT),
        ENUMS(PARAM_LEVEL, AtariPOKEY::OSC_COUNT),
        ENUMS(PARAM_CONTROL, AtariPOKEY::CTL_FLAGS),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT, AtariPOKEY::OSC_COUNT),
        ENUMS(INPUT_FM, AtariPOKEY::OSC_COUNT),
        ENUMS(INPUT_NOISE, AtariPOKEY::OSC_COUNT),
        ENUMS(INPUT_LEVEL, AtariPOKEY::OSC_COUNT),
        ENUMS(INPUT_CONTROL, AtariPOKEY::CTL_FLAGS),
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_OSCILLATOR, AtariPOKEY::OSC_COUNT),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(LIGHTS_LEVEL, 3 * AtariPOKEY::OSC_COUNT),
        NUM_LIGHTS
    };

    /// @brief Initialize a new POKEY Chip module.
    PotKeys() : ChipModule<AtariPOKEY>() {
        normal_outputs = true;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned i = 0; i < AtariPOKEY::OSC_COUNT; i++) {
            auto name = "Tone " + std::to_string(i + 1);
            configParam(PARAM_FREQ  + i, -2.5f, 2.5f, 0.f, name + " Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
            configParam(PARAM_FM    + i, -1.f,  1.f,  0.f, name + " FM");
            configParam(PARAM_NOISE + i,  0,    7,    7,   name + " Noise");
            configParam(PARAM_LEVEL + i,  0,   15,    7,   name + " Level");
            configInput(INPUT_VOCT + i, name + " V/Oct");
            configInput(INPUT_FM + i, name + " FM");
            configInput(INPUT_NOISE + i, name + " Noise");
            configInput(INPUT_LEVEL + i, name + " Level");
            configOutput(OUTPUT_OSCILLATOR + i, name + " Audio");
        }
        // control register controls
        configParam<BooleanParamQuantity>(PARAM_CONTROL + 0, 0, 1, 0, "Low Frequency");
        configParam<BooleanParamQuantity>(PARAM_CONTROL + 1, 0, 1, 0, "High-Pass Tone 2 from Tone 4");
        configParam<BooleanParamQuantity>(PARAM_CONTROL + 2, 0, 1, 0, "High-Pass Tone 1 from Tone 3");
        // configParam<BooleanParamQuantity>(PARAM_CONTROL + 3, 0, 1, 0, "16-bit 4 + 3");  // ignore 16-bit
        // configParam<BooleanParamQuantity>(PARAM_CONTROL + 4, 0, 1, 0, "16-bit 1 + 2");  // ignore 16-bit
        configParam<BooleanParamQuantity>(PARAM_CONTROL + 5, 0, 1, 0, "Tone 3 High Frequency");
        configParam<BooleanParamQuantity>(PARAM_CONTROL + 6, 0, 1, 0, "Tone 1 High Frequency");
        configParam<BooleanParamQuantity>(PARAM_CONTROL + 7, 0, 1, 0, "Linear Feedback Shift Register");

        configInput(PARAM_CONTROL + 0, "Low Frequency");
        configInput(PARAM_CONTROL + 1, "High-Pass Tone 2 from Tone 4");
        configInput(PARAM_CONTROL + 2, "High-Pass Tone 1 from Tone 3");
        // configInput(PARAM_CONTROL + 3, "16-bit 4 + 3");  // ignore 16-bit
        // configInput(PARAM_CONTROL + 4, "16-bit 1 + 2");  // ignore 16-bit
        configInput(PARAM_CONTROL + 5, "Tone 3 High Frequency");
        configInput(PARAM_CONTROL + 6, "Tone 1 High Frequency");
        configInput(PARAM_CONTROL + 7, "Linear Feedback Shift Register");
    }

 protected:
    // TODO: the oscillator is not tracking VOCT accurately
    /// @brief Return the frequency for the given oscillator.
    ///
    /// @param oscillator the oscillator to return the frequency for
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 8-bit frequency value from parameters and CV inputs
    ///
    inline uint8_t getFrequency(unsigned oscillator, unsigned channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float MIN = 2;
        // the maximal value for the frequency register
        static constexpr float MAX = 0xFF;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 58;
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
        freq = Math::clip(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = buffers[channel][oscillator].get_clock_rate() / (CLOCK_DIVISION * freq);
        return Math::clip(freq, MIN, MAX);
    }

    /// @brief Return the noise for the given oscillator.
    ///
    /// @param oscillator the oscillator to return the noise for
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 3-bit noise value from parameters and CV inputs
    ///
    inline uint8_t getNoise(unsigned oscillator, unsigned channel) {
        // the minimal value for the volume width register
        static constexpr float MIN = 0;
        // the maximal value for the volume width register
        static constexpr float MAX = 7;
        // get the noise from the parameter knob
        auto param = params[PARAM_NOISE + oscillator].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 10V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normal = oscillator ? inputs[INPUT_NOISE + oscillator - 1].getVoltage(channel) : 0.f;
        const auto mod = inputs[INPUT_NOISE + oscillator].getNormalVoltage(normal, channel);
        inputs[INPUT_NOISE + oscillator].setVoltage(mod, channel);
        // apply the control mod to the parameter. no adjustment is necessary
        // because the parameter lies on [0, 7]V naturally
        param += mod;
        // get the 8-bit attenuation by inverting the level and clipping
        // to the legal bounds of the parameter
        return Math::clip(param, MIN, MAX);
    }

    /// @brief Return the level for the given oscillator.
    ///
    /// @param oscillator the oscillator to return the level for
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 4-bit level value from parameters and CV inputs
    ///
    inline uint8_t getLevel(unsigned oscillator, unsigned channel) {
        // the minimal value for the volume width register
        static constexpr float MIN = 0;
        // the maximal value for the volume width register
        static constexpr float MAX = 15;
        // get the level from the parameter knob
        auto param = params[PARAM_LEVEL + oscillator].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 10V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normal = oscillator ? inputs[INPUT_LEVEL + oscillator - 1].getVoltage(channel) : 10.f;
        const auto mod = inputs[INPUT_LEVEL + oscillator].getNormalVoltage(normal, channel);
        inputs[INPUT_LEVEL + oscillator].setVoltage(mod, channel);
        // apply the control mod to the level. Normal to a constant
        // 10V source instead of checking if the cable is connected
        param = roundf(param * Math::Eurorack::fromDC(mod));
        // get the 8-bit attenuation by inverting the level and clipping
        // to the legal bounds of the parameter
        return Math::clip(param, MIN, MAX);
    }

    /// @brief Return the control byte.
    ///
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 8-bit control byte from parameters and CV inputs
    ///
    inline uint8_t getControl(unsigned channel) {
        uint8_t controlByte = 0;
        for (std::size_t bit = 0; bit < AtariPOKEY::CTL_FLAGS; bit++) {
            if (bit == 3 or bit == 4) continue;  // ignore 16-bit
            controlTriggers[channel][bit].process(rescale(inputs[INPUT_CONTROL + bit].getPolyVoltage(channel), 0.01f, 2.f, 0.f, 1.f));
            bool state = (1 - params[PARAM_CONTROL + bit].getValue()) - !controlTriggers[channel][bit].isHigh();
            // the position for the current button's index
            controlByte |= state << bit;
        }
        return controlByte;
    }

    /// @brief Process the audio rate inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the audio inputs to
    ///
    inline void processAudio(const ProcessArgs& args, const unsigned& channel) final {
        for (unsigned oscillator = 0; oscillator < AtariPOKEY::OSC_COUNT; oscillator++) {
            // there are 2 registers per oscillator, multiply first
            // oscillator by 2 to produce an offset between registers
            // based on oscillator index. the 3 noise bit occupy the MSB
            // of the control register
            apu[channel].write(AtariPOKEY::AUDF1 + AtariPOKEY::REGS_PER_VOICE * oscillator, getFrequency(oscillator, channel));
        }
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processCV(const ProcessArgs& args, const unsigned& channel) final {
        for (unsigned oscillator = 0; oscillator < AtariPOKEY::OSC_COUNT; oscillator++) {
            // there are 2 registers per oscillator, multiply first
            // oscillator by 2 to produce an offset between registers
            // based on oscillator index. the 3 noise bit occupy the MSB
            // of the control register
            apu[channel].write(AtariPOKEY::AUDC1 + AtariPOKEY::REGS_PER_VOICE * oscillator, (getNoise(oscillator, channel) << 5) | getLevel(oscillator, channel));
        }
        // write the control byte to the chip
        apu[channel].write(AtariPOKEY::AUDCTL, getControl(channel));
    }

    /// @brief Process the lights on the module.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channels the number of active polyphonic channels
    ///
    inline void processLights(const ProcessArgs& args, const unsigned& channels) final {
        for (unsigned voice = 0; voice < AtariPOKEY::OSC_COUNT; voice++) {
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

/// The panel widget for POKEY.
struct PotKeysWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit PotKeysWidget(PotKeys *module) {
        setModule(module);
        static constexpr auto panel = "res/PotKeys.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // the vertical spacing between the same component on different oscillators
        for (unsigned i = 0; i < AtariPOKEY::OSC_COUNT; i++) {  // oscillator control
            // Frequency
            addParam(createParam<Trimpot>(   Vec(13 + 35 * i, 31),  module, PotKeys::PARAM_FREQ        + i));
            addInput(createInput<PJ301MPort>(Vec(11 + 35 * i, 70),  module, PotKeys::INPUT_VOCT        + i));
            // FM
            addInput(createInput<PJ301MPort>(Vec(11 + 35 * i, 98), module, PotKeys::INPUT_FM          + i));
            addParam(createParam<Trimpot>(   Vec(13 + 35 * i, 143), module, PotKeys::PARAM_FM          + i));
            // Level
            addParam(createSnapParam<Trimpot>(Vec(13 + 35 * i, 169), module, PotKeys::PARAM_LEVEL       + i));
            addInput(createInput<PJ301MPort>( Vec(11 + 35 * i, 209), module, PotKeys::INPUT_LEVEL       + i));
            // Noise
            addParam(createSnapParam<Trimpot>(Vec(13 + 35 * i, 241), module, PotKeys::PARAM_NOISE + i));
            addInput(createInput<PJ301MPort>( Vec(11 + 35 * i, 281), module, PotKeys::INPUT_NOISE + i));
            // Output
            addChild(createLight<SmallLight<RedGreenBlueLight>>(Vec(30 + 35 * i, 319), module, PotKeys::LIGHTS_LEVEL + 3 * i));
            addOutput(createOutput<PJ301MPort>(Vec(11 + 35 * i, 324), module, PotKeys::OUTPUT_OSCILLATOR + i));
        }
        float offset = 0;
        for (unsigned i = 0; i < AtariPOKEY::CTL_FLAGS; i++) {  // Global control
            if (i == 3 or i == 4) continue;  // ignore 16-bit (not implemented)
            addParam(createParam<CKSS>(Vec(152, 45 + offset), module, PotKeys::PARAM_CONTROL + i));
            addInput(createInput<PJ301MPort>(Vec(175, 44 + offset), module, PotKeys::INPUT_CONTROL + i));
            offset += 56;
        }
    }
};

/// the global instance of the model
rack::Model *modelPotKeys = rack::createModel<PotKeys, PotKeysWidget>("POKEY");
