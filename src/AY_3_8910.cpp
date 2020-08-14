// A General Instrument AY-3-8910 Chip module.
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
#include "dsp/general_instrument_ay_3_8910.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A General Instrument AY-3-8910 chip emulator module.
struct ChipAY_3_8910 : ChipModule<GeneralInstrumentAy_3_8910> {
 private:
    /// triggers for handling inputs to the tone and noise enable switches
    rack::dsp::BooleanTrigger mixerTriggers[2 * GeneralInstrumentAy_3_8910::OSC_COUNT];

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(PARAM_LEVEL, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(PARAM_TONE, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(PARAM_NOISE, GeneralInstrumentAy_3_8910::OSC_COUNT),
        NUM_PARAMS
    };
    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_FM, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_LEVEL, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_TONE, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_NOISE, GeneralInstrumentAy_3_8910::OSC_COUNT),
        NUM_INPUTS
    };
    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_OSCILLATOR, GeneralInstrumentAy_3_8910::OSC_COUNT),
        NUM_OUTPUTS
    };
    /// the indexes of lights on the module
    enum LightIds { NUM_LIGHTS };

    /// @brief Initialize a new FME7 Chip module.
    ChipAY_3_8910() : ChipModule<GeneralInstrumentAy_3_8910>() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned oscillator = 0; oscillator < GeneralInstrumentAy_3_8910::OSC_COUNT; oscillator++) {
            // get the channel name starting with ACII code 65 (A)
            auto channel_name = std::string(1, static_cast<char>(65 + oscillator));
            configParam(PARAM_FREQ  + oscillator, -60.f, 60.f, 0.f,  "Pulse " + channel_name + " Frequency",     " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
            configParam(PARAM_LEVEL + oscillator,  0.f,   1.f, 0.9f, "Pulse " + channel_name + " Level",         "%",   0.f,                100.f       );
            configParam(PARAM_TONE  + oscillator,  0,     1,   1,    "Pulse " + channel_name + " Tone Enabled",  "");
            configParam(PARAM_NOISE + oscillator,  0,     1,   0,    "Pulse " + channel_name + " Noise Enabled", "");
        }
    }

    /// @brief Return the frequency for the given channel.
    ///
    /// @param oscillator the oscillator to return the frequency for
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 12-bit frequency in a 16-bit container
    ///
    inline uint16_t getFrequency(unsigned oscillator, unsigned channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ12BIT_MIN = 2;
        // the maximal value for the frequency register
        static constexpr float FREQ12BIT_MAX = 4095;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + oscillator].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + oscillator].getPolyVoltage(channel);
        pitch += inputs[INPUT_FM + oscillator].getPolyVoltage(channel) / 5.f;
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = buffers[channel][oscillator].get_clock_rate() / (CLOCK_DIVISION * freq);
        return rack::clamp(freq, FREQ12BIT_MIN, FREQ12BIT_MAX);
    }

    /// @brief Return the level for the given channel.
    ///
    /// @param oscillator the oscillator to return the frequency for
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 4-bit level value in an 8-bit container
    ///
    inline uint8_t getLevel(unsigned oscillator, unsigned channel) {
        // the minimal value for the volume width register
        static constexpr float LEVEL_MIN = 0;
        // the maximal value for the volume width register
        static constexpr float LEVEL_MAX = 15;
        // get the level from the parameter knob
        auto param = params[PARAM_LEVEL + oscillator].getValue();
        // apply the control voltage to the attenuation
        if (inputs[INPUT_LEVEL + oscillator].isConnected()) {
            auto cv = inputs[INPUT_LEVEL + oscillator].getPolyVoltage(channel) / 10.f;
            cv = rack::clamp(cv, 0.f, 1.f);
            cv = roundf(100.f * cv) / 100.f;
            param *= 2 * cv;
        }
        // get the 8-bit level clamped within legal limits
        return rack::clamp(LEVEL_MAX * param, LEVEL_MIN, LEVEL_MAX);
    }

    /// @brief Return the noise period.
    ///
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the period for the noise oscillator
    /// @details
    /// Returns a frequency based on the knob for oscillator 3 (index 2)
    ///
    inline uint8_t getNoise(unsigned channel) {
        // use the frequency control knob from oscillator index 2
        static constexpr unsigned oscillator = 2;
        // the minimal value for the noise frequency register to produce sound
        static constexpr float FREQ_MIN = 0;
        // the maximal value for the noise frequency register
        static constexpr float FREQ_MAX = 31;
        // get the parameter value from the UI knob. the knob represents
        // frequency, so translate to a simple [0, 1] scale.
        auto param = (0.5f + params[PARAM_FREQ + oscillator].getValue() / 120.f);
        // 5V scale for V/OCT input
        param += inputs[INPUT_VOCT + oscillator].getPolyVoltage(channel) / 5.f;
        // 10V scale for mod input
        param += inputs[INPUT_FM + oscillator].getPolyVoltage(channel) / 10.f;
        // clamp the parameter within its legal limits
        param = rack::clamp(param, 0.f, 1.f);
        // get the 5-bit noise period clamped within legal limits. invert the
        // value about the maximal to match directionality of oscillator frequency
        return FREQ_MAX - rack::clamp(FREQ_MAX * param, FREQ_MIN, FREQ_MAX);
    }

    /// @brief Return the mixer byte.
    ///
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 8-bit mixer byte from parameters and CV inputs
    ///
    inline uint8_t getMixer(unsigned channel) {
        uint8_t mixerByte = 0;
        // iterate over the oscillators to set the mixer tone and noise flags.
        // there is a set of 3 flags for both tone and noise. start with
        // iterating over the tone inputs and parameters, but fall through to
        // getting the noise inputs and parameters, which immediately follow
        // those of the tone. I.e., INPUT_TONE = INPUT_NOISE and
        // PARAM_TONE = PARAM_NOISE when i > 2.
        for (unsigned i = 0; i < 2 * GeneralInstrumentAy_3_8910::OSC_COUNT; i++) {
            // clamp the input within [0, 10]. this allows bipolar signals to
            // be interpreted as unipolar signals for the trigger input
            auto cv = math::clamp(inputs[INPUT_TONE + i].getPolyVoltage(channel), 0.f, 10.f);
            mixerTriggers[i].process(rescale(cv, 0.f, 2.f, 0.f, 1.f));
            // get the state of the tone based on the parameter and trig input
            bool toneState = params[PARAM_TONE + i].getValue() - mixerTriggers[i].state;
            // invert the state to indicate "OFF" semantics instead of "ON"
            mixerByte |= !toneState << i;
        }
        return mixerByte;
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processCV(const ProcessArgs &args, unsigned channel) final {
        for (unsigned oscillator = 0; oscillator < GeneralInstrumentAy_3_8910::OSC_COUNT; oscillator++) {
            // 2 frequency registers per voice, shift over by 1 instead of
            // multiplying
            auto offset = oscillator << 1;
            auto freq = getFrequency(oscillator, channel);
            auto lo =  freq & 0b0000000011111111;
            apu[channel].write(GeneralInstrumentAy_3_8910::PERIOD_CH_A_LO + offset, lo);
            auto hi = (freq & 0b0000111100000000) >> 8;
            apu[channel].write(GeneralInstrumentAy_3_8910::PERIOD_CH_A_HI + offset, hi);
            // volume
            auto level = getLevel(oscillator, channel);
            apu[channel].write(GeneralInstrumentAy_3_8910::VOLUME_CH_A + oscillator, level);
        }
        // set the 5-bit noise value based on the channel 3 parameter
        apu[channel].write(GeneralInstrumentAy_3_8910::NOISE_PERIOD, getNoise(channel));
        // set the 6-channel boolean mixer (tone and noise for each channel)
        apu[channel].write(GeneralInstrumentAy_3_8910::CHANNEL_ENABLES, getMixer(channel));
        // envelope period (TODO: fix envelope in engine)
        // apu[channel].write(GeneralInstrumentAy_3_8910::PERIOD_ENVELOPE_LO, 0b10101011);
        // apu[channel].write(GeneralInstrumentAy_3_8910::PERIOD_ENVELOPE_HI, 0b00000011);
        // envelope shape bits (TODO: fix envelope in engine)
        // apu[channel].write(
        //     GeneralInstrumentAy_3_8910::ENVELOPE_SHAPE,
        //     GeneralInstrumentAy_3_8910::ENVELOPE_SHAPE_NONE
        // );
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

/// The panel widget for AY-3-8910.
struct ChipAY_3_8910Widget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit ChipAY_3_8910Widget(ChipAY_3_8910 *module) {
        setModule(module);
        static constexpr auto panel = "res/AY_3_8910.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        for (unsigned i = 0; i < GeneralInstrumentAy_3_8910::OSC_COUNT; i++) {
            addInput(createInput<PJ301MPort>(    Vec(18,  27  + i * 111), module, ChipAY_3_8910::INPUT_FM       + i));
            addInput(createInput<PJ301MPort>(    Vec(18,  100 + i * 111), module, ChipAY_3_8910::INPUT_VOCT     + i));
            addParam(createParam<Rogan6PSWhite>( Vec(47,  29  + i * 111), module, ChipAY_3_8910::PARAM_FREQ     + i));
            addParam(createParam<CKSS>(          Vec(144, 29  + i * 111), module, ChipAY_3_8910::PARAM_TONE     + i));
            addInput(createInput<PJ301MPort>(    Vec(147, 53  + i * 111), module, ChipAY_3_8910::INPUT_TONE     + i));
            addParam(createParam<CKSS>(          Vec(138, 105 + i * 111), module, ChipAY_3_8910::PARAM_NOISE    + i));
            addInput(createInput<PJ301MPort>(    Vec(175, 65  + i * 111), module, ChipAY_3_8910::INPUT_NOISE    + i));
            addInput(createInput<PJ301MPort>(    Vec(182, 35  + i * 111), module, ChipAY_3_8910::INPUT_LEVEL    + i));
            addParam(createParam<BefacoSlidePot>(Vec(211, 21  + i * 111), module, ChipAY_3_8910::PARAM_LEVEL    + i));
            addOutput(createOutput<PJ301MPort>(  Vec(180, 100 + i * 111), module, ChipAY_3_8910::OUTPUT_OSCILLATOR + i));
        }
    }
};

/// the global instance of the model
Model *modelChipAY_3_8910 = createModel<ChipAY_3_8910, ChipAY_3_8910Widget>("AY_3_8910");
