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
#include <limits>

// TODO: remove algorithm widget
// TODO: attenuators for all inputs
// TODO: polyphony
// TODO: pitch and gate inputs for each of the 6 voices
// -   normalled inputs for chords and stuff

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Yamaha YM2612 chip emulator module.
struct Chip2612 : rack::Module {
    /// the number of FM algorithms on the module
    static constexpr unsigned NUM_ALGORITHMS = 8;
    /// the number of FM operators on the module
    static constexpr unsigned NUM_OPERATORS = 4;
    /// the number of independent FM synthesis oscillators on the module
    static constexpr unsigned NUM_VOICES = 6;

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
        NUM_PARAMS
    };
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
        NUM_INPUTS
    };
    enum OutputIds {
        ENUMS(OUTPUT_MASTER, 2),
        NUM_OUTPUTS
    };
    enum LightIds { NUM_LIGHTS };

    /// the current FM algorithm
    uint8_t algorithm = 7;

    /// the YM2612 chip emulator
    YM2612 ym2612;

    /// triggers for opening and closing the oscillator gates
    dsp::BooleanTrigger gate_triggers[NUM_VOICES];

    /// a clock divider for reducing computation (on CV acquisition)
    dsp::ClockDivider cvDivider;

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
            configParam(PARAM_TL  + i, 0, 127, 0,  opName + " Total Level");
            configParam(PARAM_AR  + i, 0, 31,  31, opName + " Attack Rate");
            configParam(PARAM_D1  + i, 0, 31,  0,  opName + " 1st Decay Rate");
            configParam(PARAM_SL  + i, 0, 15,  0,  opName + " Sustain Level");
            configParam(PARAM_D2  + i, 0, 31,  0,  opName + " 2nd Decay Rate");
            configParam(PARAM_RR  + i, 0, 15,  15, opName + " Release Rate");
            configParam(PARAM_MUL + i, 0, 15,  1,  opName + " Multiplier");
            configParam(PARAM_DET + i, 0, 7,   0,  opName + " Detune");
            configParam(PARAM_RS  + i, 0, 3,   0,  opName + " Rate Scaling");
            configParam(PARAM_AM  + i, 0, 1,   0,  opName + " Amplitude Modulation");
        }
        // reset the emulator
        ym2612.reset();
        // set the rate of the CV acquisition clock divider
        cvDivider.setDivision(16);
    }

    /// Return the binary value for the given parameter.
    ///
    /// @param channel the channel to get the parameter value for
    /// @param paramIndex the index of the parameter in the params list
    /// @param inputIndex the index of the CV input in the inputs list
    /// @param int max the maximal value for the parameter
    ///
    inline uint8_t computeValue(
        unsigned channel,
        unsigned paramIndex,
        unsigned inputIndex,
        unsigned max
    ) {
        auto param = params[paramIndex].getValue();
        auto cv = max * inputs[inputIndex].getVoltage() / 10.0f;
        return clamp(static_cast<int>(param + cv), 0, max);
    }

    /// Process the CV inputs on the module.
    inline void processCV() {
        // this value is used in the algorithm widget
        algorithm = params[PARAM_AL].getValue() + inputs[INPUT_AL].getVoltage();
        algorithm = clamp(algorithm, 0, 7);
        ym2612.setLFO(computeValue(0, PARAM_LFO, INPUT_LFO, 7));
        // iterate over each oscillator on the chip
        float pitch = 0;
        float gate = 0;
        for (unsigned osc = 0; osc < NUM_VOICES; osc++) {
            // set the global parameters
            ym2612.setAL (osc, computeValue(osc, PARAM_AL,  INPUT_AL,  7));
            ym2612.setFB (osc, computeValue(osc, PARAM_FB,  INPUT_FB,  7));
            ym2612.setAMS(osc, computeValue(osc, PARAM_AMS, INPUT_AMS, 3));
            ym2612.setFMS(osc, computeValue(osc, PARAM_FMS, INPUT_FMS, 7));
            // set the operator parameters
            for (unsigned op = 0; op < NUM_OPERATORS; op++) {
                ym2612.setAR (osc, op, computeValue(osc, PARAM_AR  + op, INPUT_AR  + op, 31 ));
                ym2612.setD1 (osc, op, computeValue(osc, PARAM_D1  + op, INPUT_D1  + op, 31 ));
                ym2612.setSL (osc, op, computeValue(osc, PARAM_SL  + op, INPUT_SL  + op, 15 ));
                ym2612.setD2 (osc, op, computeValue(osc, PARAM_D2  + op, INPUT_D2  + op, 31 ));
                ym2612.setRR (osc, op, computeValue(osc, PARAM_RR  + op, INPUT_RR  + op, 15 ));
                ym2612.setTL (osc, op, computeValue(osc, PARAM_TL  + op, INPUT_TL  + op, 127));
                ym2612.setMUL(osc, op, computeValue(osc, PARAM_MUL + op, INPUT_MUL + op, 15 ));
                ym2612.setDET(osc, op, computeValue(osc, PARAM_DET + op, INPUT_DET + op, 7  ));
                ym2612.setRS (osc, op, computeValue(osc, PARAM_RS  + op, INPUT_RS  + op, 3  ));
                ym2612.setAM (osc, op, computeValue(osc, PARAM_AM  + op, INPUT_AM  + op, 1  ));
            }
            // Compute the frequency from the pitch parameter and input
            pitch = inputs[INPUT_PITCH + osc].getNormalVoltage(pitch);
            float freq = dsp::FREQ_C4 * std::pow(2.f, clamp(pitch, -4.f, 6.f));
            ym2612.setFREQ(osc, freq);
            // process the gate trigger
            gate = inputs[INPUT_GATE + osc].getNormalVoltage(gate);
            gate_triggers[osc].process(rescale(gate, 0.f, 2.f, 0.f, 1.f));
            ym2612.setGATE(osc, gate_triggers[osc].state);
        }
    }

    /// @brief Process a sample.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    void process(const ProcessArgs &args) override {
        // only process control voltage when the CV divider is high
        if (cvDivider.process()) processCV();
        // advance one sample in the emulator
        ym2612.step();
        // TODO: Normal master outputs
        // set the outputs of the module
        outputs[OUTPUT_MASTER + 0].setVoltage(static_cast<float>(ym2612.MOL) / std::numeric_limits<int16_t>::max());
        outputs[OUTPUT_MASTER + 1].setVoltage(static_cast<float>(ym2612.MOR) / std::numeric_limits<int16_t>::max());
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
    Chip2612Widget(Chip2612 *module) {
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
        uint8_t defaultAlgorithm = 0;
        uint8_t* index = module ? &module->algorithm : &defaultAlgorithm;
        addChild(new IndexedFrameDisplay<uint8_t>(
            index,
            "res/2612algorithms/",
            Chip2612::NUM_ALGORITHMS,
            Vec(115, 20),
            Vec(110, 70)
        ));
        // global parameters and inputs
        addParam(createParam<Rogan3PBlue>(Vec(115, 113),  module, Chip2612::PARAM_AL));
        addInput(createInput<PJ301MPort>( Vec(124, 171),  module, Chip2612::INPUT_AL));
        addParam(createParam<Rogan3PBlue>(Vec(182, 113),  module, Chip2612::PARAM_FB));
        addInput(createInput<PJ301MPort>( Vec(191, 171),  module, Chip2612::INPUT_FB));
        addParam(createParam<Rogan2PWhite>(Vec(187, 223), module, Chip2612::PARAM_LFO));
        addInput(createInput<PJ301MPort>( Vec(124, 226),  module, Chip2612::INPUT_LFO));
        addParam(createParam<Rogan2PWhite>(Vec(187, 279), module, Chip2612::PARAM_AMS));
        addInput(createInput<PJ301MPort>(  Vec(124, 282), module, Chip2612::INPUT_AMS));
        addParam(createParam<Rogan2PWhite>(Vec(187, 335), module, Chip2612::PARAM_FMS));
        addInput(createInput<PJ301MPort>(  Vec(124, 338), module, Chip2612::INPUT_FMS));
        // operator parameters and inputs
        for (unsigned i = 0; i < Chip2612::NUM_OPERATORS; i++) {
            // the X & Y offsets for the operator bank
            auto offsetX = 348 * (i % (Chip2612::NUM_OPERATORS / 2));
            auto offsetY = 175 * (i / (Chip2612::NUM_OPERATORS / 2));
            for (unsigned parameter = 0; parameter < 10; parameter++) {
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
