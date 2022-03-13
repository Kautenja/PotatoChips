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
#include "dsp/math.hpp"
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
        getParamQuantity(PARAM_PW + 0)->snapEnabled = true;
        getParamQuantity(PARAM_PW + 1)->snapEnabled = true;
        configParam(PARAM_LEVEL + 0,  0,   15,   12,   "Pulse 1 Level");
        configParam(PARAM_LEVEL + 1,  0,   15,   12,   "Pulse 2 Level");
        configParam(PARAM_LEVEL + 2,  0,   63,   32,   "Saw Level");
        getParamQuantity(PARAM_LEVEL + 0)->snapEnabled = true;
        getParamQuantity(PARAM_LEVEL + 1)->snapEnabled = true;
        getParamQuantity(PARAM_LEVEL + 2)->snapEnabled = true;
        configInput(INPUT_VOCT + 0, "Pulse 1 V/Oct");
        configInput(INPUT_VOCT + 1, "Pulse 2 V/Oct");
        configInput(INPUT_VOCT + 2, "Saw V/Oct");
        configInput(INPUT_FM + 0, "Pulse 1 FM");
        configInput(INPUT_FM + 1, "Pulse 2 FM");
        configInput(INPUT_FM + 2, "Saw FM");
        configInput(INPUT_PW + 0, "Pulse 1 Duty Cycle");
        configInput(INPUT_PW + 1, "Pulse 2 Duty Cycle");
        configInput(INPUT_SYNC, "Saw Sync");
        configInput(INPUT_LEVEL + 0, "Pulse 1 Level");
        configInput(INPUT_LEVEL + 1, "Pulse 2 Level");
        configInput(INPUT_LEVEL + 2, "Saw Level");
        configOutput(OUTPUT_OSCILLATOR + 0, "Pulse 1");
        configOutput(OUTPUT_OSCILLATOR + 1, "Pulse 2");
        configOutput(OUTPUT_OSCILLATOR + 2, "Saw");
    }

 protected:
    /// trigger for handling inputs to the sync port for the saw wave
    Trigger::Threshold syncTriggers[PORT_MAX_CHANNELS];

    /// @brief Get the frequency for the given oscillator and polyphony channel.
    ///
    /// @tparam MAX the maximal value for the frequency register
    /// @tparam DIVISION the clock division of the oscillator relative
    /// @param oscillator the oscillator to return the frequency for
    /// @param channel the polyphonic channel to return the frequency for
    /// @param freq_min the minimal value for the frequency register to
    /// produce sound
    /// to the CPU
    /// @returns the 12 bit frequency value from the panel
    /// @details
    /// parameters for pulse wave (max is implied 4095):
    /// freq_min = 4, clock_division = 16
    /// parameters for saw wave (max is implied 4095):
    /// freq_min = 3, clock_division = 14
    ///
    template<uint16_t MIN, uint16_t DIVISION>
    inline uint16_t getFrequency(const unsigned& oscillator, const unsigned& channel) {
        float pitch = params[PARAM_FREQ + oscillator].getValue();
        pitch += normalChain(&inputs[INPUT_VOCT], oscillator, channel, 0.f);
        const float att = params[PARAM_FM + oscillator].getValue();
        pitch += att * Math::Eurorack::fromDC(normalChain(&inputs[INPUT_FM], oscillator, channel, 5.f));
        float freq = Math::Eurorack::voct2freq(pitch);
        // convert the frequency to an 11-bit value
        freq = (buffers[channel][oscillator].get_clock_rate() / (static_cast<float>(DIVISION) * freq)) - 1;
        static constexpr float MAX = 4095.f;
        return Math::clip(freq, static_cast<float>(MIN), MAX);
    }

    /// @brief Return the pulse width parameter for the given oscillator and
    /// polyphony channel.
    ///
    /// @param oscillator the oscillator to return the pulse width for
    /// @param channel the polyphony channel of the given oscillator
    /// @returns the pulse width value in an 8-bit container in the high 4 bits.
    /// if channel == 2, i.e., saw channel, returns 0 (no PW for saw wave)
    ///
    inline uint8_t getPW(const unsigned& oscillator, const unsigned& channel) {
        if (oscillator == KonamiVRC6::SAW) return 0;  // no PW for saw wave
        const float param = params[PARAM_PW + oscillator].getValue();
        const float mod = normalChain(&inputs[INPUT_PW], oscillator, channel, 0.f);
        // get the 8-bit pulse width clamped within legal limits
        static constexpr float PW_MIN = 0;
        static constexpr float PW_MAX = 0b00000111;
        uint8_t pw = Math::clip(param + mod, PW_MIN, PW_MAX);
        // shift the pulse width over into the high 4 bits
        return pw << 4;
    }

    /// @brief Return the level parameter for the given oscillator and
    /// polyphony channel.
    ///
    /// @tparam MAX the maximal level for the input oscillator
    /// @param oscillator the oscillator to return the pulse width value for
    /// @param channel the polyphony channel of the given oscillator
    /// @returns the level value in an 8-bit container in the low 4 bits
    ///
    template<uint8_t MAX>
    inline uint8_t getLevel(const unsigned& oscillator, const unsigned& channel) {
        float level = params[PARAM_LEVEL + oscillator].getValue();
        const float voltage = normalChain(&inputs[INPUT_LEVEL], oscillator, channel, 10.f);
        return Math::clip(level * Math::Eurorack::fromDC(voltage), 0.f, static_cast<float>(MAX));
    }

    /// @brief Set the frequency for an oscillator.
    ///
    /// @param freq the frequency to set the oscillator to
    /// @param oscillator the oscillator to set the frequency of
    /// @param channel the polyphony channel of the given oscillator
    ///
    inline void setApuFrequency(const uint16_t& freq, const unsigned& oscillator, const unsigned& channel) {
        uint8_t lo =  freq & 0b0000000011111111;
        uint8_t hi = (freq & 0b0000111100000000) >> 8;
        hi |= KonamiVRC6::PERIOD_HIGH_ENABLED;  // enable the oscillator
        apu[channel].write(KonamiVRC6::PULSE0_PERIOD_LOW + KonamiVRC6::REGS_PER_OSC * oscillator, lo);
        apu[channel].write(KonamiVRC6::PULSE0_PERIOD_HIGH + KonamiVRC6::REGS_PER_OSC * oscillator, hi);
    }

    /// @brief Process the audio rate inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the audio inputs to
    ///
    inline void processAudio(const ProcessArgs& args, const unsigned& channel) final {
        // detect sync for triangle generator voice
        const float sync = rescale(inputs[INPUT_SYNC].getVoltage(channel), 0.01f, 0.02f, 0.f, 1.f);
        if (syncTriggers[channel].process(sync)) apu[channel].reset_phase(2);
        // set frequency for all voices
        setApuFrequency(getFrequency<4, 16>(0, channel), 0, channel);
        setApuFrequency(getFrequency<4, 16>(1, channel), 1, channel);
        setApuFrequency(getFrequency<3, 14>(2, channel), 2, channel);
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processCV(const ProcessArgs& args, const unsigned& channel) final {
        apu[channel].write(KonamiVRC6::PULSE0_DUTY_VOLUME + KonamiVRC6::REGS_PER_OSC * 0, getPW(0, channel) | getLevel<15>(0, channel));
        apu[channel].write(KonamiVRC6::PULSE0_DUTY_VOLUME + KonamiVRC6::REGS_PER_OSC * 1, getPW(1, channel) | getLevel<15>(1, channel));
        apu[channel].write(KonamiVRC6::PULSE0_DUTY_VOLUME + KonamiVRC6::REGS_PER_OSC * 2, getPW(2, channel) | getLevel<63>(2, channel));
    }

    /// @brief Process the lights on the module.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channels the number of active polyphonic channels
    ///
    inline void processLights(const ProcessArgs& args, const unsigned& channels) final {
        for (unsigned voice = 0; voice < KonamiVRC6::OSC_COUNT; voice++)
            setVULight3(vuMeter[voice], &lights[LIGHTS_LEVEL + voice * 3]);
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
            addParam(createParam<Trimpot>( Vec(15 + 35 * i, 170), module, StepSaw::PARAM_LEVEL + i));
            addInput(createInput<PJ301MPort>(  Vec(13 + 35 * i, 210), module, StepSaw::INPUT_LEVEL + i));
            if (i < 2) {  // pulse width for tone generator
                addParam(createParam<Trimpot>(Vec(15 + 35 * i, 241), module, StepSaw::PARAM_PW + i));
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
