// A Yamaha YM2612 Chip module.
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
#include "dsp/yamaha_ym2612/voice4op.hpp"
#include "widget/indexed_frame_display.hpp"

// TODO: individual operator frequency
// TODO: individual operator triggering
// TODO: option to prevent clicks in context menu
// TODO: trim pots for inputs

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Yamaha YM2612 chip emulator module.
struct Chip2612 : rack::Module {
 private:
    /// the YM2612 chip emulator
    YamahaYM2612::Voice4Op apu[PORT_MAX_CHANNELS];

    /// triggers for opening and closing the oscillator gates
    dsp::BooleanTrigger gate_triggers[YamahaYM2612::Voice4Op::NUM_OPERATORS][PORT_MAX_CHANNELS];
    /// triggers for handling input re-trigger signals
    rack::dsp::BooleanTrigger retrig_triggers[YamahaYM2612::Voice4Op::NUM_OPERATORS][PORT_MAX_CHANNELS];

    /// a clock divider for reducing computation (on CV acquisition)
    dsp::ClockDivider cvDivider;

    dsp::VuMeter2 vuMeter;
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
        PARAM_AMS,
        PARAM_FMS,
        ENUMS(PARAM_AR,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(PARAM_TL,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(PARAM_D1,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(PARAM_SL,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(PARAM_D2,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(PARAM_RR,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(PARAM_MUL,        YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(PARAM_DET,        YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(PARAM_RS,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(PARAM_AM,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(PARAM_SSG_ENABLE, YamahaYM2612::Voice4Op::NUM_OPERATORS),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_PITCH,      YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(INPUT_GATE,       YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(INPUT_RETRIG,     YamahaYM2612::Voice4Op::NUM_OPERATORS),
        INPUT_AL,
        INPUT_FB,
        INPUT_LFO,
        INPUT_AMS,
        INPUT_FMS,
        ENUMS(INPUT_AR,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(INPUT_TL,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(INPUT_D1,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(INPUT_SL,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(INPUT_D2,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(INPUT_RR,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(INPUT_MUL,        YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(INPUT_DET,        YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(INPUT_RS,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(INPUT_AM,         YamahaYM2612::Voice4Op::NUM_OPERATORS),
        ENUMS(INPUT_SSG_ENABLE, YamahaYM2612::Voice4Op::NUM_OPERATORS),
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        OUTPUT_MASTER,
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(VU_LIGHTS, 6),
        NUM_LIGHTS
    };

    /// the current FM algorithm
    uint8_t algorithm[PORT_MAX_CHANNELS] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

    /// Initialize a new Yamaha YM2612 module.
    Chip2612() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        // global parameters
        configParam(PARAM_AL,  0, 7, 7, "Algorithm");
        configParam(PARAM_FB,  0, 7, 0, "Feedback");
        configParam(PARAM_LFO, 0, 7, 0, "LFO frequency");
        configParam(PARAM_AMS, 0, 3, 0, "Amplitude modulation sensitivity");
        configParam(PARAM_FMS, 0, 7, 0, "Frequency modulation sensitivity");
        for (unsigned i = 0; i < YamahaYM2612::Voice4Op::NUM_OPERATORS; i++) {  // operator parameters
            auto opName = "Operator " + std::to_string(i + 1);
            // total level is defined on the domain [0, 127], but values above
            // 70 cause the operator to drop below usable levels
            configParam(PARAM_AR         + i, 1, 31,  31, opName + " Attack Rate");
            configParam(PARAM_TL         + i, 0, 70,  0,  opName + " Total Level");
            configParam(PARAM_D1         + i, 0, 31,  0,  opName + " 1st Decay Rate");
            configParam(PARAM_SL         + i, 0, 15,  0,  opName + " Sustain Level");
            configParam(PARAM_D2         + i, 0, 31,  0,  opName + " 2nd Decay Rate");
            configParam(PARAM_RR         + i, 0, 15,  15, opName + " Release Rate");
            configParam(PARAM_MUL        + i, 0, 15,  1,  opName + " Multiplier");
            configParam(PARAM_DET        + i, 0, 7,   4,  opName + " Detune");
            configParam(PARAM_RS         + i, 0, 3,   0,  opName + " Rate Scaling");
            configParam(PARAM_AM         + i, 0, 1,   0,  opName + " Amplitude Modulation");
            configParam(PARAM_SSG_ENABLE + i, 0, 1,   0,  opName + " Looping Envelope Enable");
        }
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
        apu[channel].set_algorithm     (getParam(channel, PARAM_AL,  INPUT_AL,  7));
        apu[channel].set_feedback      (getParam(channel, PARAM_FB,  INPUT_FB,  7));
        apu[channel].set_am_sensitivity(getParam(channel, PARAM_AMS, INPUT_AMS, 3));
        apu[channel].set_fm_sensitivity(getParam(channel, PARAM_FMS, INPUT_FMS, 7));
        // normal pitch gate and re-trigger
        float pitch = 0;
        float gate = 0;
        float retrig = 0;
        // set the operator parameters
        for (unsigned op = 0; op < YamahaYM2612::Voice4Op::NUM_OPERATORS; op++) {
            apu[channel].set_attack_rate  (op, getParam(channel, PARAM_AR         + op, INPUT_AR         + op, 31 ));
            apu[channel].set_total_level  (op, getParam(channel, PARAM_TL         + op, INPUT_TL         + op, 70 ));
            apu[channel].set_decay_rate   (op, getParam(channel, PARAM_D1         + op, INPUT_D1         + op, 31 ));
            apu[channel].set_sustain_level(op, getParam(channel, PARAM_SL         + op, INPUT_SL         + op, 15 ));
            apu[channel].set_sustain_rate (op, getParam(channel, PARAM_D2         + op, INPUT_D2         + op, 31 ));
            apu[channel].set_release_rate (op, getParam(channel, PARAM_RR         + op, INPUT_RR         + op, 15 ));
            apu[channel].set_multiplier   (op, getParam(channel, PARAM_MUL        + op, INPUT_MUL        + op, 15 ));
            apu[channel].set_detune       (op, getParam(channel, PARAM_DET        + op, INPUT_DET        + op, 7  ));
            apu[channel].set_rate_scale   (op, getParam(channel, PARAM_RS         + op, INPUT_RS         + op, 3  ));
            apu[channel].set_am_enabled   (op, getParam(channel, PARAM_AM         + op, INPUT_AM         + op, 1  ));
            apu[channel].set_ssg_enabled  (op, getParam(channel, PARAM_SSG_ENABLE + op, INPUT_SSG_ENABLE + op, 1  ));
            // Compute the frequency from the pitch parameter and input.
            pitch = inputs[INPUT_PITCH + op].getNormalVoltage(pitch, channel);
            apu[channel].set_frequency(op, dsp::FREQ_C4 * std::pow(2.f, clamp(pitch, -6.5f, 6.5f)));
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
        // advance one sample in the emulator
        for (unsigned channel = 0; channel < channels; channel++) {
            // set the output voltage based on the 14-bit signed PCM sample
            int16_t audio_output = apu[channel].step();
            // update the VU meter before clipping to more accurately detect it
            vuMeter.process(args.sampleTime, audio_output / static_cast<float>(1 << 13));
            // clip the audio output to 14-bits
            if (audio_output > YamahaYM2612::Operator::OUTPUT_MAX)
                audio_output = YamahaYM2612::Operator::OUTPUT_MAX;
            else if (audio_output < YamahaYM2612::Operator::OUTPUT_MIN)
                audio_output = YamahaYM2612::Operator::OUTPUT_MIN;
            // convert the clipped audio to a floating point sample and set
            // the output voltage for the channel
            const auto sample = audio_output / static_cast<float>(1 << 13);
            outputs[OUTPUT_MASTER].setVoltage(5.f * sample, channel);
        }
        if (lightDivider.process()) {
            lights[VU_LIGHTS + 0].setBrightness(vuMeter.getBrightness(0, 0));
            lights[VU_LIGHTS + 1].setBrightness(vuMeter.getBrightness(-3, 0));
            lights[VU_LIGHTS + 2].setBrightness(vuMeter.getBrightness(-6, -3));
            lights[VU_LIGHTS + 3].setBrightness(vuMeter.getBrightness(-12, -6));
            lights[VU_LIGHTS + 4].setBrightness(vuMeter.getBrightness(-24, -12));
            lights[VU_LIGHTS + 5].setBrightness(vuMeter.getBrightness(-36, -24));
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for 2612.
struct Chip2612Widget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit Chip2612Widget(Chip2612 *module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, "res/2612.svg")));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // Frequency, Pitch, Gate, and Retrigger controls
        for (unsigned i = 0; i < YamahaYM2612::Voice4Op::NUM_OPERATORS; i++) {
            addInput(createInput<PJ301MPort>(Vec(16,  84 + 42 * i), module, Chip2612::INPUT_PITCH  + i));
            addInput(createInput<PJ301MPort>(Vec(61,  84 + 42 * i), module, Chip2612::INPUT_GATE   + i));
            addInput(createInput<PJ301MPort>(Vec(106, 84 + 42 * i), module, Chip2612::INPUT_RETRIG + i));
        }

        // algorithm display
        addChild(new IndexedFrameDisplay(
            [&]() {
                return this->module ? reinterpret_cast<Chip2612*>(this->module)->algorithm[0] : 0;
            },
            "res/2612algorithms/",
            YamahaYM2612::Voice4Op::NUM_ALGORITHMS,
            Vec(115, 20),
            Vec(110, 70)
        ));
        // Algorithm
        auto algo = createParam<Rogan3PBlue>(Vec(115, 113),  module, Chip2612::PARAM_AL);
        algo->snap = true;
        addParam(algo);
        addInput(createInput<PJ301MPort>( Vec(124, 171),  module, Chip2612::INPUT_AL));
        // Feedback
        auto feedback = createParam<Rogan3PBlue>(Vec(182, 113),  module, Chip2612::PARAM_FB);
        feedback->snap = true;
        addParam(feedback);
        addInput(createInput<PJ301MPort>( Vec(191, 171),  module, Chip2612::INPUT_FB));
        // LFO
        auto lfo = createParam<Rogan2PWhite>(Vec(187, 223), module, Chip2612::PARAM_LFO);
        lfo->snap = true;
        addParam(lfo);
        addInput(createInput<PJ301MPort>( Vec(124, 226),  module, Chip2612::INPUT_LFO));
        // Amplitude Modulation Sensitivity
        auto ams = createParam<Rogan2PWhite>(Vec(187, 279), module, Chip2612::PARAM_AMS);
        ams->snap = true;
        addParam(ams);
        addInput(createInput<PJ301MPort>(  Vec(124, 282), module, Chip2612::INPUT_AMS));
        // Frequency Modulation Sensitivity
        auto fms = createParam<Rogan2PWhite>(Vec(187, 335), module, Chip2612::PARAM_FMS);
        fms->snap = true;
        addParam(fms);
        addInput(createInput<PJ301MPort>(  Vec(124, 338), module, Chip2612::INPUT_FMS));
        // operator parameters and inputs
        for (unsigned i = 0; i < YamahaYM2612::Voice4Op::NUM_OPERATORS; i++) {
            // the X & Y offsets for the operator bank
            auto offsetX = 450 * (i % (YamahaYM2612::Voice4Op::NUM_OPERATORS / 2));
            auto offsetY = 175 * (i / (YamahaYM2612::Voice4Op::NUM_OPERATORS / 2));
            for (unsigned parameter = 0; parameter < 11; parameter++) {
                // the parameter & input offset
                auto offset = i + parameter * YamahaYM2612::Voice4Op::NUM_OPERATORS;
                auto param = createParam<BefacoSlidePot>(Vec(248 + offsetX + 34 * parameter, 25 + offsetY), module, Chip2612::PARAM_AR + offset);
                param->snap = true;
                addParam(param);
                addInput(createInput<PJ301MPort>(Vec(244 + offsetX + 34 * parameter, 160 + offsetY), module, Chip2612::INPUT_AR + offset));
            }
        }
        // left + right master outputs
        addOutput(createOutput<PJ301MPort>(Vec(26, 325), module, Chip2612::OUTPUT_MASTER));

        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(6.7, 34.758)), module, Chip2612::VU_LIGHTS + 0));
        addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(6.7, 39.884)), module, Chip2612::VU_LIGHTS + 1));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(6.7, 45.009)), module, Chip2612::VU_LIGHTS + 2));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(6.7, 50.134)), module, Chip2612::VU_LIGHTS + 3));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(6.7, 55.259)), module, Chip2612::VU_LIGHTS + 4));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(6.7, 60.384)), module, Chip2612::VU_LIGHTS + 5));
    }
};

Model *modelChip2612 = createModel<Chip2612, Chip2612Widget>("2612");
