// A Eurorack FM operator module based on a Yamaha YM2612 chip emulation.
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
#include "dsp/triggers.hpp"
#include "dsp/yamaha_ym2612/feedback_operator.hpp"
#include "engine/yamaha_ym2612_params.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Eurorack FM operator module based on the Yamaha YM2612.
struct MiniBoss : rack::Module {
 private:
    /// a YM2612 operator 1 emulator
    YamahaYM2612::FeedbackOperator apu[PORT_MAX_CHANNELS];

    /// triggers for opening and closing the oscillator gates
    Trigger::Boolean gates[PORT_MAX_CHANNELS];
    /// triggers for handling input re-trigger signals
    Trigger::Boolean retriggers[PORT_MAX_CHANNELS];

    /// a clock divider for reducing computation (on CV acquisition)
    rack::dsp::ClockDivider cvDivider;
    /// a light divider for updating the LEDs every 512 processing steps
    rack::dsp::ClockDivider lightDivider;

    /// Return the binary value for the given parameter.
    ///
    /// @param channel the channel to get the parameter value for
    /// @param paramIndex the index of the parameter in the params list
    /// @param inputIndex the index of the CV input in the inputs list
    /// @param min the minimal value for the parameter
    /// @param max the maximal value for the parameter
    /// @returns the 8-bit value of the given parameter
    ///
    inline uint8_t getParam(
        unsigned channel,
        unsigned paramIndex,
        unsigned inputIndex,
        unsigned min,
        unsigned max
    ) {
        auto param = params[paramIndex].getValue();
        auto cv = max * inputs[inputIndex].getVoltage(channel) / 8.f;
        return clamp(static_cast<int>(param + cv), min, max);
    }

