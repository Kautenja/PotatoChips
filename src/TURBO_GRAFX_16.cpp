// A NEC Turbo-Grafx-16 Chip module.
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
#include "widget/wavetable_editor.hpp"
#include "dsp/nec_turbo_grafx_16.hpp"

/// the default values for the wave-table
const uint8_t default_values[32] = {
    0xA,0x8,0xD,0xC,0xE,0xE,0xF,0xF,0xF,0xF,0xE,0xF,0xD,0xE,0xA,0xC,
    0x5,0x8,0x2,0x3,0x1,0x1,0x0,0x0,0x0,0x0,0x1,0x0,0x2,0x1,0x5,0x3
};

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A NEC Turbo-Grafx-16 Chip module.
struct ChipTurboGrafx16 : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, NECTurboGrafx16::OSC_COUNT),
        ENUMS(PARAM_VOLUME, NECTurboGrafx16::OSC_COUNT),
        PARAM_NUM_CHANNELS, PARAM_NUM_CHANNELS_ATT,
        PARAM_WAVETABLE, PARAM_WAVETABLE_ATT,
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, NECTurboGrafx16::OSC_COUNT),
        ENUMS(INPUT_FM, NECTurboGrafx16::OSC_COUNT),
        ENUMS(INPUT_VOLUME, NECTurboGrafx16::OSC_COUNT),
        INPUT_NUM_CHANNELS,
        INPUT_WAVETABLE,
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, NECTurboGrafx16::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds {
        ENUMS(LIGHT_CHANNEL, NECTurboGrafx16::OSC_COUNT),
        LIGHT_COUNT
    };

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[NECTurboGrafx16::OSC_COUNT];
    /// The NEC Turbo-Grafx-16 instance to synthesize sound with
    NECTurboGrafx16 apu;
    /// the number of active channels
    int num_channels = 1;

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

    /// the bit-depth of the wave-table
    static constexpr auto bit_depth = 15;
    /// the number of samples in the wave-table
    static constexpr auto num_samples = 32;
    /// the samples in the wave-table (1)
    uint8_t values0[num_samples];
    /// the samples in the wave-table (2)
    uint8_t values1[num_samples];
    /// the samples in the wave-table (3)
    uint8_t values2[num_samples];
    /// the samples in the wave-table (4)
    uint8_t values3[num_samples];
    /// the samples in the wave-table (5)
    uint8_t values4[num_samples];

    // the number of editors on the module
    static constexpr int num_wavetables = 5;
    /// the wave-tables to morph between
    uint8_t* values[num_wavetables] = {
        values0,
        values1,
        values2,
        values3,
        values4
    };

    /// Initialize a new NEC Turbo-Grafx-16 Chip module.
    ChipTurboGrafx16() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_NUM_CHANNELS, 1, 8, 4, "Active Channels");
        configParam(PARAM_NUM_CHANNELS_ATT, -1, 1, 0, "Active Channels Attenuverter");
        configParam(PARAM_WAVETABLE, 1, 5, 1, "Wavetable Morph");
        configParam(PARAM_WAVETABLE_ATT, -1, 1, 0, "Wavetable Morph Attenuverter");
        cvDivider.setDivision(16);
        // set the output buffer for each individual voice
        for (int i = 0; i < NECTurboGrafx16::OSC_COUNT; i++) {
            auto descFreq = "Channel " + std::to_string(i + 1) + " Frequency";
            configParam(PARAM_FREQ + i, -30.f, 30.f, 0.f, descFreq,  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
            auto descVol = "Channel " + std::to_string(i + 1) + " Volume";
            configParam(PARAM_VOLUME + i, 0, 31, 31, descVol,  "%", 0, 100.f / 31.f);
            apu.set_output(i, &buf[i], &buf[i], &buf[i]);
        }
        // set the wave-forms to the default values
        for (int i = 0; i < num_wavetables; i++)
            memcpy(values[i], default_values, num_samples);
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.set_volume(3.f);



        // TODO: move
        for (int i = 0; i < NECTurboGrafx16::OSC_COUNT; i++) {
            // select channel 0
            apu.write(0x0800, i);
            // clear to write wave data
            apu.write(0x0804, 0b00000000);
            // write the wave-table
            for (int sample = 0; sample < num_samples; sample++)
                apu.write(0x0806, values[0][sample]);
        }
    }

    // /// Return the active channels parameter.
    // ///
    // /// @returns the active channel count in [1, 8]
    // ///
    // inline uint8_t getActiveChannels() {
    //     auto param = params[PARAM_NUM_CHANNELS].getValue();
    //     auto att = params[PARAM_NUM_CHANNELS_ATT].getValue();
    //     // get the CV as 1V per channel
    //     auto cv = 8.f * inputs[INPUT_NUM_CHANNELS].getVoltage() / 10.f;
    //     // channels are indexed maths style on the chip, not CS style
    //     return rack::math::clamp(param + att * cv, 1.f, 8.f);
    // }

    // /// Return the wave-table parameter.
    // ///
    // /// @returns the floating index of the wave-table table in [0, 4]
    // ///
    // inline float getWavetable() {
    //     auto param = params[PARAM_WAVETABLE].getValue();
    //     auto att = params[PARAM_WAVETABLE_ATT].getValue();
    //     // get the CV as 1V per wave-table
    //     auto cv = inputs[INPUT_WAVETABLE].getVoltage() / 2.f;
    //     // wave-tables are indexed maths style on panel, subtract 1 for CS style
    //     return rack::math::clamp(param + att * cv, 1.f, 5.f) - 1;
    // }

    /// Return the frequency for the given channel.
    ///
    /// @param channel the index of the channel to get the frequency of
    /// @returns the 12-bit frequency in a 16-bit container
    ///
    inline uint16_t getFrequency(int channel) {
        // TODO update min max for Freq and Level
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ12BIT_MIN = 4;
        // the maximal value for the frequency register
        static constexpr float FREQ12BIT_MAX = 8191;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // the constant modulation factor
        static constexpr auto MOD_FACTOR = 10.f;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + channel].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + channel].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM + channel].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = (buf[channel].get_clock_rate() / (CLOCK_DIVISION * freq)) - 1;
        return rack::clamp(freq, FREQ12BIT_MIN, FREQ12BIT_MAX);
    }

    /// Return the volume parameter for the given channel.
    ///
    /// @param channel the channel to get the volume parameter for
    /// @returns the volume parameter for the given channel. This includes
    /// the value of the knob and any CV modulation.
    ///
    inline uint8_t getVolume(uint8_t channel) {
        // the minimal value for the volume width register
        static constexpr float VOLUME_MIN = 0;
        // the maximal value for the volume width register
        static constexpr float VOLUME_MAX = 31;
        // get the volume from the parameter knob
        auto levelParam = params[PARAM_VOLUME + channel].getValue();
        // apply the control voltage to the volume
        static constexpr float FM_SCALE = 0.5f;
        if (inputs[INPUT_VOLUME + channel].isConnected())
            levelParam *= FM_SCALE * inputs[INPUT_VOLUME + channel].getVoltage();
        // get the 8-bit volume clamped within legal limits
        return rack::clamp(levelParam, VOLUME_MIN, VOLUME_MAX);
    }

    /// Return a 10V signed sample from the chip.
    ///
    /// @param channel the channel to get the audio sample for
    ///
    float getAudioOut(uint8_t channel) {
        // the peak to peak output of the voltage
        static constexpr float Vpp = 10.f;
        // the amount of voltage per increment of 16-bit fidelity volume
        static constexpr float divisor = std::numeric_limits<int16_t>::max();
        // convert the 16-bit sample to 10Vpp floating point
        return Vpp * buf[channel].read_sample() / divisor;
    }

    /// Process a sample.
    void process(const ProcessArgs &args) override {
        // calculate the number of clock cycles on the chip per audio sample
        uint32_t cycles_per_sample = CLOCK_RATE / args.sampleRate;
        // check for sample rate changes from the engine to send to the chip
        if (new_sample_rate) {
            // update the buffer for each channel
            for (int i = 0; i < NECTurboGrafx16::OSC_COUNT; i++)
                buf[i].set_sample_rate(args.sampleRate, CLOCK_RATE);
            // clear the new sample rate flag
            new_sample_rate = false;
        }
        if (cvDivider.process()) {
            // set the main amplifier level
            apu.write(0x0801, 0b11111111);
            // set the channel values
            for (int i = 0; i < NECTurboGrafx16::OSC_COUNT; i++) {
                // select the i'th channel
                apu.write(0x0800, i);
                // frequency
                auto freq = getFrequency(i);
                auto lo =  freq & 0b0000000011111111;
                apu.write(0x0802, lo);
                auto hi = (freq & 0b0000111100000000) >> 8;
                apu.write(0x0803, hi);
                // volume
                auto volume = getVolume(i);
                apu.write(0x0804, 0b10000000 | volume);
                // balance
                apu.write(0x0805, 0b11111111);
                // noise
                // apu.write(0x0807, 0b11111111);
            }

            // // write the waveform data to the chip's RAM
            // auto wavetable = getWavetable();
            // // calculate the address of the base waveform in the table
            // int wavetable0 = floor(wavetable);
            // // calculate the address of the next waveform in the table
            // int wavetable1 = ceil(wavetable);
            // // calculate floating point offset between the base and next table
            // float interpolate = wavetable - wavetable0;
            // for (int i = 0; i < num_samples / 2; i++) {  // iterate over nibbles
            //     apu.write_addr(i);
            //     // get the first waveform data
            //     auto nibbleHi0 = values[wavetable0][2 * i];
            //     auto nibbleLo0 = values[wavetable0][2 * i + 1];
            //     // get the second waveform data
            //     auto nibbleHi1 = values[wavetable1][2 * i];
            //     auto nibbleLo1 = values[wavetable1][2 * i + 1];
            //     // floating point interpolation
            //     uint8_t nibbleHi = ((1.f - interpolate) * nibbleHi0 + interpolate * nibbleHi1);
            //     uint8_t nibbleLo = ((1.f - interpolate) * nibbleLo0 + interpolate * nibbleLo1);
            //     // combine the two nibbles into a byte for the RAM
            //     apu.write_data(0, (nibbleHi << 4) | nibbleLo);
            // }
        }
        // process audio samples on the chip engine
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < NECTurboGrafx16::OSC_COUNT; i++)
            outputs[i].setVoltage(getAudioOut(i));
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override { new_sample_rate = true; }

    // /// Respond to the user resetting the module with the "Initialize" action.
    // void onReset() override {
    //     for (int i = 0; i < num_wavetables; i++)
    //         memcpy(values[i], default_values, num_samples);
    // }

    // /// Respond to the user randomizing the module with the "Randomize" action.
    // void onRandomize() override {
    //     for (int table = 0; table < num_wavetables; table++) {
    //         for (int sample = 0; sample < num_samples; sample++) {
    //             values[table][sample] = random::u32() % bit_depth;
    //             // interpolate between random samples to smooth slightly
    //             if (sample > 0) {
    //                 auto last = values[table][sample - 1];
    //                 auto next = values[table][sample];
    //                 values[table][sample] = (last + next) / 2;
    //             }
    //         }
    //     }
    // }

    // /// Convert the module's state to a JSON object.
    // json_t* dataToJson() override {
    //     json_t* rootJ = json_object();
    //     for (int table = 0; table < num_wavetables; table++) {
    //         json_t* array = json_array();
    //         for (int sample = 0; sample < num_samples; sample++)
    //             json_array_append_new(array, json_integer(values[table][sample]));
    //         auto key = "values" + std::to_string(table);
    //         json_object_set_new(rootJ, key.c_str(), array);
    //     }
    //
    //     return rootJ;
    // }

    // /// Load the module's state from a JSON object.
    // void dataFromJson(json_t* rootJ) override {
    //     for (int table = 0; table < num_wavetables; table++) {
    //         auto key = "values" + std::to_string(table);
    //         json_t* data = json_object_get(rootJ, key.c_str());
    //         if (data) {
    //             for (int sample = 0; sample < num_samples; sample++)
    //                 values[table][sample] = json_integer_value(json_array_get(data, sample));
    //         }
    //     }
    // }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The widget structure that lays out the panel of the module and the UI menus.
