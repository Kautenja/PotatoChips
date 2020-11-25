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
    /// a YM2612 operator 1 emulator
    YamahaYM2612::Voice1Op apu[PORT_MAX_CHANNELS];

    /// triggers for opening and closing the oscillator gates
    dsp::BooleanTrigger gate_triggers[PORT_MAX_CHANNELS];
    /// triggers for handling input re-trigger signals
    rack::dsp::BooleanTrigger retrig_triggers[PORT_MAX_CHANNELS];

    /// a clock divider for reducing computation (on CV acquisition)
    dsp::ClockDivider cvDivider;

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
        // envelope generator
        PARAM_AR,
        PARAM_TL,
        PARAM_D1,
        PARAM_SL,
        PARAM_D2,
        PARAM_RR,
        PARAM_SSG_ENABLE,
        PARAM_RS,
        // row 1
        PARAM_FREQ,
        PARAM_LFO,
        PARAM_FMS,
        PARAM_AMS,
        // row 2
        PARAM_FM,
        PARAM_MUL,
        PARAM_FB,
        PARAM_VOLUME,
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        // row 1
        INPUT_AR,
        INPUT_TL,
        INPUT_D1,
        INPUT_SL,
        INPUT_D2,
        INPUT_RR,
        // row 2
        INPUT_GATE,
        INPUT_RETRIG,
        INPUT_VOCT,
        INPUT_FM,
        INPUT_VOLUME,
        // TODO: remove?
        INPUT_FB,
        INPUT_LFO,
        INPUT_MUL,
        INPUT_AMS,
        INPUT_FMS,
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        OUTPUT_OSC,
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(LIGHT_AR, 3),
        ENUMS(LIGHT_TL, 3),
        ENUMS(LIGHT_D1, 3),
        ENUMS(LIGHT_SL, 3),
        ENUMS(LIGHT_D2, 3),
        ENUMS(LIGHT_RR, 3),
        NUM_LIGHTS
    };

    /// Initialize a new Boss Fight module.
    MiniBoss() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        // global parameters
        configParam(PARAM_FB,  0, 7, 0, "Feedback");
        configParam(PARAM_LFO, 0, 7, 0, "LFO frequency");
        configParam(PARAM_VOLUME, 0, 127, 127, "Output Volume");
        configParam(PARAM_FREQ, -5.f, 5.f, 0.f, "Frequency", " Hz", 2, dsp::FREQ_C4);
        configParam(PARAM_FM, -1, 1, 0, "Frequency Modulation");
        // operator parameters
        configParam(PARAM_AR,  1,  31,  31, "Attack Rate");
        configParam(PARAM_TL,  0, 100, 100, "Total Level");
        configParam(PARAM_D1,  0,  31,   0, "Decay Rate");
        configParam(PARAM_SL,  0,  15,  15, "Sustain Level");
        configParam(PARAM_D2,  0,  31,   0, "Sustain Rate");
        configParam(PARAM_RR,  0,  15,  15, "Release Rate");
        configParam(PARAM_MUL, 0,  15,   1, "Multiplier");
        configParam(PARAM_RS,  0,   3,   0, "Rate Scaling");
        configParam(PARAM_AMS, 0,   3,   0, "LFO amplitude modulation sensitivity");
        configParam(PARAM_FMS, 0,   7,   0, "LFO frequency modulation sensitivity");
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
    inline int32_t getVolume(unsigned channel) {
        const float param = params[PARAM_VOLUME].getValue();
        const float cv = inputs[INPUT_VOLUME].getPolyVoltage(channel) / 10.f;
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
        apu[channel].set_lfo           (getParam(channel, PARAM_LFO, INPUT_LFO, 7));
        apu[channel].set_feedback      (getParam(channel, PARAM_FB,  INPUT_FB,  7));
        apu[channel].set_attack_rate   (getParam(channel,       PARAM_AR,  INPUT_AR,  31 ));
        apu[channel].set_total_level   (100 - getParam(channel, PARAM_TL,  INPUT_TL,  100));
        apu[channel].set_decay_rate    (getParam(channel,       PARAM_D1,  INPUT_D1,  31 ));
        apu[channel].set_sustain_level (15 - getParam(channel,  PARAM_SL,  INPUT_SL,  15 ));
        apu[channel].set_sustain_rate  (getParam(channel,       PARAM_D2,  INPUT_D2,  31 ));
        apu[channel].set_release_rate  (getParam(channel,       PARAM_RR,  INPUT_RR,  15 ));
        apu[channel].set_multiplier    (getParam(channel,       PARAM_MUL, INPUT_MUL, 15 ));
        apu[channel].set_fm_sensitivity(getParam(channel,       PARAM_FMS, INPUT_FMS, 7  ));
        apu[channel].set_am_sensitivity(getParam(channel,       PARAM_AMS, INPUT_AMS, 4  ));
        apu[channel].set_ssg_enabled   (params[PARAM_SSG_ENABLE].getValue());
        apu[channel].set_rate_scale    (params[PARAM_RS].getValue());
        // process the gate trigger, high at 2V
        gate_triggers[channel].process(rescale(inputs[INPUT_GATE].getVoltage(channel), 0.f, 2.f, 0.f, 1.f));
        // process the retrig trigger, high at 2V
        const auto trigger = retrig_triggers[channel].process(rescale(inputs[INPUT_RETRIG].getVoltage(channel), 0.f, 2.f, 0.f, 1.f));
        // use the exclusive or of the gate and retrigger. This ensures that
        // when either gate or trigger alone is high, the gate is open,
        // but when neither or both are high, the gate is closed. This
        // causes the gate to get shut for a sample when re-triggering an
        // already gated voice
        apu[channel].set_gate(trigger ^ gate_triggers[channel].state);
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
            const float frequency = params[PARAM_FREQ].getValue();
            const float pitch = inputs[INPUT_VOCT].getVoltage(channel);
            apu[channel].set_frequency(dsp::FREQ_C4 * std::pow(2.f, clamp(frequency + pitch, -6.5f, 6.5f)));
        }
        // advance one sample in the emulator
        for (unsigned channel = 0; channel < channels; channel++) {
            // get the FM signal as a 14-bit signed sample
            const float fm = (1 << 13) * clamp(params[PARAM_FM].getValue() * inputs[INPUT_FM].getVoltage(channel) / 5.0, -1.f, 1.f);
            // set the output voltage based on the 14-bit signed sample
            const int16_t audio_output = (apu[channel].step(fm) * getVolume(channel)) >> 7;
            // convert the clipped audio to a floating point sample and set the
            // output voltage for the channel
            const auto sample = YamahaYM2612::Operator::clip(audio_output) / static_cast<float>(1 << 13);
            outputs[OUTPUT_OSC].setVoltage(5.f * sample, channel);
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
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, "res/MiniBoss.svg")));
        // Panel Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // ADSR
        for (unsigned i = 0; i < 6; i++) {
            auto slider = createLightParam<LEDLightSlider<RedGreenBlueLight>>(Vec(7 + 33 * i, 41), module, MiniBoss::PARAM_AR + i, MiniBoss::LIGHT_AR + 3 * i);
            slider->snap = true;
            addParam(slider);
        }
        // Looping ADSR, Key Scaling
        addParam(createParam<CKSS>(Vec(209, 43), module, MiniBoss::PARAM_SSG_ENABLE));
        addParam(createSnapParam<Trimpot>(Vec(208, 98), module, MiniBoss::PARAM_RS));
        // Frequency, Multiplier, FM, LFO, Volume
        const unsigned KNOB_PER_ROW = 4;
        for (unsigned row = 0; row < 2; row++) {
            for (unsigned knob = 0; knob < KNOB_PER_ROW; knob++) {
                const auto position = Vec(13 + 60 * knob, 157 + 68 * row);
                // get the index of the parameter. there are 4 knobs per row
                const auto index = MiniBoss::PARAM_FREQ + KNOB_PER_ROW * row + knob;
                auto param = createParam<Rogan2PWhite>(position,  module, index);
                // knobs 2,3,4 on all rows are discrete. knob 1 is continuous
                param->snap = knob > 1;
                addParam(param);
            }
        }
        // ports
        for (unsigned j = 0; j < 6; j++) {
            const auto x = 13 + j * 37;
            addInput(createInput<PJ301MPort>(Vec(x, 288), module, MiniBoss::INPUT_AR + j));
            if (j >= 5) continue;
            addInput(createInput<PJ301MPort>(Vec(x, 331), module, MiniBoss::INPUT_GATE + j));
        }
        addOutput(createOutput<PJ301MPort>(Vec(198, 331), module, MiniBoss::OUTPUT_OSC));
    }
};

Model *modelMiniBoss = createModel<MiniBoss, MiniBossWidget>("MiniBoss");
