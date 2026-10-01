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
#include "dsp/math.hpp"
#include "dsp/eurorack.hpp"
#include "dsp/trigger_divider.hpp"
#include "dsp/trigger_threshold.hpp"
#include "dsp/sony_s_dsp/adsr.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// An envelope generator module based on the S-SMP chip from Nintendo SNES.
struct SuperADSR : Module {
    /// the number of processing lanes on the module
    static constexpr unsigned LANES = 2;

 private:
    /// the Sony S-DSP ADSR enveloper generator emulator
    SonyS_DSP::ADSR apus[LANES][PORT_MAX_CHANNELS];
    /// triggers for handling input trigger and gate signals
    Trigger::Threshold gateTrigger[LANES][PORT_MAX_CHANNELS];
    /// triggers for handling input re-trigger signals
    Trigger::Threshold retrigTrigger[LANES][PORT_MAX_CHANNELS];
    /// Rising edges retained until the next chip tick, including short pulses.
    bool pendingTrigger[LANES][PORT_MAX_CHANNELS] = {};
    /// Fraction of a 32 kHz tick; preserved across host sample-rate changes.
    double clockPhase = 0.;
    /// a clock divider for light updates
    Trigger::Divider lightDivider;

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
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_ENVELOPE, LANES),
        ENUMS(OUTPUT_INVERTED, LANES),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(LIGHT_AMPLITUDE,     3 * LANES),
        ENUMS(LIGHT_ATTACK,        3 * LANES),
        ENUMS(LIGHT_DECAY,         3 * LANES),
        ENUMS(LIGHT_SUSTAIN_LEVEL, 3 * LANES),
        ENUMS(LIGHT_SUSTAIN_RATE,  3 * LANES),
        NUM_LIGHTS
    };

    /// @brief Initialize a new S-DSP Chip module.
    SuperADSR() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned lane = 0; lane < LANES; lane++) {
            configParam(PARAM_AMPLITUDE     + lane, -128, 127, 127, "Amplitude");
            configParam(PARAM_ATTACK        + lane,    0,  15,  10, "Attack");
            configParam(PARAM_DECAY         + lane,    0,   7,   7, "Decay");
            configParam(PARAM_SUSTAIN_LEVEL + lane,    0,   7,   5, "Sustain Level", "%", 0, 100.f / 7.f);
            configParam(PARAM_SUSTAIN_RATE  + lane,    0,  31,  20, "Sustain Rate");
            getParamQuantity(PARAM_SUSTAIN_RATE + lane)->description =
                "SR: decay while Gate is high. Key-off uses a fixed release of at most 8 ms.";
            configInput(INPUT_GATE + lane, "Gate");
            configInput(INPUT_RETRIG + lane, "Retrig");
            configOutput(OUTPUT_ENVELOPE + lane, "Envelope");
            configOutput(OUTPUT_INVERTED + lane, "Inverted");
        }
        lightDivider.setDivision(512);
    }

 protected:
    /// @brief Return true if the envelope for given lane and polyphony channel
    /// is being triggered.
    ///
    /// @param channel the polyphonic channel to get the trigger input for
    /// @param lane the processing lane to get the trigger input for
    /// @returns True if the given envelope generator is triggered
    ///
    inline bool getTrigger(const unsigned& channel, const unsigned& lane) {
        // get the trigger from the gate input
        auto& gateInput = inputs[INPUT_GATE + lane];
        const float gateVoltage = channel < static_cast<unsigned>(gateInput.getChannels())
            ? gateInput.getVoltage(channel) : 0.f;
        // Compare voltage thresholds directly: rescale() can round 0.01 V
        // slightly above zero and prevent a held gate from rearming.
        const float gateCV = gateVoltage >= 2.f ? 1.f : gateVoltage <= 0.01f ? 0.f : 0.5f;
        const bool gate = gateTrigger[lane][channel].process(gateCV);
        // get the trigger from the re-trigger input
        auto& retrigInput = inputs[INPUT_RETRIG + lane];
        const float retrigVoltage = channel < static_cast<unsigned>(retrigInput.getChannels())
            ? retrigInput.getVoltage(channel) : 0.f;
        const float retrigCV = retrigVoltage >= 2.f ? 1.f : retrigVoltage <= 0.01f ? 0.f : 0.5f;
        const bool retrig = retrigTrigger[lane][channel].process(retrigCV);
        // OR the two boolean values together
        return gate || retrig;
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param channel the polyphonic channel to process the CV inputs to
    /// @param lane the processing lane on the module to process
    ///
    inline void processChannel(const unsigned& channel, const unsigned& lane) {
        // cache the APU for this lane and channel
        SonyS_DSP::ADSR& apu = apus[lane][channel];
        // set the ADSR parameters for this APU
        apu.setAttack(15 - params[PARAM_ATTACK + lane].getValue());
        apu.setDecay(7 - params[PARAM_DECAY + lane].getValue());
        apu.setSustainRate(31 - params[PARAM_SUSTAIN_RATE + lane].getValue());
        apu.setSustainLevel(params[PARAM_SUSTAIN_LEVEL + lane].getValue());
        apu.setAmplitude(params[PARAM_AMPLITUDE + lane].getValue());
        // trigger this APU and process the output
        const bool trigger = pendingTrigger[lane][channel];
        pendingTrigger[lane][channel] = false;
        const float sample = apu.run(trigger, gateTrigger[lane][channel].isHigh());
        const float voltage = Math::Eurorack::toDC(sample / 128.f);
        outputs[OUTPUT_ENVELOPE + lane].setVoltage(voltage, channel);
        outputs[OUTPUT_INVERTED + lane].setVoltage(-voltage, channel);
    }

    /// @brief Process the CV inputs for the given channel.
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
        // Detect every host-sample edge before advancing the slower chip clock.
        // Disconnected voices also see key-off so reconnecting cannot revive a
        // frozen envelope or stale high gate. All storage/work is bounded to 32.
        for (unsigned lane = 0; lane < LANES; lane++) {
            for (unsigned channel = 0; channel < PORT_MAX_CHANNELS; channel++)
                pendingTrigger[lane][channel] |= getTrigger(channel, lane);
        }
        clockPhase += double(SonyS_DSP::SAMPLE_RATE) / args.sampleRate;
        // Tolerate sub-picosecond rounding at rational clock boundaries.
        while (clockPhase >= 1. - 1e-9) {
            clockPhase = std::max(0., clockPhase - 1.);
            for (unsigned lane = 0; lane < LANES; lane++) {
                for (unsigned channel = 0; channel < PORT_MAX_CHANNELS; channel++)
                    processChannel(channel, lane);
            }
        }
        if (lightDivider.process()) {
            const float sample_time = lightDivider.getDivision() * args.sampleTime;
            for (unsigned lane = 0; lane < LANES; lane++) {
                // set amplitude light based on the output
                const float output = Math::Eurorack::fromDC(outputs[OUTPUT_ENVELOPE + lane].getVoltageSum() / channels);
                if (output > 0) {  // positive, green light
                    lights[LIGHT_AMPLITUDE + 3 * lane + 0].setSmoothBrightness(0, sample_time);
                    lights[LIGHT_AMPLITUDE + 3 * lane + 1].setSmoothBrightness(output, sample_time);
                    lights[LIGHT_AMPLITUDE + 3 * lane + 2].setSmoothBrightness(0, sample_time);
                } else {  // negative, red light
                    lights[LIGHT_AMPLITUDE + 3 * lane + 0].setSmoothBrightness(-output, sample_time);
                    lights[LIGHT_AMPLITUDE + 3 * lane + 1].setSmoothBrightness(0, sample_time);
                    lights[LIGHT_AMPLITUDE + 3 * lane + 2].setSmoothBrightness(0, sample_time);
                }
                // set stage lights based on active stage
                for (int i = 0; i < 3; i++) {
                    float active = 0;
                    for (unsigned channel = 0; channel < channels; channel++)
                        active += static_cast<int>(apus[lane][channel].getStage()) == i + 1;
                    active /= channels;
                    // const auto active = static_cast<int>(apus[lane][0].getStage()) == i + 1;
                    // go in BGR order so blue can be set first
                    lights[LIGHT_ATTACK + 3 * lane + 6 * i + 2].setSmoothBrightness(active, sample_time);
                    // zero out the value if polyphonic
                    if (channels > 1) active = 0;
                    lights[LIGHT_ATTACK + 3 * lane + 6 * i + 1].setSmoothBrightness(active, sample_time);
                    lights[LIGHT_ATTACK + 3 * lane + 6 * i + 0].setSmoothBrightness(active, sample_time);
                }
            }
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for SPC700.
struct SuperADSRWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit SuperADSRWidget(SuperADSR *module) {
        setModule(module);
        static constexpr auto panel = "res/SuperADSR.svg";
        setPanel(createThemedPanel(plugin_instance, panel));
        // panel screws
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        for (unsigned i = 0; i < SuperADSR::LANES; i++) {
            // Gate, Retrig, Output
            addInput(createInput<ThemedPJ301MPort>(Vec(20 + 84 * i, 281), module, SuperADSR::INPUT_GATE + i));
            addInput(createInput<ThemedPJ301MPort>(Vec(53 + 84 * i, 281), module, SuperADSR::INPUT_RETRIG + i));
            addOutput(createOutput<ThemedPJ301MPort>(Vec(20 + 84 * i, 324), module, SuperADSR::OUTPUT_ENVELOPE + i));
            addOutput(createOutput<ThemedPJ301MPort>(Vec(53 + 84 * i, 324), module, SuperADSR::OUTPUT_INVERTED + i));
            // Amplitude
            auto amplitude = createLightParam<LEDLightSlider<RedGreenBlueLight>>(Vec(12, 48 + 119 * i), module, SuperADSR::PARAM_AMPLITUDE + i, SuperADSR::LIGHT_AMPLITUDE + 3 * i);
            amplitude->snap = true;
            addParam(amplitude);
            // Attack
            auto attack = createLightParam<LEDLightSlider<RedGreenBlueLight>>(Vec(46, 48 + 119 * i), module, SuperADSR::PARAM_ATTACK + i, SuperADSR::LIGHT_ATTACK + 3 * i);
            attack->snap = true;
            addParam(attack);
            // Decay
            auto decay = createLightParam<LEDLightSlider<RedGreenBlueLight>>(Vec(80, 48 + 119 * i), module, SuperADSR::PARAM_DECAY + i, SuperADSR::LIGHT_DECAY + 3 * i);
            decay->snap = true;
            addParam(decay);
            // Sustain Level
            auto sustainLevel = createLightParam<LEDLightSlider<RedGreenBlueLight>>(Vec(114, 48 + 119 * i), module, SuperADSR::PARAM_SUSTAIN_LEVEL + i, SuperADSR::LIGHT_SUSTAIN_LEVEL + 3 * i);
            sustainLevel->snap = true;
            addParam(sustainLevel);
            // Sustain Rate
            auto sustainRate = createLightParam<LEDLightSlider<RedGreenBlueLight>>(Vec(148, 48 + 119 * i), module, SuperADSR::PARAM_SUSTAIN_RATE + i, SuperADSR::LIGHT_SUSTAIN_RATE + 3 * i);
            sustainRate->snap = true;
            addParam(sustainRate);
        }
    }
};

/// the global instance of the model
rack::Model *modelSuperADSR = createModel<SuperADSR, SuperADSRWidget>("SuperADSR");
