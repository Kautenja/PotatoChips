// A Sunsoft 5B FME7 Chip module.
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
#include "components.hpp"
#include "dsp/fme7.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Sunsoft 5B (FME7) Chip module.
struct ChipFME7 : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, 3),
        ENUMS(PARAM_LEVEL, 3),
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, 3),
        ENUMS(INPUT_FM, 3),
        ENUMS(INPUT_LEVEL, 3),
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, 3),
        OUTPUT_COUNT
    };
    enum LightIds { LIGHT_COUNT };

    /// the clock rate of the module
    static constexpr uint64_t CLOCK_RATE = 768000;

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[FME7::OSC_COUNT];
    /// The FME7 instance to synthesize sound with
    FME7 apu;

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    /// Initialize a new FME7 Chip module.
    ChipFME7() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ + 0, -48.f, 48.f, 0.f,  "Pulse A Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -48.f, 48.f, 0.f,  "Pulse B Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2, -48.f, 48.f, 0.f,  "Pulse C Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_LEVEL + 0,  0.f,  1.f, 0.5f, "Pulse A Level",     "%",   0.f,                100.f       );
        configParam(PARAM_LEVEL + 1,  0.f,  1.f, 0.5f, "Pulse B Level",     "%",   0.f,                100.f       );
        configParam(PARAM_LEVEL + 2,  0.f,  1.f, 0.5f, "Pulse C Level",     "%",   0.f,                100.f       );
        // set the output buffer for each individual voice
        for (int i = 0; i < FME7::OSC_COUNT; i++) {
            apu.osc_output(i, &buf[i]);
            buf[i].set_clock_rate(CLOCK_RATE);
        }
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.volume(3.f);
    }

    /// Process pulse wave for the given channel.
    ///
    /// @param channel the index of the channel to process
    ///
    inline void pulse(int channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ12BIT_MIN = 4;
        // the maximal value for the frequency register
        static constexpr float FREQ12BIT_MAX = 8191;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // the constant modulation factor
        static constexpr auto MOD_FACTOR = 10.f;
        // the minimal value for the volume width register
        static constexpr float LEVEL_MIN = 0;
        // the maximal value for the volume width register
        static constexpr float LEVEL_MAX = 13;

        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + channel].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + channel].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM + channel].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = CLOCK_RATE / (CLOCK_DIVISION * freq);
        uint16_t freq12bit = rack::clamp(freq, FREQ12BIT_MIN, FREQ12BIT_MAX);
        // write the registers with the frequency data
        apu.write_latch(PULSE_A_LO + 2 * channel);
        apu.write_data(0, freq12bit & 0b11111111);
        apu.write_latch(PULSE_A_HI + 2 * channel);
        apu.write_data(0, (freq12bit & 0b0000111100000000) >> 8);

        // get the level from the parameter knob
        auto levelParam = params[PARAM_LEVEL + channel].getValue();
        // apply the control voltage to the level
        if (inputs[INPUT_LEVEL + channel].isConnected())
            levelParam *= inputs[INPUT_LEVEL + channel].getVoltage() / 2.f;
        // get the 8-bit level clamped within legal limits
        uint8_t level = rack::clamp(LEVEL_MAX * levelParam, LEVEL_MIN, LEVEL_MAX);
        apu.write_latch(PULSE_A_ENV + channel);
        apu.write_data(0, level);
    }

    /// Return a 10V signed sample from the FME7.
    ///
    /// @param channel the channel to get the audio sample for
    ///
    inline float getAudioOut(int channel) {
        // the peak to peak output of the voltage
        static constexpr float Vpp = 10.f;
        // the amount of voltage per increment of 16-bit fidelity volume
        static constexpr float divisor = std::numeric_limits<int16_t>::max();
        auto samples = buf[channel].samples_count();
        if (samples == 0) return 0.f;
        // copy the buffer to a local vector and return the first sample
        std::vector<int16_t> output_buffer(samples);
        buf[channel].read_samples(&output_buffer[0], samples);
        // convert the 16-bit sample to 10Vpp floating point
        return Vpp * output_buffer[0] / divisor;
    }

    /// Process a sample.
    void process(const ProcessArgs &args) override {
        // calculate the number of clock cycles on the chip per audio sample
        uint32_t cycles_per_sample = CLOCK_RATE / args.sampleRate;
        // check for sample rate changes from the engine to send to the chip
        if (new_sample_rate) {
            // update the buffer for each channel
            for (int i = 0; i < FME7::OSC_COUNT; i++) {
                buf[i].set_sample_rate(args.sampleRate);
                buf[i].set_clock_rate(cycles_per_sample * args.sampleRate);
            }
            // clear the new sample rate flag
            new_sample_rate = false;
        }
        // process the data on the chip
        for (int i = 0; i < FME7::OSC_COUNT; i++) pulse(i);
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < FME7::OSC_COUNT; i++) {
            buf[i].end_frame(cycles_per_sample);
            outputs[OUTPUT_CHANNEL + i].setVoltage(getAudioOut(i));
        }
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override { new_sample_rate = true; }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The widget structure that lays out the panel of the module and the UI menus.
struct ChipFME7Widget : ModuleWidget {
    ChipFME7Widget(ChipFME7 *module) {
        setModule(module);
        static const auto panel = "res/FME7.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // V/OCT inputs
        addInput(createInput<PJ301MPort>(Vec(20, 78),  module, ChipFME7::INPUT_VOCT + 0));
        addInput(createInput<PJ301MPort>(Vec(20, 188), module, ChipFME7::INPUT_VOCT + 1));
        addInput(createInput<PJ301MPort>(Vec(20, 298), module, ChipFME7::INPUT_VOCT + 2));
        // FM inputs
        addInput(createInput<PJ301MPort>(Vec(26, 37),  module, ChipFME7::INPUT_FM + 0));
        addInput(createInput<PJ301MPort>(Vec(26, 149), module, ChipFME7::INPUT_FM + 1));
        addInput(createInput<PJ301MPort>(Vec(26, 258), module, ChipFME7::INPUT_FM + 2));
        // Frequency parameters
        addParam(createParam<Rogan3PSNES>(Vec(54, 42),  module, ChipFME7::PARAM_FREQ + 0));
        addParam(createParam<Rogan3PSNES>(Vec(54, 151), module, ChipFME7::PARAM_FREQ + 1));
        addParam(createParam<Rogan3PSNES>(Vec(54, 266), module, ChipFME7::PARAM_FREQ + 2));
        // levels
        addInput(createInput<PJ301MPort>(Vec(102, 36),   module, ChipFME7::INPUT_LEVEL + 0));
        addInput(createInput<PJ301MPort>(Vec(102, 146),  module, ChipFME7::INPUT_LEVEL + 1));
        addInput(createInput<PJ301MPort>(Vec(102, 255),  module, ChipFME7::INPUT_LEVEL + 2));
        addParam(createParam<Rogan0PSNES>(Vec(103, 64),  module, ChipFME7::PARAM_LEVEL + 0));
        addParam(createParam<Rogan0PSNES>(Vec(103, 174), module, ChipFME7::PARAM_LEVEL + 1));
        addParam(createParam<Rogan0PSNES>(Vec(103, 283), module, ChipFME7::PARAM_LEVEL + 2));
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(107, 104), module, ChipFME7::OUTPUT_CHANNEL + 0));
        addOutput(createOutput<PJ301MPort>(Vec(107, 214), module, ChipFME7::OUTPUT_CHANNEL + 1));
        addOutput(createOutput<PJ301MPort>(Vec(107, 324), module, ChipFME7::OUTPUT_CHANNEL + 2));
    }
};

/// the global instance of the model
Model *modelChipFME7 = createModel<ChipFME7, ChipFME7Widget>("FME7");
