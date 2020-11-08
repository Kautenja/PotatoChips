// An envelope generator module based on the S-SMP chip from Nintendo SNES.
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

#include <limits>
#include "plugin.hpp"
#include "dsp/sony_s_dsp/adsr.hpp"

// TODO: lights
// - on sliders for CV levels
// - on panel for stage indication
// TODO: EOR / EOC outputs

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// An envelope generator module based on the S-SMP chip from Nintendo SNES.
struct ChipS_SMP_ADSR : Module {
    /// the number of processing lanes on the module
    static constexpr unsigned LANES = 2;

 private:
    /// the Sony S-DSP ADSR enveloper generator emulator
    Sony_S_DSP_ADSR apus[LANES][PORT_MAX_CHANNELS];
    /// triggers for handling input trigger and gate signals
    rack::dsp::BooleanTrigger gateTrigger[LANES][PORT_MAX_CHANNELS];
    /// triggers for handling input re-trigger signals
    rack::dsp::BooleanTrigger retrigTrigger[LANES][PORT_MAX_CHANNELS];

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_AMPLITUDE,     LANES),
        ENUMS(PARAM_ATTACK,        LANES),
        ENUMS(PARAM_DECAY,         LANES),
        ENUMS(PARAM_SUSTAIN_LEVEL, LANES),
        ENUMS(PARAM_SUSTAIN_RATE,  LANES),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_GATE,          LANES),
        ENUMS(INPUT_RETRIG,        LANES),
        ENUMS(INPUT_AMPLITUDE,     LANES),
        ENUMS(INPUT_ATTACK,        LANES),
        ENUMS(INPUT_DECAY,         LANES),
        ENUMS(INPUT_SUSTAIN_LEVEL, LANES),
        ENUMS(INPUT_SUSTAIN_RATE,  LANES),
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_ENVELOPE, LANES),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds { NUM_LIGHTS };

    /// @brief Initialize a new S-DSP Chip module.
    ChipS_SMP_ADSR() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned lane = 0; lane < LANES; lane++) {
            configParam(PARAM_AMPLITUDE     + lane, -128, 127, 127, "Amplitude");
            configParam(PARAM_ATTACK        + lane,    0,  15,  10, "Attack");
            configParam(PARAM_DECAY         + lane,    0,   7,   7, "Decay");
            configParam(PARAM_SUSTAIN_LEVEL + lane,    0,   7,   5, "Sustain Level", "%", 0, 100.f / 7.f);
            configParam(PARAM_SUSTAIN_RATE  + lane,    0,  31,  20, "Sustain Rate");
        }
    }

 protected:
    /// @brief Return the value of the attack parameter from the panel.
    ///
    /// @param channel the polyphonic channel to get the attack parameter for
    /// @param lane the processing lane to get the attack rate parameter for
    /// @returns the 8-bit attack parameter after applying CV modulations
    ///
    inline uint8_t getAttack(unsigned channel, unsigned lane) {
        const float param = params[PARAM_ATTACK + lane].getValue();
        const float cv = inputs[INPUT_ATTACK + lane].getPolyVoltage(channel);
        const float mod = std::numeric_limits<int8_t>::max() * cv / 10.f;
        // invert attack so it increases in time as it increase in value
        return 15 - clamp(param + mod, 0.f, 15.f);
    }

    /// @brief Return the value of the decay parameter from the panel.
    ///
    /// @param channel the polyphonic channel to get the decay parameter for
    /// @param lane the processing lane to get the decay rate parameter for
    /// @returns the 8-bit decay parameter after applying CV modulations
    ///
    inline uint8_t getDecay(unsigned channel, unsigned lane) {
        const float param = params[PARAM_DECAY + lane].getValue();
        const float cv = inputs[INPUT_DECAY + lane].getPolyVoltage(channel);
        const float mod = std::numeric_limits<int8_t>::max() * cv / 10.f;
        // invert decay so it increases in time as it increase in value
        return 7 - clamp(param + mod, 0.f, 7.f);
    }

    /// @brief Return the value of the sustain rate parameter from the panel.
    ///
    /// @param channel the polyphonic channel to get the sustain rate parameter for
    /// @param lane the processing lane to get the sustain rate parameter for
    /// @returns the 8-bit sustain rate parameter after applying CV modulations
    ///
    inline uint8_t getSustainRate(unsigned channel, unsigned lane) {
        const float param = params[PARAM_SUSTAIN_RATE + lane].getValue();
        const float cv = inputs[INPUT_SUSTAIN_RATE + lane].getPolyVoltage(channel);
        const float mod = std::numeric_limits<int8_t>::max() * cv / 10.f;
        // invert sustain rate so it increases in time as it increase in value
        return 31 - clamp(param + mod, 0.f, 31.f);
    }

    /// @brief Return the value of the sustain level parameter from the panel.
    ///
    /// @param channel the polyphonic channel to get the sustain level parameter for
    /// @param lane the processing lane to get the sustain level parameter for
    /// @returns the 8-bit sustain level parameter after applying CV modulations
    ///
    inline uint8_t getSustainLevel(unsigned channel, unsigned lane) {
        const float param = params[PARAM_SUSTAIN_LEVEL + lane].getValue();
        const float cv = inputs[INPUT_SUSTAIN_LEVEL + lane].getPolyVoltage(channel);
        const float mod = std::numeric_limits<int8_t>::max() * cv / 10.f;
        return clamp(param + mod, 0.f, 7.f);
    }

    /// @brief Return the value of the amplitude parameter from the panel.
    ///
    /// @param channel the polyphonic channel to get the amplitude parameter for
    /// @param lane the processing lane to get the amplitude parameter for
    /// @returns the 8-bit amplitude parameter after applying CV modulations
    ///
    inline int8_t getAmplitude(unsigned channel, unsigned lane) {
        const float param = params[PARAM_AMPLITUDE + lane].getValue();
        const float cv = inputs[INPUT_AMPLITUDE + lane].getPolyVoltage(channel);
        const float mod = std::numeric_limits<int8_t>::max() * cv / 10.f;
        static constexpr float MIN = std::numeric_limits<int8_t>::min();
        static constexpr float MAX = std::numeric_limits<int8_t>::max();
        return clamp(param + mod, MIN, MAX);
    }

    /// @brief Return true if the envelope for given lane and polyphony channel
    /// is being triggered.
    ///
    /// @param channel the polyphonic channel to get the trigger input for
    /// @param lane the processing lane to get the trigger input for
    /// @returns True if the given envelope generator is triggered
    ///
    inline bool getTrigger(unsigned channel, unsigned lane) {
        // get the trigger from the gate input
        const auto gateCV = rescale(inputs[INPUT_GATE + lane].getPolyVoltage(channel), 0.f, 2.f, 0.f, 1.f);
        const bool gate = gateTrigger[lane][channel].process(gateCV);
        // get the trigger from the re-trigger input
        const auto retrigCV = rescale(inputs[INPUT_RETRIG + lane].getPolyVoltage(channel), 0.f, 2.f, 0.f, 1.f);
        const bool retrig = retrigTrigger[lane][channel].process(retrigCV);
        // OR the two boolean values together
        return gate || retrig;
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param channel the polyphonic channel to process the CV inputs to
    /// @param lane the processing lane on the module to process
    ///
    inline void processChannel(unsigned channel, unsigned lane) {
        // cache the APU for this lane and channel
        Sony_S_DSP_ADSR& apu = apus[lane][channel];
        // set the ADSR parameters for this APU
        apu.setAttack(getAttack(channel, lane));
        apu.setDecay(getDecay(channel, lane));
        apu.setSustainRate(getSustainRate(channel, lane));
        apu.setSustainLevel(getSustainLevel(channel, lane));
        apu.setAmplitude(getAmplitude(channel, lane));
        // trigger this APU and process the output
        auto trigger = getTrigger(channel, lane);
        auto sample = apu.run(trigger, gateTrigger[lane][channel].state);
        outputs[OUTPUT_ENVELOPE + lane].setVoltage(10.f * sample / 128.f, channel);
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
        for (unsigned lane = 0; lane < LANES; lane++) {
            for (unsigned channel = 0; channel < channels; channel++)
                processChannel(channel, lane);
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for SPC700.
struct ChipS_SMP_ADSRWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit ChipS_SMP_ADSRWidget(ChipS_SMP_ADSR *module) {
        setModule(module);
        static constexpr auto panel = "res/S-SMP-ADSR-Light.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        for (unsigned i = 0; i < ChipS_SMP_ADSR::LANES; i++) {
            // Gate, Retrig, Output
            addInput(createInput<PJ301MPort>(Vec(20, 45 + 169 * i), module, ChipS_SMP_ADSR::INPUT_GATE + i));
            addInput(createInput<PJ301MPort>(Vec(20, 100 + 169 * i), module, ChipS_SMP_ADSR::INPUT_RETRIG + i));
            addOutput(createOutput<PJ301MPort>(Vec(20, 156 + 169 * i), module, ChipS_SMP_ADSR::OUTPUT_ENVELOPE + i));
            // Amplitude
            auto amplitude = createParam<BefacoSlidePot>(Vec(66, 22 + 169 * i), module, ChipS_SMP_ADSR::PARAM_AMPLITUDE + i);
            amplitude->snap = true;
            addParam(amplitude);
            addInput(createInput<PJ301MPort>(Vec(61, 157 + 169 * i), module, ChipS_SMP_ADSR::INPUT_AMPLITUDE + i));
            // Attack
            auto attack = createParam<BefacoSlidePot>(Vec(100, 22 + 169 * i), module, ChipS_SMP_ADSR::PARAM_ATTACK + i);
            attack->snap = true;
            addParam(attack);
            addInput(createInput<PJ301MPort>(Vec(95, 157 + 169 * i), module, ChipS_SMP_ADSR::INPUT_ATTACK + i));
            // Decay
            auto decay = createParam<BefacoSlidePot>(Vec(134, 22 + 169 * i), module, ChipS_SMP_ADSR::PARAM_DECAY + i);
            decay->snap = true;
            addParam(decay);
            addInput(createInput<PJ301MPort>(Vec(129, 157 + 169 * i), module, ChipS_SMP_ADSR::INPUT_DECAY + i));
            // Sustain Level
            auto sustainLevel = createParam<BefacoSlidePot>(Vec(168, 22 + 169 * i), module, ChipS_SMP_ADSR::PARAM_SUSTAIN_LEVEL + i);
            sustainLevel->snap = true;
            addParam(sustainLevel);
            addInput(createInput<PJ301MPort>(Vec(163, 157 + 169 * i), module, ChipS_SMP_ADSR::INPUT_SUSTAIN_LEVEL + i));
            // Sustain Rate
            auto sustainRate = createParam<BefacoSlidePot>(Vec(202, 22 + 169 * i), module, ChipS_SMP_ADSR::PARAM_SUSTAIN_RATE + i);
            sustainRate->snap = true;
            addParam(sustainRate);
            addInput(createInput<PJ301MPort>(Vec(197, 157 + 169 * i), module, ChipS_SMP_ADSR::INPUT_SUSTAIN_RATE + i));
        }
    }
};

/// the global instance of the model
rack::Model *modelChipS_SMP_ADSR = createModel<ChipS_SMP_ADSR, ChipS_SMP_ADSRWidget>("S_SMP_ADSR");
