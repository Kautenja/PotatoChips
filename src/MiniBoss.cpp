// A Eurorack module based on a Yamaha YM2612 chip emulation.
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

#include <functional>
#include "plugin.hpp"
#include "dsp/yamaha_ym2612/voice1op.hpp"
#include "widget/indexed_frame_display.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Eurorack module based on the Yamaha YM2612.
struct MiniBoss : rack::Module {
 private:
    /// a YM2612 chip emulator
    YamahaYM2612::Voice1Op apu[PORT_MAX_CHANNELS];

    /// triggers for opening and closing the oscillator gates
    dsp::BooleanTrigger gate_triggers[YamahaYM2612::Voice1Op::NUM_OPERATORS][PORT_MAX_CHANNELS];
    /// triggers for handling input re-trigger signals
    rack::dsp::BooleanTrigger retrig_triggers[YamahaYM2612::Voice1Op::NUM_OPERATORS][PORT_MAX_CHANNELS];

    /// a clock divider for reducing computation (on CV acquisition)
    dsp::ClockDivider cvDivider;

    /// a VU meter for measuring the output audio level from the emulator
    dsp::VuMeter2 vuMeter;
    /// a light divider for updating the LEDs every 512 processing steps
    dsp::ClockDivider lightDivider;

    /// Return the binary value for the given parameter.
    ///
    /// @param channel the channel to get the parameter value for
    /// @param paramIndex the index of the parameter in the params list
    /// @param inputIndex the index of the CV input in the inputs list
    /// @param int max the maximal value for the parameter
    ///
    inline uint8_t getParam(
        unsigned channel,
        unsigned paramIndex,
        unsigned inputIndex,
        unsigned max
    ) {
        auto param = params[paramIndex].getValue();
        auto cv = max * inputs[inputIndex].getVoltage(channel) / 10.0f;
        return clamp(static_cast<int>(param + cv), 0, max);
    }

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        PARAM_AL,
        PARAM_FB,
        PARAM_LFO,
        PARAM_SATURATION,
        ENUMS(PARAM_AR,         YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(PARAM_TL,         YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(PARAM_D1,         YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(PARAM_SL,         YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(PARAM_D2,         YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(PARAM_RR,         YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(PARAM_FREQ,       YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(PARAM_MUL,        YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(PARAM_AMS,        YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(PARAM_FMS,        YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(PARAM_RS,         YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(PARAM_SSG_ENABLE, YamahaYM2612::Voice1Op::NUM_OPERATORS),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        INPUT_AL,
        INPUT_FB,
        INPUT_LFO,
        INPUT_SATURATION,
        // Row 1
        ENUMS(INPUT_AR,         YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(INPUT_TL,         YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(INPUT_D1,         YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(INPUT_SL,         YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(INPUT_D2,         YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(INPUT_RR,         YamahaYM2612::Voice1Op::NUM_OPERATORS),
        // Row 2
        ENUMS(INPUT_GATE,       YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(INPUT_RETRIG,     YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(INPUT_PITCH,      YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(INPUT_MUL,        YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(INPUT_AMS,        YamahaYM2612::Voice1Op::NUM_OPERATORS),
        ENUMS(INPUT_FMS,        YamahaYM2612::Voice1Op::NUM_OPERATORS),
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_MASTER, 2),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(VU_LIGHTS, 6),
        NUM_LIGHTS
    };

    /// the current FM algorithm
    uint8_t algorithm[PORT_MAX_CHANNELS] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

    /// Initialize a new Boss Fight module.
    MiniBoss() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        // global parameters
        configParam(PARAM_AL,  0, 7, 7, "Algorithm");
        configParam(PARAM_FB,  0, 7, 0, "Feedback");
        configParam(PARAM_LFO, 0, 7, 0, "LFO frequency");
        configParam(PARAM_SATURATION, 0, 127, 127, "Output Saturation");
        configParam(PARAM_FREQ, -5.f, 5.f, 0.f, "Frequency", " Hz", 2, dsp::FREQ_C4);
        // operator parameters
        configParam(PARAM_AR,  1,  31,  31, "Attack Rate");
        configParam(PARAM_TL,  0, 100, 100, "Total Level");
        configParam(PARAM_D1,  0,  31,   0, "1st Decay Rate");
        configParam(PARAM_SL,  0,  15,  15, "Sustain Level");
        configParam(PARAM_D2,  0,  31,   0, "2nd Decay Rate");
        configParam(PARAM_RR,  0,  15,  15, "Release Rate");
        configParam(PARAM_MUL, 0,  15,   1, "Multiplier");
        configParam(PARAM_RS,  0,   3,   0, "Rate Scaling");
        configParam(PARAM_AMS, 0,   3,   0, "Amplitude modulation sensitivity");
        configParam(PARAM_FMS, 0,   7,   0, "Frequency modulation sensitivity");
        configParam<BooleanParamQuantity>(PARAM_SSG_ENABLE, 0, 1, 0, "Looping Envelope");
        // reset the emulator
        onSampleRateChange();
        // set the rate of the CV acquisition clock divider
        cvDivider.setDivision(16);
        lightDivider.setDivision(512);
    }

    /// @brief Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() final {
        // update the buffer for each oscillator and polyphony channel
        for (unsigned channel = 0; channel < PORT_MAX_CHANNELS; channel++)
            apu[channel].set_sample_rate(APP->engine->getSampleRate(), CLOCK_RATE);
    }

    /// @brief Return the value of the mix parameter from the panel.
    ///
    /// @returns the 8-bit saturation value
    ///
    inline int32_t getSaturation(unsigned channel) {
        const float param = params[PARAM_SATURATION].getValue();
        const float cv = inputs[INPUT_SATURATION].getPolyVoltage(channel) / 10.f;
        const float mod = std::numeric_limits<int8_t>::max() * cv;
        static constexpr float MAX = std::numeric_limits<int8_t>::max();
        return clamp(param + mod, 0.f, MAX);
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processCV(const ProcessArgs &args, unsigned channel) {
        // this value is used in the algorithm widget
        algorithm[channel] = params[PARAM_AL].getValue() + inputs[INPUT_AL].getVoltage(channel);
        algorithm[channel] = clamp(algorithm[channel], 0, 7);
        apu[channel].set_lfo(getParam(0, PARAM_LFO, INPUT_LFO, 7));
        // set the global parameters
        apu[channel].set_feedback(getParam(channel, PARAM_FB,  INPUT_FB,  7));
        // normal pitch gate and re-trigger
        float gate = 0;
        float retrig = 0;
        // set the operator parameters
        for (unsigned op = 0; op < YamahaYM2612::Voice1Op::NUM_OPERATORS; op++) {
            apu[channel].set_attack_rate   (op, getParam(channel, PARAM_AR         + op, INPUT_AR         + op, 31 ));
            apu[channel].set_total_level   (op, 100 - getParam(channel, PARAM_TL   + op, INPUT_TL         + op, 100));
            apu[channel].set_decay_rate    (op, getParam(channel, PARAM_D1         + op, INPUT_D1         + op, 31 ));
            apu[channel].set_sustain_level (op, 15 - getParam(channel, PARAM_SL    + op, INPUT_SL         + op, 15 ));
            apu[channel].set_sustain_rate  (op, getParam(channel, PARAM_D2         + op, INPUT_D2         + op, 31 ));
            apu[channel].set_release_rate  (op, getParam(channel, PARAM_RR         + op, INPUT_RR         + op, 15 ));
            apu[channel].set_multiplier    (op, getParam(channel, PARAM_MUL        + op, INPUT_MUL        + op, 15 ));
            apu[channel].set_fm_sensitivity(op, getParam(channel, PARAM_FMS        + op, INPUT_FMS        + op, 7  ));
            apu[channel].set_am_sensitivity(op, getParam(channel, PARAM_AMS        + op, INPUT_AMS        + op, 4  ));
            // SSG and rate scale
            apu[channel].set_ssg_enabled(op, params[PARAM_SSG_ENABLE + op].getValue());
            apu[channel].set_rate_scale(op, params[PARAM_RS + op].getValue());
            // process the gate trigger, high at 2V
            gate = inputs[INPUT_GATE + op].getNormalVoltage(gate, channel);
            gate_triggers[op][channel].process(rescale(gate, 0.f, 2.f, 0.f, 1.f));
            // process the retrig trigger, high at 2V
            retrig = inputs[INPUT_RETRIG + op].getNormalVoltage(retrig, channel);
            const auto trigger = retrig_triggers[op][channel].process(rescale(retrig, 0.f, 2.f, 0.f, 1.f));
            // use the exclusive or of the gate and retrigger. This ensures that
            // when either gate or trigger alone is high, the gate is open,
            // but when neither or both are high, the gate is closed. This
            // causes the gate to get shut for a sample when re-triggering an
            // already gated voice
            apu[channel].set_gate(op, trigger ^ gate_triggers[op][channel].state);
        }
    }

    /// @brief Process a sample.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
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
        // process control voltage when the CV divider is high
        if (cvDivider.process())
            for (unsigned channel = 0; channel < channels; channel++)
                processCV(args, channel);
        // set the operator parameters
        for (unsigned channel = 0; channel < channels; channel++) {
            float pitch = 0;
            for (unsigned op = 0; op < YamahaYM2612::Voice1Op::NUM_OPERATORS; op++) {
                float frequency = params[PARAM_FREQ + op].getValue();
                pitch = inputs[INPUT_PITCH + op].getNormalVoltage(pitch, channel);
                apu[channel].set_frequency(op, dsp::FREQ_C4 * std::pow(2.f, clamp(frequency + pitch, -6.5f, 6.5f)));
            }
        }
        // advance one sample in the emulator
        for (unsigned channel = 0; channel < channels; channel++) {
            // set the output voltage based on the 14-bit signed PCM sample
            const int16_t audio_output = (apu[channel].step() * getSaturation(channel)) >> 7;
            // update the VU meter before clipping to more accurately detect it
            vuMeter.process(args.sampleTime, audio_output / static_cast<float>(1 << 13));
            // convert the clipped audio to a floating point sample and set
            // the output voltage for the channel
            const auto sample = YamahaYM2612::Voice1Op::clip(audio_output) / static_cast<float>(1 << 13);
            outputs[OUTPUT_MASTER + 0].setVoltage(5.f * sample, channel);
            outputs[OUTPUT_MASTER + 1].setVoltage(5.f * sample, channel);
        }
        // process the lights based on the VU meter readings
        if (lightDivider.process()) {
            lights[VU_LIGHTS + 0].setBrightness(vuMeter.getBrightness(3, 6));
            lights[VU_LIGHTS + 1].setBrightness(vuMeter.getBrightness(0, 3));
            lights[VU_LIGHTS + 2].setBrightness(vuMeter.getBrightness(-3, 0));
            lights[VU_LIGHTS + 3].setBrightness(vuMeter.getBrightness(-6, -3));
            lights[VU_LIGHTS + 4].setBrightness(vuMeter.getBrightness(-12, -6));
            lights[VU_LIGHTS + 5].setBrightness(vuMeter.getBrightness(-24, -12));
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for MiniBoss.
struct MiniBossWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit MiniBossWidget(MiniBoss *module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, "res/BossFight.svg")));
        // Panel Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // Algorithm, Feedback, LFO, Saturation
        addParam(createSnapParam<Rogan3PWhite>(Vec(77, 116),  module, MiniBoss::PARAM_FB));
        addParam(createSnapParam<Rogan3PWhite>(Vec(10, 187), module, MiniBoss::PARAM_LFO));
        addParam(createSnapParam<Rogan3PWhite>(Vec(77, 187), module, MiniBoss::PARAM_SATURATION));
        // Saturation Indicator
        addChild(createLightCentered<MediumLight<RedLight>>   (Vec(20, 270), module, MiniBoss::VU_LIGHTS + 0));
        addChild(createLightCentered<MediumLight<RedLight>>   (Vec(20, 285), module, MiniBoss::VU_LIGHTS + 1));
        addChild(createLightCentered<MediumLight<YellowLight>>(Vec(20, 300), module, MiniBoss::VU_LIGHTS + 2));
        addChild(createLightCentered<MediumLight<YellowLight>>(Vec(20, 315), module, MiniBoss::VU_LIGHTS + 3));
        addChild(createLightCentered<MediumLight<GreenLight>> (Vec(20, 330), module, MiniBoss::VU_LIGHTS + 4));
        addChild(createLightCentered<MediumLight<GreenLight>> (Vec(20, 345), module, MiniBoss::VU_LIGHTS + 5));
        // Global Ports
        addInput(createInput<PJ301MPort>  (Vec(63, 249), module, MiniBoss::INPUT_AL));
        addInput(createInput<PJ301MPort>  (Vec(98, 249), module, MiniBoss::INPUT_FB));
        addInput(createInput<PJ301MPort>  (Vec(63, 293), module, MiniBoss::INPUT_LFO));
        addInput(createInput<PJ301MPort>  (Vec(98, 293), module, MiniBoss::INPUT_SATURATION));
        addOutput(createOutput<PJ301MPort>(Vec(63, 337), module, MiniBoss::OUTPUT_MASTER + 0));
        addOutput(createOutput<PJ301MPort>(Vec(98, 337), module, MiniBoss::OUTPUT_MASTER + 1));
        // ADSR
        addParam(createSnapParam<Rogan2PWhite>(Vec(159, 35),  module, MiniBoss::PARAM_AR));
        addParam(createSnapParam<Rogan2PWhite>(Vec(223, 60),  module, MiniBoss::PARAM_TL));
        addParam(createSnapParam<Rogan2PWhite>(Vec(159, 103), module, MiniBoss::PARAM_D1));
        addParam(createSnapParam<Rogan2PWhite>(Vec(223, 147), module, MiniBoss::PARAM_SL));
        addParam(createSnapParam<Rogan2PWhite>(Vec(159, 173), module, MiniBoss::PARAM_D2));
        addParam(createSnapParam<Rogan2PWhite>(Vec(159, 242), module, MiniBoss::PARAM_RR));
        // Looping ADSR, Key Scaling
        addParam(createParam<CKSS>(Vec(216, 203), module, MiniBoss::PARAM_SSG_ENABLE));
        addParam(createSnapParam<Trimpot>(Vec(248, 247), module, MiniBoss::PARAM_RS));
        // Frequency and modulation
        addParam(createParam<Rogan2PWhite>(Vec(290, 35),  module, MiniBoss::PARAM_FREQ));
        addParam(createSnapParam<Rogan2PWhite>(Vec(290, 103), module, MiniBoss::PARAM_MUL));
        addParam(createSnapParam<Rogan2PWhite>(Vec(290, 173), module, MiniBoss::PARAM_AMS));
        addParam(createSnapParam<Rogan2PWhite>(Vec(290, 242), module, MiniBoss::PARAM_FMS));
        // Input Ports
        for (unsigned j = 0; j < 6; j++) {
            const auto x = 140 + j * 35;
            addInput(createInput<PJ301MPort>(Vec(x, 295), module, MiniBoss::INPUT_AR + j));
            addInput(createInput<PJ301MPort>(Vec(x, 339), module, MiniBoss::INPUT_GATE + j));
        }
    }
};

Model *modelMiniBoss = createModel<MiniBoss, MiniBossWidget>("MiniBoss");
