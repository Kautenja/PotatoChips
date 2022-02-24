// A Namco 163 Chip module.
// Copyright 2020 Christian Kauten
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
#include "engine/chip_module.hpp"
#include "dsp/math.hpp"
#include "dsp/namco_163.hpp"
#include "dsp/wavetable4bit.hpp"
#include "widget/wavetable_editor.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Namco 163 chip emulator module.
struct NameCorpOctalWaveGenerator : ChipModule<Namco163> {
 private:
    /// the number of active oscillators on the chip
    unsigned num_oscillators[PORT_MAX_CHANNELS];

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ, Namco163::OSC_COUNT),
        ENUMS(PARAM_FM, Namco163::OSC_COUNT),
        ENUMS(PARAM_VOLUME, Namco163::OSC_COUNT),
        PARAM_NUM_OSCILLATORS,
        PARAM_NUM_OSCILLATORS_ATT,
        PARAM_WAVETABLE,
        PARAM_WAVETABLE_ATT,
        NUM_PARAMS
    };
    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT, Namco163::OSC_COUNT),
        ENUMS(INPUT_FM, Namco163::OSC_COUNT),
        ENUMS(INPUT_VOLUME, Namco163::OSC_COUNT),
        INPUT_NUM_OSCILLATORS,
        INPUT_WAVETABLE,
        NUM_INPUTS
    };
    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_OSCILLATOR, Namco163::OSC_COUNT),
        NUM_OUTPUTS
    };
    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(LIGHT_CHANNEL, 3 * Namco163::OSC_COUNT),
        ENUMS(LIGHT_LEVEL, 3 * Namco163::OSC_COUNT),
        NUM_LIGHTS
    };

    /// the bit-depth of the wave-table
    static constexpr auto BIT_DEPTH = 15;
    /// the number of samples in the wave-table
    static constexpr auto SAMPLES_PER_WAVETABLE = 32;
    /// the number of editors on the module
    static constexpr int NUM_WAVEFORMS = 5;

    /// the wave-tables to morph between
    uint8_t wavetable[NUM_WAVEFORMS][SAMPLES_PER_WAVETABLE];

    /// @brief Initialize a new 106 Chip module.
    NameCorpOctalWaveGenerator() : ChipModule<Namco163>() {
        normal_outputs = true;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(PARAM_NUM_OSCILLATORS,      1, Namco163::OSC_COUNT, 4, "Active Channels");
        configParam(PARAM_NUM_OSCILLATORS_ATT, -1, 1,                   0, "Active Channels Attenuverter");
        configInput(INPUT_NUM_OSCILLATORS, "Active Channels");
        configParam(PARAM_WAVETABLE,            1, NUM_WAVEFORMS,       1, "Waveform Morph");
        configParam(PARAM_WAVETABLE_ATT,       -1, 1,                   0, "Waveform Morph Attenuverter");
        configInput(INPUT_WAVETABLE, "Waveform Morph");
        // set the output buffer for each individual voice
        for (unsigned osc = 0; osc < Namco163::OSC_COUNT; osc++) {
            auto osc_name = "Voice " + std::to_string(osc + 1);
            configParam(PARAM_FREQ + osc, -2.5f, 2.5f, 0.f, osc_name + " Frequency", " Hz", 2, rack::dsp::FREQ_C4);
            configParam(PARAM_FM + osc, -1.f, 1.f, 0.f, osc_name + " FM");
            configParam(PARAM_VOLUME + osc, 0, 15, 15, osc_name + " Volume");
            configInput(INPUT_VOCT + osc, osc_name + " V/Oct");
            configInput(INPUT_FM + osc, osc_name + " FM");
            configInput(INPUT_VOLUME + osc, osc_name + " Volume");
            configOutput(OUTPUT_OSCILLATOR + osc, osc_name + " Audio");
        }
        memset(num_oscillators, 1, sizeof num_oscillators);
        resetWavetable();
    }

    /// Reset the waveform table to the default state.
    void resetWavetable() {
        static constexpr uint8_t* WAVETABLE[NUM_WAVEFORMS] = {
            SINE,
            PW5,
            RAMP_UP,
            TRIANGLE_DIST,
            RAMP_DOWN
        };
        for (unsigned i = 0; i < NUM_WAVEFORMS; i++)
            memcpy(wavetable[i], WAVETABLE[i], SAMPLES_PER_WAVETABLE);
    }

    /// @brief Respond to the module being reset by the host environment.
    void onReset() final {
        ChipModule<Namco163>::onReset();
        resetWavetable();
    }

    /// @brief Respond to parameter randomization by the host environment.
    void onRandomize() final {
        for (unsigned table = 0; table < NUM_WAVEFORMS; table++) {
            for (unsigned sample = 0; sample < SAMPLES_PER_WAVETABLE; sample++) {
                wavetable[table][sample] = random::u32() % BIT_DEPTH;
                // interpolate between random samples to smooth slightly
                if (sample > 0) {
                    auto last = wavetable[table][sample - 1];
                    auto next = wavetable[table][sample];
                    wavetable[table][sample] = (last + next) / 2;
                }
            }
        }
    }

    /// @brief Convert the module's state to a JSON object.
    json_t* dataToJson() final {
        json_t* rootJ = json_object();
        for (int table = 0; table < NUM_WAVEFORMS; table++) {
            json_t* array = json_array();
            for (int sample = 0; sample < SAMPLES_PER_WAVETABLE; sample++)
                json_array_append_new(array, json_integer(wavetable[table][sample]));
            auto key = "wavetable" + std::to_string(table);
            json_object_set_new(rootJ, key.c_str(), array);
        }

        return rootJ;
    }

    /// @brief Load the module's state from a JSON object.
    void dataFromJson(json_t* rootJ) final {
        for (int table = 0; table < NUM_WAVEFORMS; table++) {
            auto key = "wavetable" + std::to_string(table);
            json_t* data = json_object_get(rootJ, key.c_str());
            if (data) {
                for (int sample = 0; sample < SAMPLES_PER_WAVETABLE; sample++)
                    wavetable[table][sample] = json_integer_value(json_array_get(data, sample));
            }
        }
    }

 protected:
    /// @brief Return the active oscillators parameter.
    ///
    /// @param channel the polyphonic channel to return the active oscillators for
    /// @returns the active oscillator count \f$\in [1, 8]\f$
    ///
    inline uint8_t getActiveOscillators(unsigned channel) {
        auto param = params[PARAM_NUM_OSCILLATORS].getValue();
        auto att = params[PARAM_NUM_OSCILLATORS_ATT].getValue();
        // get the CV as 1V per oscillator
        auto cv = 8.f * Math::Eurorack::fromDC(inputs[INPUT_NUM_OSCILLATORS].getPolyVoltage(channel));
        // oscillators are indexed maths style on the chip, not CS style
        return Math::clip(param + att * cv, 1.f, 8.f);
    }

    /// @brief Return the wave-table position parameter.
    ///
    /// @param channel the polyphonic channel to return the wavetable position for
    /// @returns the floating index of the wave-table position \f$\in [0, 4]\f$
    ///
    inline float getWavetablePosition(unsigned channel) {
        auto param = params[PARAM_WAVETABLE].getValue();
        auto att = params[PARAM_WAVETABLE_ATT].getValue();
        // get the CV as 1V per wave-table
        auto cv = rescale(inputs[INPUT_WAVETABLE].getVoltage(channel), -7.f, 7.f, -5.f, 5.f);
        // wave-tables are indexed maths style on panel, subtract 1 for CS style
        return Math::clip(param + att * cv, 1.f, 5.f) - 1;
    }

    /// @brief Return the frequency parameter for the given oscillator.
    ///
    /// @param oscillator the oscillator to get the frequency parameter for
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the frequency parameter for the given oscillator. This includes
    /// the value of the knob and any CV modulation.
    ///
    inline uint32_t getFrequency(unsigned oscillator, unsigned channel) {
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + oscillator].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 0V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normalPitch = oscillator ? inputs[INPUT_VOCT + oscillator - 1].getVoltage(channel) : 0.f;
        const auto pitchCV = inputs[INPUT_VOCT + oscillator].getNormalVoltage(normalPitch, channel);
        inputs[INPUT_VOCT + oscillator].setVoltage(pitchCV, channel);
        pitch += pitchCV;
        // get the attenuverter parameter value
        const auto att = params[PARAM_FM + oscillator].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 5V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normalMod = oscillator ? inputs[INPUT_FM + oscillator - 1].getVoltage(channel) : 5.f;
        const auto mod = inputs[INPUT_FM + oscillator].getNormalVoltage(normalMod, channel);
        inputs[INPUT_FM + oscillator].setVoltage(mod, channel);
        pitch += att * mod / 5.f;
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq = Math::clip(freq, 0.0f, 20000.0f);
        // convert the frequency to the 8-bit value for the oscillator
        static constexpr uint32_t wave_length = 64 - (SAMPLES_PER_WAVETABLE / 4);
        // ignoring num_oscillators in the calculation allows the standard 103
        // function where additional oscillators reduce the frequency of all
        freq *= (wave_length * 15.f * 65536.f) / buffers[channel][oscillator].get_clock_rate();
        // clamp within the legal bounds for the frequency value
        freq = Math::clip(freq, 512.f, 262143.f);
        // OR the waveform length into the high 6 bits of "frequency Hi"
        // register, which is the third bite, i.e. shift left 2 + 16
        return static_cast<uint32_t>(freq) | (wave_length << 18);
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
        static constexpr float MIN = 0;
        // the maximal value for the volume width register
        static constexpr float MAX = 15;
        // get the level from the parameter knob
        auto level = params[PARAM_VOLUME + oscillator].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 10V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normal = oscillator ? inputs[INPUT_VOLUME + oscillator - 1].getVoltage(channel) : 10.f;
        const auto voltage = inputs[INPUT_VOLUME + oscillator].getNormalVoltage(normal, channel);
        inputs[INPUT_VOLUME + oscillator].setVoltage(voltage, channel);
        // apply the control voltage to the level. Normal to a constant
        // 10V source instead of checking if the cable is connected
        level = roundf(level * Math::Eurorack::fromDC(voltage));
        // get the 8-bit attenuation by inverting the level and clipping
        // to the legal bounds of the parameter
        return Math::clip(level, MIN, MAX);
    }

    /// @brief Process the audio rate inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the audio inputs to
    ///
    inline void processAudio(const ProcessArgs& args, const unsigned& channel) final {
        // set the frequency for all oscillators on the chip
        for (unsigned oscillator = 0; oscillator < Namco163::OSC_COUNT; oscillator++) {
            // extract the low, medium, and high frequency register values
            auto freq = getFrequency(oscillator, channel);
            // FREQUENCY LOW
            uint8_t low = (freq & 0b000000000000000011111111) >> 0;
            apu[channel].write(Namco163::FREQ_LOW + Namco163::REGS_PER_VOICE * oscillator, low);
            // FREQUENCY MEDIUM
            uint8_t med = (freq & 0b000000001111111100000000) >> 8;
            apu[channel].write(Namco163::FREQ_MEDIUM + Namco163::REGS_PER_VOICE * oscillator, med);
            // WAVEFORM LENGTH + FREQUENCY HIGH
            uint8_t hig = (freq & 0b111111110000000000000000) >> 16;
            apu[channel].write(Namco163::FREQ_HIGH + Namco163::REGS_PER_VOICE * oscillator, hig);
        }
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processCV(const ProcessArgs& args, const unsigned& channel) final {
        // get the number of active oscillators from the panel
        num_oscillators[channel] = getActiveOscillators(channel);
        // set the frequency for all oscillators on the chip
        for (unsigned oscillator = 0; oscillator < Namco163::OSC_COUNT; oscillator++) {
            // WAVE ADDRESS
            apu[channel].write(Namco163::WAVE_ADDRESS + Namco163::REGS_PER_VOICE * oscillator, 0);
            // VOLUME (and oscillator selection on oscillator 8, this has
            // no effect on other oscillators, so check logic is skipped)
            apu[channel].write(Namco163::VOLUME + Namco163::REGS_PER_VOICE * oscillator, ((num_oscillators[channel] - 1) << 4) | getVolume(oscillator, channel));
        }
        // write waveform data to the chip's RAM based on the position in
        // the wave-table
        auto position = getWavetablePosition(channel);
        // calculate the address of the base waveform in the table
        int wavetable0 = floor(position);
        // calculate the address of the next waveform in the table
        int wavetable1 = ceil(position);
        // calculate floating point offset between the base and next table
        float interpolate = position - wavetable0;
        for (int i = 0; i < SAMPLES_PER_WAVETABLE / 2; i++) {  // iterate over nibbles
            // get the first waveform data
            auto nibbleLo0 = wavetable[wavetable0][2 * i];
            auto nibbleHi0 = wavetable[wavetable0][2 * i + 1];
            // get the second waveform data
            auto nibbleLo1 = wavetable[wavetable1][2 * i];
            auto nibbleHi1 = wavetable[wavetable1][2 * i + 1];
            // floating point interpolation
            uint8_t nibbleLo = ((1.f - interpolate) * nibbleLo0 + interpolate * nibbleLo1);
            uint8_t nibbleHi = ((1.f - interpolate) * nibbleHi0 + interpolate * nibbleHi1);
            // combine the two nibbles into a byte for the RAM
            apu[channel].write(i, (nibbleHi << 4) | nibbleLo);
        }
    }

    /// @brief Process the lights on the module.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channels the number of active polyphonic channels
    ///
    inline void processLights(const ProcessArgs& args, const unsigned& channels) final {
        // set the lights based on the accumulated brightness
        for (unsigned oscillator = 0; oscillator < Namco163::OSC_COUNT; oscillator++) {
            // accumulate brightness for all the channels
            float brightness = 0.f;
            for (unsigned channel = 0; channel < channels; channel++)
                brightness = brightness + (oscillator < num_oscillators[channel]);
            const auto light = LIGHT_CHANNEL + 3 * (Namco163::OSC_COUNT - oscillator - 1);
            // get the brightness level for the oscillator.  Because the
            // signal is boolean, the root mean square will have no effect.
            // Instead, the average over the channels is used as brightness
            auto level = brightness / channels;
            // set the light colors in BGR order.
            lights[light + 2].setSmoothBrightness(level, args.sampleTime * lightDivider.getDivision());
            // if there is more than one channel running (polyphonic), set
            // red and green to 0 to produce a blue LED color. This is the
            // standard for LEDs that indicate polyphonic signals in VCV
            // Rack.
            if (channels > 1) level *= 0;
            lights[light + 1].setSmoothBrightness(level, args.sampleTime * lightDivider.getDivision());
            lights[light + 0].setSmoothBrightness(level, args.sampleTime * lightDivider.getDivision());
        }

        for (unsigned voice = 0; voice < Namco163::OSC_COUNT; voice++) {
            // get the global brightness scale from -12 to 3
            auto brightness = vuMeter[voice].getBrightness(-12, 3);
            // set the red light based on total brightness and
            // brightness from 0dB to 3dB
            lights[LIGHT_LEVEL + voice * 3 + 0].setBrightness(brightness * vuMeter[voice].getBrightness(0, 3));
            // set the red light based on inverted total brightness and
            // brightness from -12dB to 0dB
            lights[LIGHT_LEVEL + voice * 3 + 1].setBrightness((1 - brightness) * vuMeter[voice].getBrightness(-12, 0));
            // set the blue light to off
            lights[LIGHT_LEVEL + voice * 3 + 2].setBrightness(0);
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for 106.
struct NameCorpOctalWaveGeneratorWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit NameCorpOctalWaveGeneratorWidget(NameCorpOctalWaveGenerator *module) {
        setModule(module);
        static constexpr auto panel = "res/NameCorpOctalWaveGenerator.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // the fill colors for the wave-table editor lines
        static constexpr NVGcolor colors[NameCorpOctalWaveGenerator::NUM_WAVEFORMS] = {
            {{{1.f, 0.f, 0.f, 1.f}}},  // red
            {{{0.f, 1.f, 0.f, 1.f}}},  // green
            {{{0.f, 0.f, 1.f, 1.f}}},  // blue
            {{{1.f, 1.f, 0.f, 1.f}}},  // yellow
            {{{1.f, 1.f, 1.f, 1.f}}}   // white
        };
        /// the default wave-table for each page of the wave-table editor
        static constexpr uint8_t* wavetables[NameCorpOctalWaveGenerator::NUM_WAVEFORMS] = {
            SINE,
            PW5,
            RAMP_UP,
            TRIANGLE_DIST,
            RAMP_DOWN
        };
        // Add wave-table editors. If the module is displaying in/being
        // rendered for the library, the module will be null and a dummy
        // waveform is displayed
        for (int waveform = 0; waveform < NameCorpOctalWaveGenerator::NUM_WAVEFORMS; waveform++) {
            // get the wave-table buffer for this editor
            uint8_t* wavetable = module ? &module->wavetable[waveform][0] : &wavetables[waveform][0];
            // setup a table editor for the buffer
            auto table_editor = new WaveTableEditor<uint8_t>(
                wavetable,                       // wave-table buffer
                NameCorpOctalWaveGenerator::SAMPLES_PER_WAVETABLE,  // wave-table length
                NameCorpOctalWaveGenerator::BIT_DEPTH,              // waveform bit depth
                Vec(10, 26 + 68 * waveform),     // position
                Vec(135, 60),                    // size
                colors[waveform],                 // line fill color
                {{{0, 0, 0, 1}}},
                {{{0, 0, 0, 1}}}
            );
            // add the table editor to the module
            addChild(table_editor);
        }
        // oscillator select
        addParam(createSnapParam<Rogan3PWhite>(Vec(156, 42), module, NameCorpOctalWaveGenerator::PARAM_NUM_OSCILLATORS));
        addParam(createParam<Trimpot>(Vec(168, 110), module, NameCorpOctalWaveGenerator::PARAM_NUM_OSCILLATORS_ATT));
        addInput(createInput<PJ301MPort>(Vec(165, 153), module, NameCorpOctalWaveGenerator::INPUT_NUM_OSCILLATORS));
        // wave-table morph
        addParam(createParam<Rogan3PWhite>(Vec(156, 214), module, NameCorpOctalWaveGenerator::PARAM_WAVETABLE));
        addParam(createParam<Trimpot>(Vec(168, 282), module, NameCorpOctalWaveGenerator::PARAM_WAVETABLE_ATT));
        addInput(createInput<PJ301MPort>(Vec(165, 325), module, NameCorpOctalWaveGenerator::INPUT_WAVETABLE));
        // individual oscillator controls
        for (unsigned i = 0; i < Namco163::OSC_COUNT; i++) {
            addInput(createInput<PJ301MPort>(  Vec(212, 40 + i * 41), module, NameCorpOctalWaveGenerator::INPUT_VOCT + i    ));
            addParam(createParam<Trimpot>(     Vec(251, 43 + i * 41), module, NameCorpOctalWaveGenerator::PARAM_FREQ + i    ));
            addParam(createParam<Trimpot>(     Vec(294, 43 + i * 41), module, NameCorpOctalWaveGenerator::PARAM_FM + i      ));
            addInput(createInput<PJ301MPort>(  Vec(328, 40 + i * 41), module, NameCorpOctalWaveGenerator::INPUT_FM + i      ));
            addInput(createInput<PJ301MPort>(  Vec(362, 40 + i * 41), module, NameCorpOctalWaveGenerator::INPUT_VOLUME + i  ));
            addParam(createParam<Trimpot>(     Vec(401, 43 + i * 41), module, NameCorpOctalWaveGenerator::PARAM_VOLUME + i  ));
            addOutput(createOutput<PJ301MPort>(Vec(437, 40 + i * 41), module, NameCorpOctalWaveGenerator::OUTPUT_OSCILLATOR + i));
            addChild(createLight<SmallLight<RedGreenBlueLight>>(Vec(431, 60 + i * 41), module, NameCorpOctalWaveGenerator::LIGHT_CHANNEL + 3 * i));
            addChild(createLight<SmallLight<RedGreenBlueLight>>(Vec(460, 60 + i * 41), module, NameCorpOctalWaveGenerator::LIGHT_LEVEL + 3 * i));
        }
    }
};

/// the global instance of the model
Model *modelNameCorpOctalWaveGenerator = createModel<NameCorpOctalWaveGenerator, NameCorpOctalWaveGeneratorWidget>("106");
