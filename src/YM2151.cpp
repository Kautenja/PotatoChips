// Voice 2151: independent Yamaha OPM channel 8 for every Rack lane.
// Copyright 2026 Arhythmetic Units
// SPDX-License-Identifier: GPL-3.0-or-later
#include "plugin.hpp"
#include "dsp/yamaha_ym2151/voice.hpp"
#include "dsp/trigger_threshold.hpp"
#include "widget/indexed_frame_display.hpp"
#include <atomic>
#include <cmath>

/// @brief Polyphonic OPM voice. All saved state is standard Rack parameters.
struct YM2151 : Module {
    enum ParamIds {
        TUNE, ALGORITHM, FEEDBACK, LEVEL, LFO, AMD, PMD, AMS, PMS,
        WAVE, NOISE, NOISE_RATE, ROUTE,
        OPERATOR, NUM_PARAMS = OPERATOR + 4 * 11
    };
    enum OperatorParam { AR, D1, SR, SL, RR, TL, MUL, KS, DT1, DT2, AM, OP_PARAMS };
    enum InputIds {
        PITCH_INPUT, GATE_INPUT, RETRIG_INPUT, ALGORITHM_INPUT, FEEDBACK_INPUT,
        LFO_INPUT, AMD_INPUT, PMD_INPUT, NOISE_INPUT, LEVEL_INPUT,
        OPERATOR_INPUT, NUM_INPUTS = OPERATOR_INPUT + 4 * 7
    };
    enum OutputIds { LEFT_OUTPUT, RIGHT_OUTPUT, NUM_OUTPUTS };

    YamahaYM2151::Voice voices[PORT_MAX_CHANNELS];
    Trigger::Threshold gates[PORT_MAX_CHANNELS], retriggers[PORT_MAX_CHANNELS];
    std::atomic<unsigned> displayAlgorithm{0};
    int active = 0;
    unsigned divider = 0;
    float sample_rate = 0;

