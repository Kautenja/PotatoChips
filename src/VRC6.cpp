// A Konami VRC6 Chip module.
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
#include "engine/chip_module.hpp"
#include "dsp/konami_vrc6.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Konami VRC6 chip emulator module.
struct ChipVRC6 : ChipModule<KonamiVRC6> {
 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ, KonamiVRC6::OSC_COUNT),
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
        NUM_INPUTS
    };
    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_OSCILLATOR, KonamiVRC6::OSC_COUNT),
        NUM_OUTPUTS
    };
    /// the indexes of lights on the module
    enum LightIds {
        NUM_LIGHTS
    };

    /// @brief Initialize a new VRC6 Chip module.
    ChipVRC6() : ChipModule<KonamiVRC6>() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(PARAM_FREQ + 0,  -2.5f, 2.5f, 0.f,  "Pulse 1 Frequency",        " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1,  -2.5f, 2.5f, 0.f,  "Pulse 2 Frequency",        " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2,  -2.5f, 2.5f, 0.f,  "Saw Frequency",            " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_PW + 0,     0,    7,    4,    "Pulse 1 Duty Cycle"                                               );
        configParam(PARAM_PW + 1,     0,    7,    4,    "Pulse 1 Duty Cycle"                                               );
        configParam(PARAM_LEVEL + 0,  0.f,  1.f,  0.8f, "Pulse 1 Level",            "%",   0.f,                100.f       );
        configParam(PARAM_LEVEL + 1,  0.f,  1.f,  0.8f, "Pulse 2 Level",            "%",   0.f,                100.f       );
        configParam(PARAM_LEVEL + 2,  0.f,  1.f,  0.5f, "Saw Level / Quantization", "%",   0.f,                100.f       );
    }

 protected:
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
        pitch += inputs[INPUT_VOCT + oscillator].getPolyVoltage(channel);
        pitch += inputs[INPUT_FM + oscillator].getPolyVoltage(channel) / 5.f;
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
        auto pwParam = params[PARAM_PW + oscillator].getValue();
        // get the control voltage to the pulse width with 1V/step
        auto pwCV = inputs[INPUT_PW + oscillator].getPolyVoltage(channel) / 2.f;
        // get the 8-bit pulse width clamped within legal limits
        uint8_t pw = rack::clamp(pwParam + pwCV, PW_MIN, PW_MAX);
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
        auto param = params[PARAM_LEVEL + oscillator].getValue();
        // apply the control voltage to the level
        if (inputs[INPUT_LEVEL + oscillator].isConnected()) {
            auto cv = inputs[INPUT_LEVEL + oscillator].getPolyVoltage(channel) / 10.f;
            cv = rack::clamp(cv, 0.f, 1.f);
            cv = roundf(100.f * cv) / 100.f;
            param *= 2 * cv;
        }
        // get the 8-bit level clamped within legal limits
        return rack::clamp(max_level * param, 0.f, static_cast<float>(max_level));
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processCV(const ProcessArgs &args, unsigned channel) final {
        static constexpr float freq_low[KonamiVRC6::OSC_COUNT] =       { 4,  4,  3};
        static constexpr float clock_division[KonamiVRC6::OSC_COUNT] = {16, 16, 14};
        static constexpr float max_level[KonamiVRC6::OSC_COUNT] =      {15, 15, 63};
        for (unsigned oscillator = 0; oscillator < KonamiVRC6::OSC_COUNT; oscillator++) {
            // frequency (max frequency is same for pulses and saw, 4095)
            uint16_t freq = getFrequency(oscillator, channel, freq_low[oscillator], 4095, clock_division[oscillator]);
            uint8_t lo =  freq & 0b0000000011111111;
            uint8_t hi = (freq & 0b0000111100000000) >> 8;
            hi |= KonamiVRC6::PERIOD_HIGH_ENABLED;  // enable the oscillator
            apu[channel].write(KonamiVRC6::PULSE0_PERIOD_LOW + KonamiVRC6::REGS_PER_OSC * oscillator, lo);
            apu[channel].write(KonamiVRC6::PULSE0_PERIOD_HIGH + KonamiVRC6::REGS_PER_OSC * oscillator, hi);
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
    inline void processLights(const ProcessArgs &args, unsigned channels) final { }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for VRC6.
struct ChipVRC6Widget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit ChipVRC6Widget(ChipVRC6 *module) {
        setModule(module);
        static constexpr auto panel = "res/VRC6.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        for (unsigned i = 0; i < KonamiVRC6::OSC_COUNT - 1; i++) {
            addInput(createInput<PJ301MPort>(    Vec(18,  69  + i * 111), module, ChipVRC6::INPUT_VOCT     + i));
            addInput(createInput<PJ301MPort>(    Vec(18,  34  + i * 111), module, ChipVRC6::INPUT_FM       + i));
            addParam(createParam<Rogan6PSWhite>( Vec(47,  29  + i * 111), module, ChipVRC6::PARAM_FREQ     + i));
            auto pw = createParam<RoundSmallBlackKnob>(Vec(146, 35 + i * 111), module, ChipVRC6::PARAM_PW + i);
            pw->snap = true;
            addParam(pw);
            addInput(createInput<PJ301MPort>(Vec(145, 70 + i * 111), module, ChipVRC6::INPUT_PW + i));
            addInput(createInput<PJ301MPort>(    Vec(18, 104  + i * 111), module, ChipVRC6::INPUT_LEVEL    + i));
            addParam(createParam<BefacoSlidePot>(Vec(180, 21  + i * 111), module, ChipVRC6::PARAM_LEVEL    + i));
            addOutput(createOutput<PJ301MPort>(  Vec(150, 100 + i * 111), module, ChipVRC6::OUTPUT_OSCILLATOR + i));
        }
        int i = 2;
        addInput(createInput<PJ301MPort>(    Vec(18,  322), module, ChipVRC6::INPUT_VOCT     + i));
        addInput(createInput<PJ301MPort>(    Vec(18,  249), module, ChipVRC6::INPUT_FM       + i));
        addParam(createParam<Rogan6PSWhite>( Vec(47,  29  + i * 111), module, ChipVRC6::PARAM_FREQ     + i));
        addInput(createInput<PJ301MPort>(    Vec(152, 257), module, ChipVRC6::INPUT_LEVEL    + i));
        addParam(createParam<BefacoSlidePot>(Vec(180, 21  + i * 111), module, ChipVRC6::PARAM_LEVEL    + i));
        addOutput(createOutput<PJ301MPort>(  Vec(150, 100 + i * 111), module, ChipVRC6::OUTPUT_OSCILLATOR + i));
    }
};

/// the global instance of the model
Model *modelChipVRC6 = createModel<ChipVRC6, ChipVRC6Widget>("VRC6");
