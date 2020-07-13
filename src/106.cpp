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
#include <iostream>

/// Addresses to the registers for channel 1. To get channel \f$n\f$,
/// multiply by \f$8n\f$.
enum Namco106Registers {
    FREQ_LOW = 0x40,
    PHASE_LOW,
    FREQ_MEDIUM,
    PHASE_MEDIUM,
    FREQ_HIGH,
    PHASE_HIGH,
    WAVE_ADDRESS,
    VOLUME
};

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Namco 106 Chip module.
struct Chip106 : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, Namco106::OSC_COUNT),
        ENUMS(PARAM_VOLUME, Namco106::OSC_COUNT),
        PARAM_NUM_CHANNELS,
        PARAM_WAVETABLE,
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, Namco106::OSC_COUNT),
        ENUMS(INPUT_FM, Namco106::OSC_COUNT),
        ENUMS(INPUT_VOLUME, Namco106::OSC_COUNT),
        INPUT_NUM_CHANNELS,
        INPUT_WAVETABLE,
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, Namco106::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds {
        ENUMS(LIGHT_CHANNEL, Namco106::OSC_COUNT),
        LIGHT_COUNT
    };

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
        configParam(PARAM_NUM_CHANNELS, 1, 8, 4, "Active Channels",  "");
        configParam(PARAM_WAVETABLE, 1, 5, 1, "Waveform Morph", "");
        // set the output buffer for each individual voice
        for (int i = 0; i < Namco106::OSC_COUNT; i++) {
            configParam(PARAM_FREQ + i, -30.f, 30.f, 0.f, "Channel Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
            configParam(PARAM_VOLUME + i, 0, 15, 15, "Channel Volume",  "%", 0, 100.f / 15.f);
            apu.osc_output(i, &buf[i]);
            buf[i].set_clock_rate(CLOCK_RATE);
        }
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.volume(3.f);
    }

    /// Set the frequency for a given channel.
    ///
    /// @param channel the channel to set the frequency for
    /// @param num_channels the number of enabled channels in [1, 8]
    /// @param wave_address the address of the waveform for the channel
    ///
    void setFrequency(int channel, uint8_t num_channels = 1, uint8_t wave_address = 0) {
        // get the frequency of the oscillator from the parameter and CVs
        float pitch = params[PARAM_FREQ + channel].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + channel].getVoltage();
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        static constexpr float FM_SCALE = 5.f;
        freq += FM_SCALE * inputs[INPUT_FM + channel].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to the 8-bit value for the oscillator
        auto wave_length = 64 - (num_samples / 4);
        // changing num_channels to 1 allows the standard 103 function where
        // additional channels reduce the frequency of all channels
        freq *= (wave_length * 1 * 15.f * 65536.f) / CLOCK_RATE;
        // clamp within the legal bounds for the frequency value
        freq = rack::clamp(freq, 4.f, 262143.f);
        // extract the low, medium, and high frequency register values
        auto freq18bit = static_cast<uint32_t>(freq);
        // FREQUENCY LOW
        uint8_t low = (freq18bit & 0b000000000011111111) >> 0;
        apu.write_addr(FREQ_LOW + 8 * channel);
        apu.write_data(0, low);
        // FREQUENCY MEDIUM
        uint8_t med = (freq18bit & 0b001111111100000000) >> 8;
        apu.write_addr(FREQ_MEDIUM + 8 * channel);
        apu.write_data(0, med);
        // WAVEFORM LENGTH + FREQUENCY HIGH
        uint8_t hig = (freq18bit & 0b110000000000000000) >> 16;
        apu.write_addr(FREQ_HIGH + 8 * channel);
        apu.write_data(0, (wave_length << 2) | hig);
        // WAVE ADDRESS
        apu.write_addr(WAVE_ADDRESS + 8 * channel);
        apu.write_data(0, wave_address);
        // VOLUME (and channel selection on channel 8, no effect elsewhere)
        // the minimal value for the volume width register
        static constexpr float VOLUME_MIN = 0;
        // the maximal value for the volume width register
        static constexpr float VOLUME_MAX = 15;
        // get the volume from the parameter knob
        auto levelParam = params[PARAM_VOLUME + channel].getValue();
        // apply the control voltage to the volume
        if (inputs[INPUT_VOLUME + channel].isConnected())
            levelParam *= inputs[INPUT_VOLUME + channel].getVoltage() / 2.f;
        // get the 8-bit volume clamped within legal limits
        uint8_t volume = rack::clamp(levelParam, VOLUME_MIN, VOLUME_MAX);
        apu.write_addr(VOLUME + 8 * channel);
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
        // get the number of active channels
        float num_channels = params[PARAM_NUM_CHANNELS].getValue();
        num_channels += inputs[INPUT_NUM_CHANNELS].getVoltage();
        num_channels = rack::math::clamp(num_channels, 1.f, 8.f);
        // set the frequency for all channels
        for (int i = 0; i < Namco106::OSC_COUNT; i++)
            setFrequency(i, num_channels);
        // set the output from the oscillators (in reverse order)
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < Namco106::OSC_COUNT; i++) {
            buf[i].end_frame(cycles_per_sample);
            outputs[i].setVoltage(getAudioOut(i));
        }
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override { new_sample_rate = true; }

    /// Convert the module's state to a JSON object.
    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_t* array = json_array();
        for (int i = 0; i < num_samples; i++)
            json_array_append_new(array, json_integer(values[i]));
        json_object_set_new(rootJ, "values", array);
        return rootJ;
    }

    /// Load the module's state from a JSON object.
    void dataFromJson(json_t* rootJ) override {
        json_t* data = json_object_get(rootJ, "values");
        if (data) {
            for (int i = 0; i < num_samples; i++)
                values[i] = json_integer_value(json_array_get(data, i));
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The widget structure that lays out the panel of the module and the UI menus.
struct Chip106Widget : ModuleWidget {
    Chip106Widget(Chip106 *module) {
        setModule(module);
        static constexpr auto panel = "res/106.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // if the module is displaying in/being rendered for the library, the
        // module will be null and a dummy waveform is displayed
        static uint8_t default_values[Chip106::num_samples] = {
            0xA,0x8,0xD,0xC,0xE,0xE,0xF,0xF,0xF,0xF,0xE,0xF,0xD,0xE,0xA,0xC,
            0x5,0x8,0x2,0x3,0x1,0x1,0x0,0x0,0x0,0x0,0x1,0x0,0x2,0x1,0x5,0x3
        };
        auto module_ = reinterpret_cast<Chip106*>(this->module);
        // the number of editors on the module
        static constexpr int num_editors = 5;
        // the fill colors for the wave-ttable editor lines
        static constexpr NVGcolor colors[num_editors] = {
            {{{1.f, 0.f, 0.f, 1.f}}},  // red
            {{{0.f, 1.f, 0.f, 1.f}}},  // green
            {{{0.f, 0.f, 1.f, 1.f}}},  // blue
            {{{1.f, 1.f, 0.f, 1.f}}},  // yellow
            {{{1.f, 1.f, 1.f, 1.f}}}   // white
        };
        // add wave-table editors
        for (int i = 0; i < num_editors; i++) {
            // get the wavetable buffer for this editor
            uint8_t* wavetable = module ? &module_->values[0] : &default_values[0];
            // setup a table editor for the buffer
            auto table_editor = new WaveTableEditor<uint8_t>(
                wavetable,             // wave-table buffer
                Chip106::num_samples,  // wave-table length
                Chip106::bit_depth,    // waveform bit depth
                Vec(10, 26 + 67 * i),  // position
                Vec(135, 60),          // size
                colors[i]              // line fill color
            );
            // add the table editor to the module
            addChild(table_editor);
        }
        // channel select
        addParam(createParam<Rogan3PSNES>(Vec(157, 38), module, Chip106::PARAM_NUM_CHANNELS));
        addInput(createInput<PJ301MPort>(Vec(166, 95), module, Chip106::INPUT_NUM_CHANNELS));
        // wave-table morph
        addParam(createParam<Rogan3PSNES>(Vec(157, 148), module, Chip106::PARAM_WAVETABLE));
        addInput(createInput<PJ301MPort>(Vec(166, 205), module, Chip106::INPUT_WAVETABLE));
        // individual channel controls
        for (int i = 0; i < Namco106::OSC_COUNT; i++) {
            addInput(createInput<PJ301MPort>(  Vec(212, 35 + i * 41), module, Chip106::INPUT_VOCT + i    ));
            addInput(createInput<PJ301MPort>(  Vec(242, 35 + i * 41), module, Chip106::INPUT_FM + i      ));
            addParam(createParam<Rogan2PSNES>( Vec(275, 35 + i * 41), module, Chip106::PARAM_FREQ + i    ));
            addInput(createInput<PJ301MPort>(  Vec(317, 35 + i * 41), module, Chip106::INPUT_VOLUME + i  ));
            addParam(createParam<Rogan2PSNES>( Vec(350, 35 + i * 41), module, Chip106::PARAM_VOLUME + i  ));
            addOutput(createOutput<PJ301MPort>(Vec(392, 35 + i * 41), module, Chip106::OUTPUT_CHANNEL + i));
            addChild(createLight<SmallLight<WhiteLight>>(Vec(415, 52 + i * 41), module, Chip106::LIGHT_CHANNEL + i));
        }
    }
};

/// the global instance of the model
Model *modelChip106 = createModel<Chip106, Chip106Widget>("106");