    YM2151() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
        configParam(TUNE, -4, 4, 0, "Tune", " oct");
        configParam(ALGORITHM, 0, 7, 0, "Algorithm");
        configParam(FEEDBACK, 0, 7, 0, "Operator 1 feedback");
        configParam(LEVEL, 0, 1, 0.8, "Output level", "%", 0, 100);
        configParam(LFO, 0, 255, 0, "LFO frequency register");
        configParam(AMD, 0, 127, 0, "LFO AM depth");
        configParam(PMD, 0, 127, 0, "LFO PM depth");
        configParam(AMS, 0, 3, 0, "Channel AM sensitivity");
        configParam(PMS, 0, 7, 0, "Channel PM sensitivity");
        configSwitch(WAVE, 0, 3, 0, "LFO waveform", {"Saw", "Square", "Triangle", "Noise"});
        configSwitch(NOISE, 0, 1, 0, "Operator 4 noise", {"Off", "On"});
        configParam(NOISE_RATE, 0, 31, 16, "Noise rate");
        configSwitch(ROUTE, 0, 2, 2, "Hardware output route", {"Left", "Right", "Both"});
        for (int id = ALGORITHM; id < OPERATOR; ++id)
            if (id != LEVEL) getParamQuantity(id)->snapEnabled = true;
        const char* labels[] = {"V/oct", "Gate", "Retrigger", "Algorithm", "Feedback",
            "LFO frequency", "AM depth", "PM depth", "Noise rate", "Output level"};
        for (int id = 0; id < OPERATOR_INPUT; ++id) configInput(id, labels[id]);
        const char* names[] = {"Attack rate", "First decay rate", "Sustain rate",
            "Sustain level", "Release rate", "Operator level", "Multiplier",
            "Key-rate scaling", "DT1", "DT2", "AM enable"};
        const int maxima[] = {31, 31, 31, 15, 15, 127, 15, 3, 7, 3, 1};
        const int defaults[] = {31, 0, 0, 15, 15, 100, 1, 0, 0, 0, 0};
        for (int op = 0; op < 4; ++op) for (int field = 0; field < OP_PARAMS; ++field) {
            const std::string name = "Operator " + std::to_string(op + 1) + " " + names[field];
            const int id = OPERATOR + op * OP_PARAMS + field;
            configParam(id, 0, maxima[field], field == TL && op == 3 ? 127 : defaults[field], name);
            getParamQuantity(id)->snapEnabled = true;
            if (field < 7) configInput(OPERATOR_INPUT + op * 7 + field, name);
        }
        configOutput(LEFT_OUTPUT, "Left audio");
        configOutput(RIGHT_OUTPUT, "Right audio");
    }

    /// @brief Clamp before rounding; malformed/nonfinite CV cannot reach the core.
    float control(int id, int input, int lane, float maximum) {
        float value = params[id].getValue();
        if (input >= 0) value += inputs[input].getPolyVoltage(lane) * maximum / 8.f;
        if (!std::isfinite(value)) return 0;
        return std::max(0.f, std::min(maximum, value));
    }
    int discrete(int id, int input, int lane, int maximum) {
        return static_cast<int>(std::round(control(id, input, lane, maximum)));
    }

    /// @brief Controls use an additive +8 V full-range scale, at 16-frame cadence.
    void processCV(int lane) {
        auto& voice = voices[lane];
        const int algorithm = discrete(ALGORITHM, ALGORITHM_INPUT, lane, 7);
        const int route = discrete(ROUTE, -1, lane, 2);
        voice.write(0x27, (route == 0 ? 0x40 : route == 1 ? 0x80 : 0xc0)
            | discrete(FEEDBACK, FEEDBACK_INPUT, lane, 7) << 3 | algorithm);
        if (lane == 0) displayAlgorithm.store(algorithm, std::memory_order_relaxed);
        voice.pitch(params[TUNE].getValue() + inputs[PITCH_INPUT].getPolyVoltage(lane));
        voice.write(0x18, discrete(LFO, LFO_INPUT, lane, 255));
        voice.write(0x19, discrete(AMD, AMD_INPUT, lane, 127));
        voice.write(0x1a, discrete(PMD, PMD_INPUT, lane, 127));
        voice.write(0x1b, discrete(WAVE, -1, lane, 3));
        voice.write(0x3f, discrete(PMS, -1, lane, 7) << 4 | discrete(AMS, -1, lane, 3));
        voice.write(0x0f, discrete(NOISE, -1, lane, 1) << 7 | discrete(NOISE_RATE, NOISE_INPUT, lane, 31));
        // Natural algorithm order M1,C1,M2,C2 uses hardware slots 0,2,1,3.
        const int slots[] = {7, 23, 15, 31};
        for (int op = 0; op < 4; ++op) {
            int base = OPERATOR + op * OP_PARAMS;
            int input = OPERATOR_INPUT + op * 7;
            auto value = [&](int field, int maximum) {
                return discrete(base + field, field < 7 ? input + field : -1, lane, maximum);
            };
            voice.write(0x40 + slots[op], value(DT1, 7) << 4 | value(MUL, 15));
            voice.write(0x60 + slots[op], 127 - value(TL, 127));
            voice.write(0x80 + slots[op], value(KS, 3) << 6 | value(AR, 31));
            voice.write(0xa0 + slots[op], value(AM, 1) << 7 | value(D1, 31));
            voice.write(0xc0 + slots[op], value(DT2, 3) << 6 | value(SR, 31));
            voice.write(0xe0 + slots[op], (15 - value(SL, 15)) << 4 | value(RR, 15));
        }
    }

    void onReset() override {
        for (int lane = 0; lane < active; ++lane) {
            voices[lane].reset(); gates[lane].reset(); retriggers[lane].reset();
        }
        active = 0; divider = 0;
    }

    void process(const ProcessArgs& args) override {
        int lanes = 1;
        for (auto& input : inputs) lanes = std::max(lanes, input.getChannels());
        lanes = std::min(lanes, PORT_MAX_CHANNELS);
        if (sample_rate != args.sampleRate) {
            sample_rate = args.sampleRate;
            for (auto& voice : voices) voice.set_sample_rate(sample_rate);
        }
        for (int lane = lanes; lane < active; ++lane) {
            voices[lane].reset(); gates[lane].reset(); retriggers[lane].reset();
        }
        for (int lane = active; lane < lanes; ++lane) {
            processCV(lane); voices[lane].prepare();
        }
        active = lanes;
        for (auto& output : outputs) output.setChannels(lanes);
        const bool update = (divider++ % 16) == 0;
        for (int lane = 0; lane < lanes; ++lane) {
            if (update) processCV(lane);
            const bool was_high = gates[lane].isHigh();
            const bool rise = gates[lane].process((inputs[GATE_INPUT].getPolyVoltage(lane) - 0.01f) / 1.99f);
            const bool retrigger = retriggers[lane].process((inputs[RETRIG_INPUT].getPolyVoltage(lane) - 0.01f) / 1.99f);
            const bool high = gates[lane].isHigh();
            if (was_high && !high) voices[lane].key(false);
            else if (rise) voices[lane].key(true);
            else if (high && retrigger) voices[lane].key(true, true);
            float audio[2];
            voices[lane].process(audio);
            const float gain = 10.f * control(LEVEL, LEVEL_INPUT, lane, 1);
            for (int side = 0; side < 2; ++side)
                outputs[side].setVoltage(std::max(-10.f, std::min(10.f, audio[side] * gain)), lane);
        }
    }
};

