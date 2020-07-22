// A Konami SCC Chip module.
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
#include "dsp/konami_scc.hpp"
#include <cstring>

/// the default values for the wave-table
const int8_t default_values[32] = {
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92, 96, 100, 104, 108, 112, 116, 120, 124
};

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Konami SCC Chip module.
struct ChipSCC : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, KonamiSCC::OSC_COUNT),
        ENUMS(PARAM_VOLUME, KonamiSCC::OSC_COUNT),
        PARAM_NUM_CHANNELS, PARAM_NUM_CHANNELS_ATT,
        PARAM_WAVETABLE, PARAM_WAVETABLE_ATT,
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, KonamiSCC::OSC_COUNT),
        ENUMS(INPUT_FM, KonamiSCC::OSC_COUNT),
        ENUMS(INPUT_VOLUME, KonamiSCC::OSC_COUNT),
        INPUT_NUM_CHANNELS,
        INPUT_WAVETABLE,
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, KonamiSCC::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds {
        LIGHT_COUNT
    };

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[KonamiSCC::OSC_COUNT];
    /// The Konami SCC instance to synthesize sound with
    KonamiSCC apu;
    /// the number of active channels
    int num_channels = 1;

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;
    // a clock divider for running LED updates slower than audio rate
    dsp::ClockDivider lightsDivider;

    /// the bit-depth of the wave-table
    static constexpr auto bit_depth = 255;
    /// the number of samples in the wave-table
    static constexpr auto num_samples = 32;
    /// the samples in the wave-table (1)
    int8_t values0[num_samples];
    /// the samples in the wave-table (2)
    int8_t values1[num_samples];
    /// the samples in the wave-table (3)
    int8_t values2[num_samples];
    /// the samples in the wave-table (4)
    int8_t values3[num_samples];
    /// the samples in the wave-table (5)
    int8_t values4[num_samples];

    // the number of editors on the module
    static constexpr int num_wavetables = 5;
    /// the wave-tables to morph between
    int8_t* values[num_wavetables] = {
        values0,
        values1,
        values2,
        values3,
        values4
    };

    /// Initialize a new Konami SCC Chip module.
    ChipSCC() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_NUM_CHANNELS, 1, 8, 4, "Active Channels");
        configParam(PARAM_NUM_CHANNELS_ATT, -1, 1, 0, "Active Channels Attenuverter");
        configParam(PARAM_WAVETABLE, 1, 5, 1, "Wavetable Morph");
        configParam(PARAM_WAVETABLE_ATT, -1, 1, 0, "Wavetable Morph Attenuverter");
        cvDivider.setDivision(16);
        lightsDivider.setDivision(128);
        // set the output buffer for each individual voice
        for (int i = 0; i < KonamiSCC::OSC_COUNT; i++) {
            auto descFreq = "Channel " + std::to_string(i + 1) + " Frequency";
            configParam(PARAM_FREQ + i, -30.f, 30.f, 0.f, descFreq,  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
            auto descVol = "Channel " + std::to_string(i + 1) + " Volume";
            configParam(PARAM_VOLUME + i, 0, 15, 15, descVol,  "%", 0, 100.f / 15.f);
            apu.set_output(i, &buf[i]);
        }
        // set the wave-forms to the default values
        for (int i = 0; i < num_wavetables; i++)
            memcpy(values[i], default_values, num_samples);
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.set_volume(3.f);
        onSampleRateChange();
    }

    /// Return the wave-table parameter.
    ///
    /// @returns the floating index of the wave-table table in [0, 4]
    ///
    inline float getWavetable() {
        auto param = params[PARAM_WAVETABLE].getValue();
        auto att = params[PARAM_WAVETABLE_ATT].getValue();
        // get the CV as 1V per wave-table
        auto cv = inputs[INPUT_WAVETABLE].getVoltage() / 2.f;
        // wave-tables are indexed maths style on panel, subtract 1 for CS style
        return rack::math::clamp(param + att * cv, 1.f, 5.f) - 1;
    }

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
        static constexpr float VOLUME_MAX = 15;
        // get the volume from the parameter knob
        auto levelParam = params[PARAM_VOLUME + channel].getValue();
        // apply the control voltage to the volume
        static constexpr float AM_SCALE = 0.5f;
        if (inputs[INPUT_VOLUME + channel].isConnected())
            levelParam *= AM_SCALE * inputs[INPUT_VOLUME + channel].getVoltage();
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
        if (cvDivider.process()) {
            // write the waveform data to the chip's RAM
            auto wavetable = getWavetable();
            // calculate the address of the base waveform in the table
            int wavetable0 = floor(wavetable);
            // calculate the address of the next waveform in the table
            int wavetable1 = ceil(wavetable);
            // calculate floating point offset between the base and next table
            float interpolate = wavetable - wavetable0;
            for (int i = 0; i < num_samples; i++) {  // iterate over samples
                // get the first waveform data
                auto sample0 = values[wavetable0][i];
                // get the second waveform data
                auto sample1 = values[wavetable1][i];
                // floating point interpolation
                uint8_t sample = ((1.f - interpolate) * sample0 + interpolate * sample1);
                // write the waveform to the chip
                apu.write(KonamiSCC::WAVEFORM_CH_1 + i, sample);
                apu.write(KonamiSCC::WAVEFORM_CH_2 + i, sample);
                apu.write(KonamiSCC::WAVEFORM_CH_3 + i, sample);
                apu.write(KonamiSCC::WAVEFORM_CH_4 + i, sample);
            }
            // frequency
            for (int i = 0; i < KonamiSCC::OSC_COUNT; i++) {
                auto freq = getFrequency(i);
                auto lo =  freq & 0b0000000011111111;
                apu.write(KonamiSCC::FREQUENCY_CH_1_LO + 2 * i, lo);
                auto hi = (freq & 0b0000111100000000) >> 8;
                apu.write(KonamiSCC::FREQUENCY_CH_1_HI + 2 * i, hi);
                auto volume = getVolume(i);
                apu.write(KonamiSCC::VOLUME_CH_1 + i, KonamiSCC::VOLUME_ON | volume);
            }
            apu.write(KonamiSCC::POWER, KonamiSCC::POWER_ALL_ON);
        }
        // process audio samples on the chip engine
        apu.end_frame(CLOCK_RATE / args.sampleRate);
        for (int i = 0; i < KonamiSCC::OSC_COUNT; i++)
            outputs[i].setVoltage(getAudioOut(i));
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override {
        // update the buffer for each channel
        for (int i = 0; i < KonamiSCC::OSC_COUNT; i++)
            buf[i].set_sample_rate(APP->engine->getSampleRate(), CLOCK_RATE);
    }

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
struct ChipSCCWidget : ModuleWidget {
    ChipSCCWidget(ChipSCC *module) {
        setModule(module);
        static constexpr auto panel = "res/SCC.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // if the module is displaying in/being rendered for the library, the
        // module will be null and a dummy waveform is displayed
        static int8_t library_values[ChipSCC::num_samples] = {
            0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92, 96, 100, 104, 108, 112, 116, 120, 124
        };
        auto module_ = reinterpret_cast<ChipSCC*>(this->module);
        // the fill colors for the wave-table editor lines
        static constexpr NVGcolor colors[ChipSCC::num_wavetables] = {
            {{{1.f, 0.f, 0.f, 1.f}}},  // red
            {{{0.f, 1.f, 0.f, 1.f}}},  // green
            {{{0.f, 0.f, 1.f, 1.f}}},  // blue
            {{{1.f, 1.f, 0.f, 1.f}}},  // yellow
            {{{1.f, 1.f, 1.f, 1.f}}}   // white
        };
        // add wave-table editors
        for (int i = 0; i < ChipSCC::num_wavetables; i++) {
            // get the wave-table buffer for this editor
            int8_t* wavetable = module ? &module_->values[i][0] : &library_values[0];
            // setup a table editor for the buffer
            auto table_editor = new WaveTableEditor<int8_t>(
                wavetable,             // wave-table buffer
                ChipSCC::num_samples,  // wave-table length
                ChipSCC::bit_depth,    // waveform bit depth
                Vec(10, 26 + 67 * i),  // position
                Vec(135, 60),          // size
                colors[i]              // line fill color
            );
            // add the table editor to the module
            addChild(table_editor);
        }
        // channel select
        addParam(createParam<Rogan3PSNES>(Vec(155, 38), module, ChipSCC::PARAM_NUM_CHANNELS));
        addParam(createParam<Rogan1PSNES>(Vec(161, 88), module, ChipSCC::PARAM_NUM_CHANNELS_ATT));
        addInput(createInput<PJ301MPort>(Vec(164, 126), module, ChipSCC::INPUT_NUM_CHANNELS));
        // wave-table morph
        addParam(createParam<Rogan3PSNES>(Vec(155, 183), module, ChipSCC::PARAM_WAVETABLE));
        addParam(createParam<Rogan1PSNES>(Vec(161, 233), module, ChipSCC::PARAM_WAVETABLE_ATT));
        addInput(createInput<PJ301MPort>(Vec(164, 271), module, ChipSCC::INPUT_WAVETABLE));
        // individual channel controls
        for (int i = 0; i < KonamiSCC::OSC_COUNT; i++) {
            addInput(createInput<PJ301MPort>(  Vec(212, 40 + i * 41), module, ChipSCC::INPUT_VOCT + i    ));
            addInput(createInput<PJ301MPort>(  Vec(242, 40 + i * 41), module, ChipSCC::INPUT_FM + i      ));
            addParam(createParam<Rogan2PSNES>( Vec(275, 35 + i * 41), module, ChipSCC::PARAM_FREQ + i    ));
            addInput(createInput<PJ301MPort>(  Vec(317, 40 + i * 41), module, ChipSCC::INPUT_VOLUME + i  ));
            addParam(createParam<Rogan2PSNES>( Vec(350, 35 + i * 41), module, ChipSCC::PARAM_VOLUME + i  ));
            addOutput(createOutput<PJ301MPort>(Vec(392, 40 + i * 41), module, ChipSCC::OUTPUT_CHANNEL + i));
        }
    }
};

/// the global instance of the model
Model *modelChipSCC = createModel<ChipSCC, ChipSCCWidget>("SCC");
