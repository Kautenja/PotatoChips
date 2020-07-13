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
#include "dsp/namco_106_apu.hpp"
#include "widgets/wavetable_editor.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Namco 106 Chip module.
struct Chip106 : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, Namco106::OSC_COUNT),
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, Namco106::OSC_COUNT),
        ENUMS(INPUT_FM, Namco106::OSC_COUNT),
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, Namco106::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds { LIGHT_COUNT };

    /// the clock rate of the module
    static constexpr uint64_t CLOCK_RATE = 768000;

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[Namco106::OSC_COUNT];
    /// The 106 instance to synthesize sound with
    Namco106 apu;

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    /// the bit-depth of the wave-table
    static constexpr auto bit_depth = 15;
    /// the number of samples in the wave-table
    static constexpr auto num_samples = 32;
    /// the samples in the wave-table
    uint8_t values[num_samples] = {
        0xA,0x8,0xD,0xC,0xE,0xE,0xF,0xF,0xF,0xF,0xE,0xF,0xD,0xE,0xA,0xC,
        0x5,0x8,0x2,0x3,0x1,0x1,0x0,0x0,0x0,0x0,0x1,0x0,0x2,0x1,0x5,0x3
    };

    /// Initialize a new 106 Chip module.
    Chip106() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        // set the output buffer for each individual voice
        for (int i = 0; i < Namco106::OSC_COUNT; i++) {
            configParam(PARAM_FREQ + i, -30.f, 30.f, 0.f, "Channel Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
            apu.osc_output(i, &buf[i]);
            buf[i].set_clock_rate(CLOCK_RATE);
        }
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.volume(3.f);
    }

    /// Set the frequency for a given channel.
    ///
    /// @param channel the channel to set the frequency for
    /// @param num_channels the number of enabled channels
    /// @param volume the volume for the channel
    ///
    void setFrequency(int channel,
        uint8_t num_channels = 2,
        uint8_t wave_address = 0,
        uint8_t volume = 0x0F
    ) {
        // get the frequency of the oscillator from the parameter and CVs
        float pitch = params[PARAM_FREQ + channel].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + channel].getVoltage();
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        static constexpr float FM_SCALE = 5.f;
        freq += FM_SCALE * inputs[INPUT_FM + channel].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to the 8-bit value for the oscillator
        auto wave_length = 64 - (num_samples / 4);
        // TODO: changing num_channels to 1 allows the standard 103 function
        //       where additional channels reduce the frequency of all
        freq *= (wave_length * num_channels * 15.f * 65536.f) / CLOCK_RATE;
        // clamp within the legal bounds for the frequency value
        freq = rack::clamp(freq, 4.f, 262143.f);
        // extract the low, medium, and high frequency register values
        auto freq18bit = static_cast<uint32_t>(freq);
        // FREQUENCY LOW
        uint8_t low = (freq18bit & 0b000000000011111111) >> 0;
        apu.write_addr(0x40 + 8 * channel);
        apu.write_data(0, low);
        // FREQUENCY MEDIUM
        uint8_t med = (freq18bit & 0b001111111100000000) >> 8;
        apu.write_addr(0x42 + 8 * channel);
        apu.write_data(0, med);
        // WAVEFORM LENGTH + FREQUENCY HIGH
        uint8_t hig = (freq18bit & 0b110000000000000000) >> 16;
        apu.write_addr(0x44 + 8 * channel);
        apu.write_data(0, (wave_length << 2) | hig);
        // WAVE ADDRESS
        apu.write_addr(0x46 + 8 * channel);
        apu.write_data(0, wave_address);
        // VOLUME (and channel selection on channel 8, no effect elsewhere)
        apu.write_addr(0x47 + 8 * channel);
        apu.write_data(0, ((num_channels - 1) << 4) | volume);
    }

    /// Return a 10V signed sample from the chip.
    ///
    /// @param channel the channel to get the audio sample for
    ///
    float getAudioOut(int channel) {
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
            for (int i = 0; i < Namco106::OSC_COUNT; i++) {
                buf[i].set_sample_rate(args.sampleRate);
                buf[i].set_clock_rate(cycles_per_sample * args.sampleRate);
            }
            // clear the new sample rate flag
            new_sample_rate = false;
        }
        // write the waveform data to the RAM
        for (int i = 0; i < num_samples / 2; i++) {
            apu.write_addr(i);
            apu.write_data(0, (values[2 * i] << 4) | values[2 * i + 1]);
        }
        // set the frequency for all channels
        for (int i = 0; i < Namco106::OSC_COUNT; i++)
            setFrequency(i);
        // set the output from the oscillators (in reverse order)
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < Namco106::OSC_COUNT; i++) {
            buf[i].end_frame(cycles_per_sample);
            outputs[i].setVoltage(getAudioOut(i));
        }
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override { new_sample_rate = true; }

    /// Update the wave-table.
    ///
    /// @param index the index in the wave-table
    /// @param value the value of the waveform at given index
    ///
    inline void update_wavetable(uint32_t index, uint64_t value) {
        values[index] = value;
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The widget structure that lays out the panel of the module and the UI menus.
struct Chip106Widget : ModuleWidget {
    Chip106Widget(Chip106 *module) {
        setModule(module);
        static const auto panel = "res/106.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // add the wavetable editor
        auto table_editor = new WaveTableEditor(
            Vec(RACK_GRID_WIDTH, 20),                     // position
            Vec(box.size.x/2 - 2*RACK_GRID_WIDTH, 80),    // size
            {.r = 0,   .g = 0,   .b = 0,   .a = 1  },     // background color
            {.r = 0,   .g = 0,   .b = 1,   .a = 1  },     // fill color
            {.r = 0.2, .g = 0.2, .b = 0.2, .a = 1  },     // border color
            Chip106::num_samples,                         // wave-table length
            Chip106::bit_depth,                           // waveform bit depth
            [&](uint32_t index, uint64_t value) {         // update callback
                auto module = reinterpret_cast<Chip106*>(this->module);
                module->update_wavetable(index, value);
            }
        );
        addChild(table_editor);
        for (int i = 0; i < Namco106::OSC_COUNT; i++) {
            addInput(createInput<PJ301MPort>(  Vec(140, 40 + i * 40), module, Chip106::INPUT_VOCT + i    ));
            addInput(createInput<PJ301MPort>(  Vec(170, 40 + i * 40), module, Chip106::INPUT_FM + i      ));
            addParam(createParam<Rogan3PSNES>( Vec(200, 30 + i * 40), module, Chip106::PARAM_FREQ + i    ));
            addOutput(createOutput<PJ301MPort>(Vec(250, 40 + i * 40), module, Chip106::OUTPUT_CHANNEL + i));
        }
    }
};

/// the global instance of the model
Model *modelChip106 = createModel<Chip106, Chip106Widget>("106");