struct ChipTurboGrafx16Widget : ModuleWidget {
    ChipTurboGrafx16Widget(ChipTurboGrafx16 *module) {
        setModule(module);
        static constexpr auto panel = "res/TURBO_GRAFX_16.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // if the module is displaying in/being rendered for the library, the
        // module will be null and a dummy waveform is displayed
        static uint8_t library_values[ChipTurboGrafx16::num_samples] = {
            0xA,0x8,0xD,0xC,0xE,0xE,0xF,0xF,0xF,0xF,0xE,0xF,0xD,0xE,0xA,0xC,
            0x5,0x8,0x2,0x3,0x1,0x1,0x0,0x0,0x0,0x0,0x1,0x0,0x2,0x1,0x5,0x3
        };
        auto module_ = reinterpret_cast<ChipTurboGrafx16*>(this->module);
        // the fill colors for the wave-table editor lines
        static constexpr NVGcolor colors[ChipTurboGrafx16::num_wavetables] = {
            {{{1.f, 0.f, 0.f, 1.f}}},  // red
            {{{0.f, 1.f, 0.f, 1.f}}},  // green
            {{{0.f, 0.f, 1.f, 1.f}}},  // blue
            {{{1.f, 1.f, 0.f, 1.f}}},  // yellow
            {{{1.f, 1.f, 1.f, 1.f}}}   // white
        };
        // add wave-table editors
        for (int i = 0; i < ChipTurboGrafx16::num_wavetables; i++) {
            // get the wave-table buffer for this editor
            uint8_t* wavetable = module ? &module_->values[i][0] : &library_values[0];
            // setup a table editor for the buffer
            auto table_editor = new WaveTableEditor<uint8_t>(
                wavetable,             // wave-table buffer
                ChipTurboGrafx16::num_samples,  // wave-table length
                ChipTurboGrafx16::bit_depth,    // waveform bit depth
                Vec(10, 26 + 67 * i),  // position
                Vec(135, 60),          // size
                colors[i]              // line fill color
            );
            // add the table editor to the module
            addChild(table_editor);
        }
        // channel select
        addParam(createParam<Rogan3PSNES>(Vec(155, 38), module, ChipTurboGrafx16::PARAM_NUM_CHANNELS));
        addParam(createParam<Rogan1PSNES>(Vec(161, 88), module, ChipTurboGrafx16::PARAM_NUM_CHANNELS_ATT));
        addInput(createInput<PJ301MPort>(Vec(164, 126), module, ChipTurboGrafx16::INPUT_NUM_CHANNELS));
        // wave-table morph
        addParam(createParam<Rogan3PSNES>(Vec(155, 183), module, ChipTurboGrafx16::PARAM_WAVETABLE));
        addParam(createParam<Rogan1PSNES>(Vec(161, 233), module, ChipTurboGrafx16::PARAM_WAVETABLE_ATT));
        addInput(createInput<PJ301MPort>(Vec(164, 271), module, ChipTurboGrafx16::INPUT_WAVETABLE));
        // individual channel controls
        for (int i = 0; i < NECTurboGrafx16::OSC_COUNT; i++) {
            addInput(createInput<PJ301MPort>(  Vec(212, 40 + i * 41), module, ChipTurboGrafx16::INPUT_VOCT + i    ));
            addInput(createInput<PJ301MPort>(  Vec(242, 40 + i * 41), module, ChipTurboGrafx16::INPUT_FM + i      ));
            addParam(createParam<Rogan2PSNES>( Vec(275, 35 + i * 41), module, ChipTurboGrafx16::PARAM_FREQ + i    ));
            addInput(createInput<PJ301MPort>(  Vec(317, 40 + i * 41), module, ChipTurboGrafx16::INPUT_VOLUME + i  ));
            addParam(createParam<Rogan2PSNES>( Vec(350, 35 + i * 41), module, ChipTurboGrafx16::PARAM_VOLUME + i  ));
            addOutput(createOutput<PJ301MPort>(Vec(392, 40 + i * 41), module, ChipTurboGrafx16::OUTPUT_CHANNEL + i));
            addChild(createLight<SmallLight<WhiteLight>>(Vec(415, 60 + i * 41), module, ChipTurboGrafx16::LIGHT_CHANNEL + i));
        }
    }
};

/// the global instance of the model
Model *modelChipTurboGrafx16 = createModel<ChipTurboGrafx16, ChipTurboGrafx16Widget>("TURBO_GRAFX_16");
