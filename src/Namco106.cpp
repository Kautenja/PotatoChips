// A Namco 106 Chip module.
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
#include "dsp/namco106.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Namco 106 Chip module.
struct ChipNamco106 : Module {
    enum ParamIds {
        PARAM_FREQ0,
        PARAM_PW0,
        PARAM_COUNT
    };
    enum InputIds {
        INPUT_VOCT0,
        INPUT_FM0,
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, 8),
        OUTPUT_COUNT
    };
    enum LightIds { LIGHT_COUNT };

    /// the clock rate of the module
    static constexpr uint64_t CLOCK_RATE = 768000;

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[Namco106::OSC_COUNT];
    /// The Namco106 instance to synthesize sound with
    Namco106 apu;

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    /// Initialize a new Namco106 Chip module.
    ChipNamco106() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ0, -30.f, 30.f, 0.f, "Pulse 1 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_PW0,     0,    3,   2,   "Pulse 1 Duty Cycle");
        // set the output buffer for each individual voice
        for (int i = 0; i < Namco106::OSC_COUNT; i++) apu.osc_output(i, &buf[i]);
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.volume(3.f);
    }

    /// Return a 10V signed sample from the chip.
    ///
    /// @param channel the channel to get the audio sample for
    ///
    float getAudioOut(int channel) {
        auto samples = buf[channel].samples_count();
        if (samples == 0) return 0.f;
        // copy the buffer to a local vector and return the first sample
        std::vector<int16_t> output_buffer(samples);
        buf[channel].read_samples(&output_buffer[0], samples);
        // convert the 16-bit sample to 10Vpp floating point
        return 10.f * output_buffer[0] / static_cast<float>(1 << 15);;
    }

    /// Process a sample.
    void process(const ProcessArgs &args) override {
        // calculate the number of clock cycles on the chip per audio sample
        uint32_t cycles_per_sample = CLOCK_RATE / args.sampleRate;
        // check for sample rate changes from the engine to send to the chip
        if (new_sample_rate) {
            // update the buffer for each channel
            for (int i = 0; i < Namco106::OSC_COUNT; i++) {
                buf[i].set_sample_rate(args.sampleRate);
                buf[i].set_clock_rate(cycles_per_sample * args.sampleRate);
            }
            // clear the new sample rate flag
            new_sample_rate = false;
        }



        // // the minimal value for the frequency register to produce sound
        // static constexpr auto FREQ_MIN = 8;
        // // the maximal value for the frequency register
        // static constexpr auto FREQ_MAX = 1023;
        // // the clock division of the oscillator relative to the CPU
        // static constexpr auto CLOCK_DIVISION = 16;
        // // get the pitch / frequency of the oscillator in 11-bit
        // float pitch = params[PARAM_FREQ0].getValue() / 12.f;
        // pitch += inputs[INPUT_VOCT0].getVoltage();
        // float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        // freq = rack::clamp(freq, 0.0f, 20000.0f);
        // uint16_t freq11bit = (CLOCK_RATE / (CLOCK_DIVISION * freq)) - 1;
        // freq11bit += inputs[INPUT_FM0].getVoltage();
        // freq11bit = rack::clamp(freq11bit, FREQ_MIN, FREQ_MAX);
        // apu.write_register(0, PULSE0_LO, freq11bit & 0b11111111);
        // apu.write_register(0, PULSE0_HI, (freq11bit & 0b0000011100000000) >> 8);
        // // set the pulse width of the pulse wave (high 2 bits) and set the
        // // volume to a constant level
        // auto pw = static_cast<uint8_t>(params[PARAM_PW0].getValue()) << 6;
        // apu.write_register(0, PULSE0_VOL, pw + 0b00011111);



        // set the output from the oscillators
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < Namco106::OSC_COUNT; i++) {
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
struct ChipNamco106Widget : ModuleWidget {
    ChipNamco106Widget(ChipNamco106 *module) {
        setModule(module);
        static const auto panel = "res/Namco106.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // V/OCT inputs
        addInput(createInput<PJ301MPort>(Vec(28, 74), module, ChipNamco106::INPUT_VOCT0));
        // FM inputs
        addInput(createInput<PJ301MPort>(Vec(33, 32), module, ChipNamco106::INPUT_FM0));
        // Frequency parameters
        addParam(createParam<Rogan3PSNES>(Vec(62, 42), module, ChipNamco106::PARAM_FREQ0));
        // PW
        addParam(createParam<Rogan0PSNES_Snap>(Vec(109, 30), module, ChipNamco106::PARAM_PW0));
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(114, 74), module, ChipNamco106::OUTPUT_CHANNEL));
    }
};

/// the global instance of the model
Model *modelChipNamco106 = createModel<ChipNamco106, ChipNamco106Widget>("Namco106");