 public:
    /// Whether to attempt to prevent clicks from the envelope generator
    bool prevent_clicks = false;

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
        configParam<LFOQuantity>(PARAM_LFO, 0, 7, 0);
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
        configParam(PARAM_RS,  0,   3,   0, "Rate Scaling");
        configParam<BooleanParamQuantity>(PARAM_SSG_ENABLE, 0, 1, 0, "Looping Envelope");
        configParam<MultiplierQuantity>(PARAM_MUL, 0, 15, 1);
        configParam<AMSQuantity>(PARAM_AMS, 0, 3, 0);
        configParam<FMSQuantity>(PARAM_FMS, 0, 7, 0);
        // reset the emulator
        onSampleRateChange();
        // set the rate of the CV acquisition clock divider
        cvDivider.setDivision(16);
        lightDivider.setDivision(512);
    }

    /// @brief Respond to the change of sample rate in the engine.
    void onSampleRateChange() final {
        // update the buffer for each oscillator and polyphony channel
        for (unsigned ch = 0; ch < PORT_MAX_CHANNELS; ch++)
            apu[ch].set_sample_rate(APP->engine->getSampleRate(), CLOCK_RATE);
    }

    /// @brief Respond to the module being reset by the engine.
    void onReset() override {
        prevent_clicks = false;
    }

    /// @brief Return a JSON representation of this module's state
    ///
    /// @returns a new JSON object with this object's serialized state data
    ///
    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "prevent_clicks", json_boolean(prevent_clicks));
        return rootJ;
    }

    /// @brief Return the object to the given serialized state.
    ///
    /// @returns a JSON object with object serialized state data to restore
    ///
    void dataFromJson(json_t* rootJ) override {
        json_t* prevent_clicks_object = json_object_get(rootJ, "prevent_clicks");
        if (prevent_clicks_object)
            prevent_clicks = json_boolean_value(prevent_clicks_object);
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

    /// @brief Process the gate trigger, high at 2V.
    ///
    /// @param channel the polyphonic channel to get the gate of
    /// @returns true if the gate is high, false otherwise
    ///
    inline bool getGate(unsigned channel) {
        const auto input = inputs[INPUT_GATE].getVoltage(channel);
        gates[channel].process(rescale(input, 0.f, 2.f, 0.f, 1.f));
        return gates[channel].isHigh();
    }

    /// @brief Process the re-trig trigger, high at 2V.
    ///
    /// @param channel the polyphonic channel to get the re-trigger of
    /// @returns true if the channel is being re-triggered
    ///
    inline bool getRetrigger(unsigned channel) {
        const auto input = inputs[INPUT_RETRIG].getVoltage(channel);
        return retriggers[channel].process(rescale(input, 0.f, 2.f, 0.f, 1.f));
    }

    /// @brief Return the frequency for the given channel.
    ///
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the floating point frequency
    ///
    inline float getFrequency(unsigned channel) {
        const float base = params[PARAM_FREQ].getValue();
        const float voct = inputs[INPUT_VOCT].getVoltage(channel);
        return dsp::FREQ_C4 * std::pow(2.f, clamp(base + voct, -6.5f, 6.5f));
    }

    /// @brief Return the frequency mod for the given channel.
    ///
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 14-bit aigned frequency modulation signal
    ///
    inline int16_t getFM(unsigned channel) {
        const auto input = inputs[INPUT_FM].getVoltage(channel) / 5.0;
        const auto depth = params[PARAM_FM].getValue();
        return (1 << 13) * clamp(depth * input, -1.f, 1.f);
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
        if (cvDivider.process()) {
            for (unsigned channel = 0; channel < channels; channel++) {
                apu[channel].set_ar            (getParam(channel,       PARAM_AR,  INPUT_AR, 1, 31 ));
                apu[channel].set_tl            (100 - getParam(channel, PARAM_TL,  INPUT_TL, 0, 100));
                apu[channel].set_dr            (getParam(channel,       PARAM_D1,  INPUT_D1, 0, 31 ));
                apu[channel].set_sl            (15 - getParam(channel,  PARAM_SL,  INPUT_SL, 0, 15 ));
                apu[channel].set_sr            (getParam(channel,       PARAM_D2,  INPUT_D2, 0, 31 ));
                apu[channel].set_rr            (getParam(channel,       PARAM_RR,  INPUT_RR, 0, 15 ));
                apu[channel].set_multiplier    (params[PARAM_MUL].getValue());
                apu[channel].set_feedback      (params[PARAM_FB].getValue());
                apu[channel].set_lfo           (params[PARAM_LFO].getValue());
                apu[channel].set_fm_sensitivity(params[PARAM_FMS].getValue());
                apu[channel].set_am_sensitivity(params[PARAM_AMS].getValue());
                apu[channel].set_ssg_enabled   (params[PARAM_SSG_ENABLE].getValue());
                apu[channel].set_rs            (params[PARAM_RS].getValue());
                // use the exclusive or of the gate and re-trigger. This ensures
                // that when either gate or trigger alone is high, the gate is
                // open, but when neither or both are high, the gate is closed.
                // This causes the gate to get shut for a sample when
                // re-triggering an already gated voice
                apu[channel].set_gate(getGate(channel) ^ getRetrigger(channel), prevent_clicks);
            }
        }
        // set the operator parameters
        for (unsigned channel = 0; channel < channels; channel++) {
            apu[channel].set_frequency(getFrequency(channel));
            // set the output voltage based on the 14-bit signed sample
            const int16_t audio_output = (apu[channel].step(getFM(channel)) * getVolume(channel)) >> 7;
            // convert the clipped audio to a floating point sample and set the
            // output voltage for the channel
            const auto sample = YamahaYM2612::Operator::clip(audio_output) / static_cast<float>(1 << 13);
            outputs[OUTPUT_OSC].setVoltage(5.f * sample, channel);
        }
        if (lightDivider.process()) {
            const auto sample_time = lightDivider.getDivision() * args.sampleTime;
            for (unsigned param = 0; param < 6; param++) {
                // get the scaled CV
                float value = 0.f;
                if (channels > 1) {  // polyphonic (average)
                    for (unsigned c = 0; c < channels; c++)
                        value += inputs[INPUT_AR + param].getVoltage(c);
                    value = value / channels;
                } else {  // monophonic
                    value = inputs[INPUT_AR + param].getVoltage();
                }
                if (value > 0) {  // green for positive voltage
                    lights[LIGHT_AR + 3 * param + 0].setSmoothBrightness(0, sample_time);
                    lights[LIGHT_AR + 3 * param + 1].setSmoothBrightness(value / 10.f, sample_time);
                    lights[LIGHT_AR + 3 * param + 2].setSmoothBrightness(0, sample_time);
                } else {  // red for negative voltage
                    lights[LIGHT_AR + 3 * param + 0].setSmoothBrightness(-value / 10.f, sample_time);
                    lights[LIGHT_AR + 3 * param + 1].setSmoothBrightness(0, sample_time);
                    lights[LIGHT_AR + 3 * param + 2].setSmoothBrightness(0, sample_time);
                }
            }
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
            const auto pos = Vec(7 + 33 * i, 41);
            const auto param = MiniBoss::PARAM_AR + i;
            const auto light = MiniBoss::LIGHT_AR + 3 * i;
            auto slider =
                createLightParam<LEDLightSlider<RedGreenBlueLight>>(pos, module, param, light);
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
                param->snap = knob > 0;
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

    /// @brief Append the context menu to the module when right clicked.
    ///
    /// @param menu the menu object to add context items for the module to
    ///
    void appendContextMenu(Menu* menu) override {
        // get a pointer to the module
        MiniBoss* const module = dynamic_cast<MiniBoss*>(this->module);

        /// a structure for holding changes to the model items
        struct PreventClicksItem : MenuItem {
            /// the module to update
            MiniBoss* module;

            /// Response to an action update to this item
            void onAction(const event::Action& e) override {
                module->prevent_clicks = !module->prevent_clicks;
            }
        };

        // add the envelope mode selection item to the menu
        menu->addChild(new MenuSeparator);
        auto item = createMenuItem<PreventClicksItem>(
            "Soft Reset Envelope Generator",
            CHECKMARK(module->prevent_clicks)
        );
        item->module = module;
        menu->addChild(item);
    }
};

Model *modelMiniBoss = createModel<MiniBoss, MiniBossWidget>("MiniBoss");
