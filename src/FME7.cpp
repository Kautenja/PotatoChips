// A SunSoft FME7 chip emulator module.
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
#include "dsp/sunsoft_fme7.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A SunSoft FME7 chip emulator module.
struct ChipFME7 : Module {
 private:
    /// The BLIP buffer to render audio samples from
    BLIPBuffer buffers[POLYPHONY_CHANNELS][SunSoftFME7::OSC_COUNT];
    /// The FME7 instance to synthesize sound with
    SunSoftFME7 apu[POLYPHONY_CHANNELS];

    /// a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ, SunSoftFME7::OSC_COUNT),
        ENUMS(PARAM_LEVEL, SunSoftFME7::OSC_COUNT),
        NUM_PARAMS
    };
    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT, SunSoftFME7::OSC_COUNT),
        ENUMS(INPUT_FM, SunSoftFME7::OSC_COUNT),
        ENUMS(INPUT_LEVEL, SunSoftFME7::OSC_COUNT),
        NUM_INPUTS
    };
    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_OSCILLATOR, SunSoftFME7::OSC_COUNT),
        NUM_OUTPUTS
    };
    /// the indexes of lights on the module
    enum LightIds { NUM_LIGHTS };

    /// @brief Initialize a new FME7 Chip module.
    ChipFME7() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        cvDivider.setDivision(16);
        // set the output buffer for each individual voice
        for (unsigned oscillator = 0; oscillator < SunSoftFME7::OSC_COUNT; oscillator++) {
            // get the oscillator name starting with ACII code 65 (A)
            auto name = std::string(1, static_cast<char>(65 + oscillator));
            configParam(PARAM_FREQ  + oscillator, -56.f, 56.f, 0.f,  "Pulse " + name + " Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
            configParam(PARAM_LEVEL + oscillator,   0.f,  1.f, 0.8f, "Pulse " + name + " Level",     "%",   0.f,                100.f       );
        }
        // set the output buffer for each individual voice
        for (unsigned channel = 0; channel < POLYPHONY_CHANNELS; channel++) {
            for (unsigned oscillator = 0; oscillator < SunSoftFME7::OSC_COUNT; oscillator++)
                apu[channel].set_output(oscillator, &buffers[channel][oscillator]);
            // volume of 3 produces a roughly 5Vpp signal from all voices
            apu[channel].set_volume(3.f);
        }
        onSampleRateChange();
    }

    /// @brief Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override {
        // update the buffer for each oscillator and polyphony channel
        for (unsigned channel = 0; channel < POLYPHONY_CHANNELS; channel++) {
            for (unsigned oscillator = 0; oscillator < SunSoftFME7::OSC_COUNT; oscillator++) {
                buffers[channel][oscillator].set_sample_rate(APP->engine->getSampleRate(), CLOCK_RATE);
            }
        }
    }

    /// @brief Return the frequency for the given oscillator.
    ///
    /// @param oscillator the index of the oscillator to get the frequency of
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 12-bit frequency in a 16-bit container
    ///
    inline uint16_t getFrequency(unsigned oscillator, unsigned channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ12BIT_MIN = 4;
        // the maximal value for the frequency register
        static constexpr float FREQ12BIT_MAX = 4067;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + oscillator].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + oscillator].getPolyVoltage(channel);
        pitch += inputs[INPUT_FM + oscillator].getPolyVoltage(channel) / 5.f;
        // convert the pitch to frequency based on standard exponential scale
        // and clamp within [0, 20000] Hz
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = buffers[channel][oscillator].get_clock_rate() / (CLOCK_DIVISION * freq);
        return rack::clamp(freq, FREQ12BIT_MIN, FREQ12BIT_MAX);
    }

    /// @brief Return the volume parameter for the given oscillator.
    ///
    /// @param oscillator the oscillator to get the volume parameter for
    /// @param channel the polyphonic channel to return the volume for
    /// @returns the volume parameter for the given oscillator. This includes
    /// the value of the knob and any CV modulation.
    ///
    inline uint8_t getVolume(unsigned oscillator, unsigned channel) {
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
        // return the 8-bit level clamped within legal limits
        return rack::clamp(LEVEL_MAX * param, LEVEL_MIN, LEVEL_MAX);
    }

    /// @brief Process a sample.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    void process(const ProcessArgs &args) override {
        // determine the number of channels based on the inputs
        unsigned channels = 1;
        for (unsigned input = 0; input < NUM_INPUTS; input++)
            channels = std::max(inputs[input].getChannels(), static_cast<int>(channels));
        if (cvDivider.process()) {  // process the CV inputs to the chip
            for (unsigned channel = 0; channel < channels; channel++) {
                for (unsigned oscillator = 0; oscillator < SunSoftFME7::OSC_COUNT; oscillator++) {
                    // frequency. there are two frequency registers per voice.
                    // shift the index left 1 instead of multiplying by 2
                    auto freq = getFrequency(oscillator, channel);
                    uint8_t lo =  freq & 0b11111111;
                    apu[channel].write(SunSoftFME7::PULSE_A_LO + (oscillator << 1), lo);
                    uint8_t hi = (freq & 0b0000111100000000) >> 8;
                    apu[channel].write(SunSoftFME7::PULSE_A_HI + (oscillator << 1), hi);
                    // level
                    apu[channel].write(SunSoftFME7::PULSE_A_ENV + oscillator, getVolume(oscillator, channel));
                }
            }
        }
        // set output polyphony channels
        for (unsigned oscillator = 0; oscillator < SunSoftFME7::OSC_COUNT; oscillator++)
            outputs[OUTPUT_OSCILLATOR + oscillator].setChannels(channels);
        // process audio samples on the chip engine
        for (unsigned channel = 0; channel < channels; channel++) {
            apu[channel].end_frame(CLOCK_RATE / args.sampleRate);
            for (unsigned oscillator = 0; oscillator < SunSoftFME7::OSC_COUNT; oscillator++) {
                const auto sample = buffers[channel][oscillator].read_sample_10V();
                outputs[OUTPUT_OSCILLATOR + oscillator].setVoltage(sample, channel);
            }
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for FME7.
struct ChipFME7Widget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    ChipFME7Widget(ChipFME7 *module) {
        setModule(module);
        static constexpr auto panel = "res/FME7.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        for (unsigned i = 0; i < SunSoftFME7::OSC_COUNT; i++) {
            addInput(createInput<PJ301MPort>(    Vec(18,  100 + 111 * i), module, ChipFME7::INPUT_VOCT     + i));
            addInput(createInput<PJ301MPort>(    Vec(18,  27  + 111 * i), module, ChipFME7::INPUT_FM       + i));
            addParam(createParam<Rogan6PSWhite>( Vec(47,  29  + 111 * i), module, ChipFME7::PARAM_FREQ     + i));
            addInput(createInput<PJ301MPort>(    Vec(152, 35  + 111 * i), module, ChipFME7::INPUT_LEVEL    + i));
            addParam(createParam<BefacoSlidePot>(Vec(179, 21  + 111 * i), module, ChipFME7::PARAM_LEVEL    + i));
            addOutput(createOutput<PJ301MPort>(  Vec(150, 100 + 111 * i), module, ChipFME7::OUTPUT_OSCILLATOR + i));
        }
    }
};

/// the global instance of the model
Model *modelChipFME7 = createModel<ChipFME7, ChipFME7Widget>("FME7");
