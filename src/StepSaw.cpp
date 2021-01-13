// A Konami VRC6 Chip module.
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
#include "dsp/trigger.hpp"
#include "dsp/konami_vrc6.hpp"
#include "engine/chip_module.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Konami VRC6 chip emulator module.
struct StepSaw : ChipModule<KonamiVRC6> {
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ, KonamiVRC6::OSC_COUNT),
        ENUMS(PARAM_FM, KonamiVRC6::OSC_COUNT),
        ENUMS(PARAM_PW, KonamiVRC6::OSC_COUNT - 1),  // pulse wave only
        ENUMS(PARAM_LEVEL, KonamiVRC6::OSC_COUNT),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT, KonamiVRC6::OSC_COUNT),
        ENUMS(INPUT_FM, KonamiVRC6::OSC_COUNT),
        ENUMS(INPUT_PW, KonamiVRC6::OSC_COUNT - 1),  // pulse wave only
        ENUMS(INPUT_LEVEL, KonamiVRC6::OSC_COUNT),
        INPUT_SYNC,
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_OSCILLATOR, KonamiVRC6::OSC_COUNT),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(LIGHTS_LEVEL, 3 * KonamiVRC6::OSC_COUNT),
        NUM_LIGHTS
    };

    /// @brief Initialize a new VRC6 Chip module.
    StepSaw() : ChipModule<KonamiVRC6>(5.f) {
        normal_outputs = true;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(PARAM_FREQ + 0,  -2.5f, 2.5f, 0.f, "Pulse 1 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1,  -2.5f, 2.5f, 0.f, "Pulse 2 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2,  -2.5f, 2.5f, 0.f, "Saw Frequency",     " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FM   + 0,  -1.f,  1.f,  0.f, "Pulse 1 FM");
        configParam(PARAM_FM   + 1,  -1.f,  1.f,  0.f, "Pulse 2 FM");
        configParam(PARAM_FM   + 2,  -1.f,  1.f,  0.f, "Saw FM");
        configParam(PARAM_PW + 0,     0,    7,    7,   "Pulse 1 Duty Cycle");
        configParam(PARAM_PW + 1,     0,    7,    7,   "Pulse 1 Duty Cycle");
        configParam(PARAM_LEVEL + 0,  0,   15,   12,   "Pulse 1 Level");
        configParam(PARAM_LEVEL + 1,  0,   15,   12,   "Pulse 2 Level");
        configParam(PARAM_LEVEL + 2,  0,   63,   32,   "Saw Level");
    }

 protected:
    /// trigger for handling inputs to the sync port for the saw wave
    Trigger::Threshold syncTriggers[PORT_MAX_CHANNELS];

    /// @brief Get the frequency for the given oscillator and polyphony channel.
    ///
    /// @param oscillator the oscillator to return the frequency for
    /// @param channel the polyphonic channel to return the frequency for
    /// @param freq_min the minimal value for the frequency register to
    /// produce sound
    /// @param freq_max the maximal value for the frequency register
    /// @param clock_division the clock division of the oscillator relative
    /// to the CPU
    /// @returns the 12 bit frequency value from the panel
    /// @details
    /// parameters for pulse wave:
    /// freq_min = 4, freq_max = 4095, clock_division = 16
    /// parameters for triangle wave:
    /// freq_min = 3, freq_max = 4095, clock_division = 14
    ///
    inline uint16_t getFrequency(
        unsigned oscillator,
        unsigned channel,
        float freq_min,
        float freq_max,
        float clock_division
    ) {
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
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to an 11-bit value
        freq = (buffers[channel][oscillator].get_clock_rate() / (clock_division * freq)) - 1;
        return rack::clamp(freq, freq_min, freq_max);
    }

    /// @brief Return the pulse width parameter for the given oscillator and
    /// polyphony channel.
    ///
    /// @param oscillator the oscillator to return the pulse width for
    /// @param channel the polyphony channel of the given oscillator
    /// @returns the pulse width value in an 8-bit container in the high 4 bits.
    /// if channel == 2, i.e., saw channel, returns 0 (no PW for saw wave)
    ///
    inline uint8_t getPW(unsigned oscillator, unsigned channel) {
        // the minimal value for the pulse width register
        static constexpr float PW_MIN = 0;
        // the maximal value for the pulse width register (before shift)
        static constexpr float PW_MAX = 0b00000111;
        if (oscillator == KonamiVRC6::SAW) return 0;  // no PW for saw wave
        // get the pulse width from the parameter knob
        auto param = params[PARAM_PW + oscillator].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 5V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normalMod = oscillator ? inputs[INPUT_PW + oscillator - 1].getVoltage(channel) : 0.f;
        const auto mod = inputs[INPUT_PW + oscillator].getNormalVoltage(normalMod, channel);
        inputs[INPUT_PW + oscillator].setVoltage(mod, channel);
        // get the 8-bit pulse width clamped within legal limits
        uint8_t pw = rack::clamp(param + mod, PW_MIN, PW_MAX);
        // shift the pulse width over into the high 4 bits
        return pw << 4;
    }

    /// @brief Return the level parameter for the given oscillator and
    /// polyphony channel.
    ///
    /// @param oscillator the oscillator to return the pulse width value for
    /// @param channel the polyphony channel of the given oscillator
    /// @param max_level the maximal level for the input oscillator
    /// @returns the level value in an 8-bit container in the low 4 bits
    ///
    inline uint8_t getLevel(unsigned oscillator, unsigned channel, uint8_t max_level) {
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
        level = roundf(level * voltage / 10.f);
        // get the 8-bit attenuation by inverting the level and clipping
        // to the legal bounds of the parameter
        return rack::clamp(level, 0.f, static_cast<float>(max_level));
    }

    /// @brief Process the audio rate inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the audio inputs to
    ///
    inline void processAudio(const ProcessArgs &args, unsigned channel) final {
        static constexpr float freq_low[KonamiVRC6::OSC_COUNT] =       { 4,  4,  3};
        static constexpr float clock_division[KonamiVRC6::OSC_COUNT] = {16, 16, 14};
        // detect sync for triangle generator voice
        const float sync = rescale(inputs[INPUT_SYNC].getVoltage(channel), 0.01f, 0.02f, 0.f, 1.f);
        if (syncTriggers[channel].process(sync)) apu[channel].reset_phase(2);
        // set frequency for all voices
        for (unsigned oscillator = 0; oscillator < KonamiVRC6::OSC_COUNT; oscillator++) {
            // frequency (max frequency is same for pulses and saw, 4095)
            uint16_t freq = getFrequency(oscillator, channel, freq_low[oscillator], 4095, clock_division[oscillator]);
            uint8_t lo =  freq & 0b0000000011111111;
            uint8_t hi = (freq & 0b0000111100000000) >> 8;
            hi |= KonamiVRC6::PERIOD_HIGH_ENABLED;  // enable the oscillator
            apu[channel].write(KonamiVRC6::PULSE0_PERIOD_LOW + KonamiVRC6::REGS_PER_OSC * oscillator, lo);
            apu[channel].write(KonamiVRC6::PULSE0_PERIOD_HIGH + KonamiVRC6::REGS_PER_OSC * oscillator, hi);
        }
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processCV(const ProcessArgs &args, unsigned channel) final {
        static constexpr float max_level[KonamiVRC6::OSC_COUNT] = {15, 15, 63};
        for (unsigned oscillator = 0; oscillator < KonamiVRC6::OSC_COUNT; oscillator++) {
            // level
            uint8_t level = getPW(oscillator, channel) | getLevel(oscillator, channel, max_level[oscillator]);
            apu[channel].write(KonamiVRC6::PULSE0_DUTY_VOLUME + KonamiVRC6::REGS_PER_OSC * oscillator, level);
        }
    }

    /// @brief Process the lights on the module.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channels the number of active polyphonic channels
    ///
    inline void processLights(const ProcessArgs &args, unsigned channels) final {
        for (unsigned voice = 0; voice < KonamiVRC6::OSC_COUNT; voice++) {
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

/// The panel widget for VRC6.
struct StepSawWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit StepSawWidget(StepSaw *module) {
        setModule(module);
        static constexpr auto panel = "res/StepSaw.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        // addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        // addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        for (unsigned i = 0; i < KonamiVRC6::OSC_COUNT; i++) {
            // Frequency
            addParam(createParam<Trimpot>(     Vec(15 + 35 * i, 32),  module, StepSaw::PARAM_FREQ  + i));
            addInput(createInput<PJ301MPort>(  Vec(13 + 35 * i, 71),  module, StepSaw::INPUT_VOCT  + i));
            // FM
            addInput(createInput<PJ301MPort>(  Vec(13 + 35 * i, 99), module, StepSaw::INPUT_FM    + i));
            addParam(createParam<Trimpot>(     Vec(15 + 35 * i, 144), module, StepSaw::PARAM_FM    + i));
            // Level
            addParam(createSnapParam<Trimpot>( Vec(15 + 35 * i, 170), module, StepSaw::PARAM_LEVEL + i));
            addInput(createInput<PJ301MPort>(  Vec(13 + 35 * i, 210), module, StepSaw::INPUT_LEVEL + i));
            if (i < 2) {  // pulse width for tone generator
                addParam(createSnapParam<Trimpot>(Vec(15 + 35 * i, 241), module, StepSaw::PARAM_PW + i));
                addInput(createInput<PJ301MPort>(Vec(13 + 35 * i, 281), module, StepSaw::INPUT_PW + i));
            } else {  // sync for saw wave
                addInput(createInput<PJ301MPort>(Vec(13 + 35 * i, 264), module, StepSaw::INPUT_SYNC));
            }
            // Output
            addChild(createLight<SmallLight<RedGreenBlueLight>>(Vec(32 + 35 * i, 319), module, StepSaw::LIGHTS_LEVEL + 3 * i));
            addOutput(createOutput<PJ301MPort>(Vec(13 + 35 * i, 324), module, StepSaw::OUTPUT_OSCILLATOR + i));
        }
    }
};

/// the global instance of the model
rack::Model *modelStepSaw = createModel<StepSaw, StepSawWidget>("VRC6");
