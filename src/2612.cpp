// A Yamaha YM2612 Chip module.
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
#include "dsp/yamaha_ym2612.hpp"
#include "widget/indexed_frame_display.hpp"
#include <functional>

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Yamaha YM2612 chip emulator module.
struct Chip2612 : rack::Module {
 public:
    /// the number of FM algorithms on the module
    static constexpr unsigned NUM_ALGORITHMS = 8;
    /// the number of FM operators on the module
    static constexpr unsigned NUM_OPERATORS = 4;
    /// the number of independent FM synthesis oscillators on the module
    static constexpr unsigned NUM_VOICES = 6;

 private:
    /// the YM2612 chip emulator
    YamahaYM2612 apu[PORT_MAX_CHANNELS];

    /// triggers for opening and closing the oscillator gates
    dsp::BooleanTrigger gate_triggers[PORT_MAX_CHANNELS][NUM_VOICES];

    /// a clock divider for reducing computation (on CV acquisition)
    dsp::ClockDivider cvDivider;

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
        ENUMS(PARAM_TL,  NUM_OPERATORS),
        ENUMS(PARAM_AR,  NUM_OPERATORS),
        ENUMS(PARAM_D1,  NUM_OPERATORS),
        ENUMS(PARAM_SL,  NUM_OPERATORS),
        ENUMS(PARAM_D2,  NUM_OPERATORS),
        ENUMS(PARAM_RR,  NUM_OPERATORS),
        ENUMS(PARAM_MUL, NUM_OPERATORS),
        ENUMS(PARAM_DET, NUM_OPERATORS),
        ENUMS(PARAM_RS,  NUM_OPERATORS),
        ENUMS(PARAM_AM,  NUM_OPERATORS),
        ENUMS(PARAM_SSG, NUM_OPERATORS),
        NUM_PARAMS
    };
    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_PITCH, NUM_VOICES),
        ENUMS(INPUT_GATE,  NUM_VOICES),
        INPUT_AL,
        INPUT_FB,
        INPUT_LFO,
        INPUT_AMS,
        INPUT_FMS,
        ENUMS(INPUT_TL,  NUM_OPERATORS),
        ENUMS(INPUT_AR,  NUM_OPERATORS),
        ENUMS(INPUT_D1,  NUM_OPERATORS),
        ENUMS(INPUT_SL,  NUM_OPERATORS),
        ENUMS(INPUT_D2,  NUM_OPERATORS),
        ENUMS(INPUT_RR,  NUM_OPERATORS),
        ENUMS(INPUT_MUL, NUM_OPERATORS),
        ENUMS(INPUT_DET, NUM_OPERATORS),
        ENUMS(INPUT_RS,  NUM_OPERATORS),
        ENUMS(INPUT_AM,  NUM_OPERATORS),
        ENUMS(INPUT_SSG, NUM_OPERATORS),
        NUM_INPUTS
    };
    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_MASTER, 2),
        NUM_OUTPUTS
    };
    /// the indexes of lights on the module
    enum LightIds { NUM_LIGHTS };

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
        for (unsigned i = 0; i < NUM_OPERATORS; i++) {  // operator parameters
            auto opName = "Operator " + std::to_string(i + 1);
            // total level is defined on the domain [0, 127], but values above
            // 70 cause the operator to drop below usable levels
            configParam(PARAM_TL  + i, 0, 70,  0,  opName + " Total Level");
            configParam(PARAM_AR  + i, 0, 31,  31, opName + " Attack Rate");
            configParam(PARAM_D1  + i, 0, 31,  0,  opName + " 1st Decay Rate");
            configParam(PARAM_SL  + i, 0, 15,  0,  opName + " Sustain Level");
            configParam(PARAM_D2  + i, 0, 31,  0,  opName + " 2nd Decay Rate");
            configParam(PARAM_RR  + i, 0, 15,  15, opName + " Release Rate");
            configParam(PARAM_MUL + i, 0, 15,  1,  opName + " Multiplier");
            configParam(PARAM_DET + i, 0, 7,   4,  opName + " Detune");
            configParam(PARAM_RS  + i, 0, 3,   0,  opName + " Rate Scaling");
            configParam(PARAM_AM  + i, 0, 1,   0,  opName + " Amplitude Modulation");
            configParam(PARAM_SSG + i, 0, 1,   0,  opName + " Looping Envelope");
        }
        // reset the emulator
        onSampleRateChange();
        // set the rate of the CV acquisition clock divider
        cvDivider.setDivision(16);
    }

    /// @brief Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() final {
        // update the buffer for each oscillator and polyphony channel
        for (unsigned channel = 0; channel < PORT_MAX_CHANNELS; channel++)
            apu[channel].setSampleRate(CLOCK_RATE, APP->engine->getSampleRate());
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
        apu[channel].setLFO(getParam(0, PARAM_LFO, INPUT_LFO, 7));
        // iterate over each oscillator on the chip
        float pitch = 0;
        float gate = 0;
        for (unsigned osc = 0; osc < NUM_VOICES; osc++) {
            // set the global parameters
            apu[channel].setAL (osc, getParam(channel, PARAM_AL,  INPUT_AL,  7));
            apu[channel].setFB (osc, getParam(channel, PARAM_FB,  INPUT_FB,  7));
            apu[channel].setAMS(osc, getParam(channel, PARAM_AMS, INPUT_AMS, 3));
            apu[channel].setFMS(osc, getParam(channel, PARAM_FMS, INPUT_FMS, 7));
            // set the operator parameters
            for (unsigned op = 0; op < NUM_OPERATORS; op++) {
                apu[channel].setTL (osc, op, getParam(channel, PARAM_TL  + op, INPUT_TL  + op, 70 ));
                apu[channel].setAR (osc, op, getParam(channel, PARAM_AR  + op, INPUT_AR  + op, 31 ));
                apu[channel].setD1 (osc, op, getParam(channel, PARAM_D1  + op, INPUT_D1  + op, 31 ));
                apu[channel].setSL (osc, op, getParam(channel, PARAM_SL  + op, INPUT_SL  + op, 15 ));
                apu[channel].setD2 (osc, op, getParam(channel, PARAM_D2  + op, INPUT_D2  + op, 31 ));
                apu[channel].setRR (osc, op, getParam(channel, PARAM_RR  + op, INPUT_RR  + op, 15 ));
                apu[channel].setMUL(osc, op, getParam(channel, PARAM_MUL + op, INPUT_MUL + op, 15 ));
                apu[channel].setDET(osc, op, getParam(channel, PARAM_DET + op, INPUT_DET + op, 7  ));
                apu[channel].setRS (osc, op, getParam(channel, PARAM_RS  + op, INPUT_RS  + op, 3  ));
                apu[channel].setAM (osc, op, getParam(channel, PARAM_AM  + op, INPUT_AM  + op, 1  ));
                apu[channel].setSSG(osc, op, getParam(channel, PARAM_SSG + op, INPUT_SSG + op, 1  ), 0xe);
            }
            // Compute the frequency from the pitch parameter and input. low
            // range of -4 octaves, high range of 6 octaves
            pitch = inputs[INPUT_PITCH + osc].getNormalVoltage(pitch, channel);
            apu[channel].setFREQ(osc, dsp::FREQ_C4 * std::pow(2.f, clamp(pitch, -4.f, 6.f)));
            // process the gate trigger, high at 2V
            gate = inputs[INPUT_GATE + osc].getNormalVoltage(gate, channel);
            gate_triggers[channel][osc].process(rescale(gate, 0.f, 2.f, 0.f, 1.f));
            apu[channel].setGATE(osc, gate_triggers[channel][osc].state);
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
            apu[channel].step();
            // set the outputs of the module
            outputs[OUTPUT_MASTER + 0].setVoltage(apu[channel].getVoltageLeft(), channel);
            outputs[OUTPUT_MASTER + 1].setVoltage(apu[channel].getVoltageRight(), channel);
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
        // voice inputs (pitch and gate)
        for (unsigned i = 0; i < Chip2612::NUM_VOICES; i++) {
            addInput(createInput<PJ301MPort>(Vec(26, 84 + 34 * i), module, Chip2612::INPUT_PITCH + i));
            addInput(createInput<PJ301MPort>(Vec(71, 84 + 34 * i), module, Chip2612::INPUT_GATE  + i));
        }
        // algorithm display
        addChild(new IndexedFrameDisplay(
            [&]() {
                return this->module ? reinterpret_cast<Chip2612*>(this->module)->algorithm[0] : 0;
            },
            "res/2612algorithms/",
            Chip2612::NUM_ALGORITHMS,
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
        for (unsigned i = 0; i < Chip2612::NUM_OPERATORS; i++) {
            // the X & Y offsets for the operator bank
            auto offsetX = 348 * (i % (Chip2612::NUM_OPERATORS / 2));
            auto offsetY = 175 * (i / (Chip2612::NUM_OPERATORS / 2));
            for (unsigned parameter = 0; parameter < 11; parameter++) {
                // the parameter & input offset
                auto offset = i + parameter * Chip2612::NUM_OPERATORS;
                auto param = createParam<BefacoSlidePot>(Vec(248 + offsetX + 34 * parameter, 25 + offsetY), module, Chip2612::PARAM_TL + offset);
                param->snap = true;
                addParam(param);
                addInput(createInput<PJ301MPort>(Vec(244 + offsetX + 34 * parameter, 160 + offsetY), module, Chip2612::INPUT_TL + offset));
            }
        }
        // left + right master outputs
        addOutput(createOutput<PJ301MPort>(Vec(26, 325), module, Chip2612::OUTPUT_MASTER + 0));
        addOutput(createOutput<PJ301MPort>(Vec(71, 325), module, Chip2612::OUTPUT_MASTER + 1));
    }
};

Model *modelChip2612 = createModel<Chip2612, Chip2612Widget>("2612");
