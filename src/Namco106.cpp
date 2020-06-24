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

#include <iostream>

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

        // a foo waveform
        static constexpr uint8_t values[32] = {
            0x00, 0x00, 0x00, 0xA8, 0xDC, 0xEE, 0xFF, 0xFF, 0xEF, 0xDE, 0xAC, 0x58, 0x23, 0x11, 0x00, 0x00,
            0x10, 0x21, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        // write the waveform to the RAM
        for (int i = 0; i < 32; i++) {
            apu.write_addr(i);
            apu.write_data(0, values[i]);
        }
        // write the wave address
        apu.write_addr(0x7E);
        apu.write_data(0, 0);

        // get the frequency of the oscillator from the parameter and CVs
        float pitch = params[PARAM_FREQ0].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT0].getVoltage();
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += 4 * inputs[INPUT_FM0].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to the 8-bit value for the oscillator
        auto num_channels = 2;
        auto wave_length = 48;
        freq *= (wave_length * num_channels * 15.f * 65536.f) / CLOCK_RATE;
        freq = rack::clamp(freq, 4.f, 262143.f);
        // extract the low, medium, and high frequency register values
        auto freq18bit = static_cast<uint32_t>(freq);
        uint8_t low = (freq18bit & 0b000000000011111111) >> 0;
        uint8_t med = (freq18bit & 0b001111111100000000) >> 8;
        uint8_t hig = (freq18bit & 0b110000000000000000) >> 16;

        // enable sound output from the chip
        // apu.write_addr(0xE000);
        // apu.write_data(0, 0);
        // low F
        apu.write_addr(0x78);
        apu.write_data(0, low);
        // med F
        apu.write_addr(0x7A);
        apu.write_data(0, med);
        // hi F / length
        apu.write_addr(0x7C);
        apu.write_data(0, (48 << 2) + hig);

        // volume and channel selection
        apu.write_addr(0x7F);
        static constexpr uint8_t volume = 0b00001111;
        apu.write_data(0, (num_channels << 4) + volume);

        // set the output from the oscillators
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < Namco106::OSC_COUNT; i++) {
            buf[i].end_frame(cycles_per_sample);
            outputs[OUTPUT_CHANNEL + 7 - i].setVoltage(getAudioOut(i));
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
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(114, 74), module, ChipNamco106::OUTPUT_CHANNEL));
    }
};

/// the global instance of the model
Model *modelChipNamco106 = createModel<ChipNamco106, ChipNamco106Widget>("Namco106");