/// @brief Four operator columns and a shared pitch/modulation column, 60 HP.
struct YM2151Widget : ModuleWidget {
    explicit YM2151Widget(YM2151* module) {
        setModule(module);
        setPanel(createThemedPanel(plugin_instance, "res/YM2151.svg"));
        for (float x : {15.f, 870.f}) for (float y : {0.f, 365.f})
            addChild(createWidget<ThemedScrew>(Vec(x, y)));
        for (int id = 0; id < YM2151::OPERATOR; ++id) {
            float x = 32 + (id % 3) * 55;
            float y = 65 + (id / 3) * 46;
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x, y), module, id));
        }
        for (int id = 0; id < YM2151::OPERATOR_INPUT; ++id) {
            float x = 24 + (id % 5) * 33;
            float y = 300 + (id / 5) * 42;
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(x, y), module, id));
        }
        for (int op = 0; op < 4; ++op) {
            float left = 180 + op * 180;
            for (int field = 0; field < YM2151::OP_PARAMS; ++field) {
                float x = left + 32 + (field % 3) * 55;
                float y = 65 + (field / 3) * 55;
                addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x, y), module,
                    YM2151::OPERATOR + op * YM2151::OP_PARAMS + field));
            }
            for (int field = 0; field < 7; ++field) {
                float x = left + 24 + (field % 4) * 43;
                float y = 300 + (field / 4) * 42;
                addInput(createInputCentered<ThemedPJ301MPort>(Vec(x, y), module,
                    YM2151::OPERATOR_INPUT + op * 7 + field));
            }
        }
        addChild(new IndexedFrameDisplay([this]() {
            return this->module ? static_cast<YM2151*>(this->module)->displayAlgorithm.load(std::memory_order_relaxed) : 0;
        }, asset::plugin(plugin_instance, "res/YM2151_algorithms/"), 8, Vec(65, 234), Vec(100, 40)));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(839, 262), module, YM2151::LEFT_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(875, 262), module, YM2151::RIGHT_OUTPUT));
    }
};
Model* modelYM2151 = createModel<YM2151, YM2151Widget>("YM2151");
