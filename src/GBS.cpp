// A Nintendo GBS Chip module.
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
#include "dsp/nintendo_gameboy.hpp"
#include "dsp/wavetable4bit.hpp"
#include "widget/wavetable_editor.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Nintendo GBS chip emulator module.
struct ChipGBS : Module {
 private:
    /// The BLIP buffer to render audio samples from
    BLIPBuffer buffers[POLYPHONY_CHANNELS][NintendoGBS::OSC_COUNT];
    /// The GBS instance to synthesize sound with
    NintendoGBS apu[POLYPHONY_CHANNELS];

    /// a Trigger for handling inputs to the LFSR port
    dsp::BooleanTrigger lfsr[POLYPHONY_CHANNELS];
    /// a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;
    /// a VU meter for keeping track of the oscillator levels
    dsp::VuMeter2 chMeters[NintendoGBS::OSC_COUNT];
    /// a clock divider for updating the mixer LEDs
    dsp::ClockDivider lightDivider;

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ, 3),
        PARAM_NOISE_PERIOD,
        ENUMS(PARAM_PW, 2),
        PARAM_WAVETABLE,
        PARAM_LFSR,
        ENUMS(PARAM_LEVEL, NintendoGBS::OSC_COUNT),
        NUM_PARAMS
    };
    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT, 3),
        INPUT_NOISE_PERIOD,
        ENUMS(INPUT_FM, 3),
        ENUMS(INPUT_PW, 2),
        INPUT_WAVETABLE,
        INPUT_LFSR,
        ENUMS(INPUT_LEVEL, NintendoGBS::OSC_COUNT),
        NUM_INPUTS
    };
    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_OSCILLATOR, NintendoGBS::OSC_COUNT),
        NUM_OUTPUTS
    };
    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(LIGHTS_LEVEL, NintendoGBS::OSC_COUNT),
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

    /// @brief Initialize a new GBS Chip module.
    ChipGBS() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(PARAM_FREQ + 0, -30.f, 30.f, 0.f, "Pulse 1 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -30.f, 30.f, 0.f, "Pulse 2 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2, -30.f, 30.f, 0.f, "Wave Frequency",    " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_NOISE_PERIOD, 0, 7, 0, "Noise Period", "", 0, 1, -7);
        configParam(PARAM_PW + 0, 0, 3, 2, "Pulse 1 Duty Cycle");
        configParam(PARAM_PW + 1, 0, 3, 2, "Pulse 2 Duty Cycle");
        configParam(PARAM_WAVETABLE, 0, NUM_WAVEFORMS, 0, "Waveform morph");
        configParam(PARAM_LFSR, 0, 1, 0, "Linear Feedback Shift Register");
        configParam(PARAM_LEVEL + 0, 0.f, 1.f, 1.0f, "Pulse 1 Volume", "%", 0, 100);
        configParam(PARAM_LEVEL + 1, 0.f, 1.f, 1.0f, "Pulse 2 Volume", "%", 0, 100);
        configParam(PARAM_LEVEL + 2, 0.f, 1.f, 1.0f, "Wave Volume", "%", 0, 100);
        configParam(PARAM_LEVEL + 3, 0.f, 1.f, 1.0f, "Noise Volume", "%", 0, 100);
        cvDivider.setDivision(16);
        lightDivider.setDivision(128);
        // set the output buffer for each individual voice
        for (unsigned channel = 0; channel < POLYPHONY_CHANNELS; channel++) {
            for (unsigned oscillator = 0; oscillator < NintendoGBS::OSC_COUNT; oscillator++)
                apu[channel].set_output(oscillator, &buffers[channel][oscillator]);
            // volume of 3 produces a roughly 5Vpp signal from all voices
            apu[channel].set_volume(3.f);
        }
        // update the sample rate on the engine
        onSampleRateChange();
        // reset the wave-tables
        onReset();
    }

    /// @brief Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override {
        // update the buffer for each oscillator
        for (unsigned channel = 0; channel < POLYPHONY_CHANNELS; channel++) {
            for (unsigned oscillator = 0; oscillator < NintendoGBS::OSC_COUNT; oscillator++) {
                buffers[channel][oscillator].set_sample_rate(APP->engine->getSampleRate(), CLOCK_RATE);
            }
        }
    }

    /// @brief Respond to the user resetting the module with the "Initialize" action.
    void onReset() override {
        /// the default wave-table for each page of the wave-table editor
        static constexpr uint8_t* wavetables[NUM_WAVEFORMS] = {
            SINE,
            PW5,
            RAMP_UP,
            TRIANGLE_DIST,
            RAMP_DOWN
        };
        for (unsigned i = 0; i < NUM_WAVEFORMS; i++)
            memcpy(wavetable[i], wavetables[i], SAMPLES_PER_WAVETABLE);
    }

    /// @brief Respond to the user randomizing the module with the "Randomize" action.
    void onRandomize() override {
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
    ///
    /// @returns a new JSON object with this module's state stored into it
    ///
    json_t* dataToJson() override {
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
    ///
    /// @param rootJ a JSON object with state data to load into this module
    ///
    void dataFromJson(json_t* rootJ) override {
        for (int table = 0; table < NUM_WAVEFORMS; table++) {
            auto key = "wavetable" + std::to_string(table);
            json_t* data = json_object_get(rootJ, key.c_str());
            if (data) {
                for (int sample = 0; sample < SAMPLES_PER_WAVETABLE; sample++)
                    wavetable[table][sample] = json_integer_value(json_array_get(data, sample));
            }
        }
    }

    /// @brief Get the frequency for the given oscillator
    ///
    /// @param oscillator the oscillator to return the frequency for
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 11 bit frequency value from the panel
    ///
    inline uint16_t getFrequency(unsigned oscillator, unsigned channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ_MIN = 8;
        // the maximal value for the frequency register
        static constexpr float FREQ_MAX = 2035;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + oscillator].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + oscillator].getPolyVoltage(channel);
        pitch += inputs[INPUT_FM + oscillator].getPolyVoltage(channel) / 5.f;
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        // TODO: why is the wave-table clocked at half rate? this is not
        // documented anywhere that I can find; however, it makes sense that
        // the wave oscillator would be an octave lower since the original
        // triangle oscillator was intended for bass
        if (oscillator == NintendoGBS::WAVETABLE) freq *= 2;
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to an 11-bit value
        freq = 2048.f - (static_cast<uint32_t>(buffers[oscillator][channel].get_clock_rate() / freq) >> 5);
        return rack::clamp(freq, FREQ_MIN, FREQ_MAX);
    }

    /// @brief Get the PW for the given oscillator
    ///
    /// @param oscillator the oscillator to return the pulse width for
    /// @param channel the polyphonic channel to return the pulse width for
    /// @returns the pulse width value coded in an 8-bit container
    ///
    inline uint8_t getPulseWidth(unsigned oscillator, unsigned channel) {
        // the minimal value for the pulse width register
        static constexpr float PW_MIN = 0;
        // the maximal value for the pulse width register
        static constexpr float PW_MAX = 3;
        // get the pulse width from the parameter knob
        auto pwParam = params[PARAM_PW + oscillator].getValue();
        // get the control voltage to the pulse width with 1V/step
        auto pwCV = inputs[INPUT_PW + oscillator].getPolyVoltage(channel) / 3.f;
        // get the 8-bit pulse width clamped within legal limits
        uint8_t pw = rack::clamp(pwParam + pwCV, PW_MIN, PW_MAX);
        return pw << 6;
    }

    /// @brief Return the wave-table parameter.
    ///
    /// @param channel the polyphonic channel to return the wave-table for
    /// @returns the floating index of the wave-table table in [0, 4]
    ///
    inline float getWavetablePosition(unsigned channel) {
        auto param = params[PARAM_WAVETABLE].getValue();
        // auto att = params[PARAM_WAVETABLE_ATT].getValue();
        // get the CV as 1V per wave-table
        auto cv = inputs[INPUT_WAVETABLE].getPolyVoltage(channel) / 2.f;
        // wave-tables are indexed maths style on panel, subtract 1 for CS style
        return rack::math::clamp(param + /*att * */ cv, 1.f, 5.f) - 1;
    }

    /// @brief Return the period of the noise oscillator from the panel controls.
    ///
    /// @param channel the polyphonic channel to return the period for
    /// @returns the period of the noise waveform generator
    ///
    inline uint8_t getNoisePeriod(unsigned channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ_MIN = 0;
        // the maximal value for the frequency register
        static constexpr float FREQ_MAX = 7;
        // get the attenuation from the parameter knob
        float freq = params[PARAM_NOISE_PERIOD].getValue();
        // apply the control voltage to the attenuation
        if (inputs[INPUT_NOISE_PERIOD].isConnected())
            freq += inputs[INPUT_NOISE_PERIOD].getPolyVoltage(channel) / 2.f;
        return FREQ_MAX - rack::clamp(floorf(freq), FREQ_MIN, FREQ_MAX);
    }

    /// @brief Return the volume parameter for the given oscillator.
    ///
    /// @param oscillator the oscillator to get the volume parameter for
    /// @param channel the polyphonic channel to return the volume for
    /// @param max_ the maximal value for the volume width register
    /// @returns the volume parameter for the given oscillator. This includes
    /// the value of the knob and any CV modulation.
    ///
    inline uint8_t getVolume(unsigned oscillator, unsigned channel, uint8_t max_) {
        // get the volume from the parameter knob
        auto param = params[PARAM_LEVEL + oscillator].getValue();
        // apply the control voltage to the attenuation
        if (inputs[INPUT_LEVEL + oscillator].isConnected()) {
            auto cv = inputs[INPUT_LEVEL + oscillator].getPolyVoltage(channel) / 10.f;
            cv = rack::clamp(cv, 0.f, 1.f);
            cv = roundf(100.f * cv) / 100.f;
            param *= 2 * cv;
        }
        // get the 8-bit volume clamped within legal limits
        uint8_t volume = rack::clamp(max_ * param, 0.f, static_cast<float>(max_));
        // wave volume is 2-bit:
        // 00 - 0%
        // 01 - 100%
        // 10 - 50%
        // 11 - 25%
        // invert to go in sequence from parameter as [0, 25, 50, 100]%
        // the 2 bits occupy the high bits 6 & 7, shift left 5 to place
        if (oscillator == NintendoGBS::WAVETABLE)
            return volume == 3 ? 1 << 5 : (static_cast<uint8_t>(4 - volume) << 5);
        // the volume level occupies the high 4 bits, shift left 4 to place
        return volume << 4;
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processCV(unsigned channel) {
        lfsr[channel].process(rescale(inputs[INPUT_LFSR].getPolyVoltage(channel), 0.f, 2.f, 0.f, 1.f));
        // turn on the power
        apu[channel].write(NintendoGBS::POWER_CONTROL_STATUS, 0b10000000);
        // set the global volume
        apu[channel].write(NintendoGBS::STEREO_ENABLES, 0b11111111);
        apu[channel].write(NintendoGBS::STEREO_VOLUME, 0b11111111);
        // ---------------------------------------------------------------
        // pulse
        // ---------------------------------------------------------------
        for (unsigned oscillator = 0; oscillator < 2; oscillator++) {
            // pulse width of the pulse wave (high 2 bits)
            apu[channel].write(NintendoGBS::PULSE0_DUTY_LENGTH_LOAD + NintendoGBS::REGS_PER_VOICE * oscillator, getPulseWidth(oscillator, channel));
            // volume of the pulse wave, envelope add mode on
            apu[channel].write(NintendoGBS::PULSE0_START_VOLUME + NintendoGBS::REGS_PER_VOICE * oscillator, getVolume(oscillator, channel, 15));
            // frequency
            auto freq = getFrequency(oscillator, channel);
            apu[channel].write(NintendoGBS::PULSE0_FREQ_LO               + NintendoGBS::REGS_PER_VOICE * oscillator,
                         freq & 0b0000000011111111
            );
            apu[channel].write(NintendoGBS::PULSE0_TRIG_LENGTH_ENABLE_HI + NintendoGBS::REGS_PER_VOICE * oscillator,
                0x80 | ((freq & 0b0000011100000000) >> 8)
            );
        }
        // ---------------------------------------------------------------
        // wave
        // ---------------------------------------------------------------
        apu[channel].write(NintendoGBS::WAVE_DAC_POWER, 0b10000000);
        apu[channel].write(NintendoGBS::WAVE_VOLUME_CODE, getVolume(NintendoGBS::WAVETABLE, channel, 3));
        // frequency
        auto freq = getFrequency(2, channel);
        apu[channel].write(NintendoGBS::WAVE_FREQ_LO,
                     freq & 0b0000000011111111
        );
        apu[channel].write(NintendoGBS::WAVE_TRIG_LENGTH_ENABLE_FREQ_HI,
            0x80 | ((freq & 0b0000011100000000) >> 8)
        );
        // get the index of the wave-table from the panel
        auto position = getWavetablePosition(channel);
        // calculate the address of the base waveform in the table
        int wavetable0 = floor(position);
        // calculate the address of the next waveform in the table
        int wavetable1 = ceil(position);
        // calculate floating point offset between the base and next table
        float interpolate = position - wavetable0;
        // iterate over samples. APU samples are packed with two samples
        // per byte, but samples at this layer are not packed for
        // simplicity. As such, iterate over APU samples and consider
        // samples at this layer two at a time. Use shift instead of
        // multiplication / division for better performance
        for (int i = 0; i < (SAMPLES_PER_WAVETABLE >> 1); i++) {
            // shift the APU sample over 1 to iterate over internal samples
            // in pairs (e.g., two at a time)
            auto sample = i << 1;
            // get the first waveform data
            auto nibbleHi0 = wavetable[wavetable0][sample];
            auto nibbleLo0 = wavetable[wavetable0][sample + 1];
            // get the second waveform data
            auto nibbleHi1 = wavetable[wavetable1][sample];
            auto nibbleLo1 = wavetable[wavetable1][sample + 1];
            // floating point interpolation between both samples
            uint8_t nibbleHi = ((1.f - interpolate) * nibbleHi0 + interpolate * nibbleHi1);
            uint8_t nibbleLo = ((1.f - interpolate) * nibbleLo0 + interpolate * nibbleLo1);
            // combine the two samples into a single byte for the RAM
            apu[channel].write(NintendoGBS::WAVE_TABLE_VALUES + i, (nibbleHi << 4) | nibbleLo);
        }
        // ---------------------------------------------------------------
        // noise
        // ---------------------------------------------------------------
        // set the period and LFSR
        bool is_lfsr = (1 - params[PARAM_LFSR].getValue()) - !lfsr[channel].state;
        auto noise_clock_shift = is_lfsr * 0b00001000 | getNoisePeriod(channel);
        if (apu[channel].read(NintendoGBS::NOISE_CLOCK_SHIFT) != noise_clock_shift) {
            apu[channel].write(NintendoGBS::NOISE_CLOCK_SHIFT, noise_clock_shift);
            apu[channel].write(NintendoGBS::NOISE_TRIG_LENGTH_ENABLE, 0x80);
        }
        // set the volume for the oscillator
        auto noiseVolume = getVolume(NintendoGBS::NOISE, channel, 15);
        if (apu[channel].read(NintendoGBS::NOISE_START_VOLUME) != noiseVolume) {
            apu[channel].write(NintendoGBS::NOISE_START_VOLUME, noiseVolume);
            // trigger the oscillator when the volume changes
            apu[channel].write(NintendoGBS::NOISE_TRIG_LENGTH_ENABLE, 0x80);
        } else if (apu[channel].read(NintendoGBS::NOISE_TRIG_LENGTH_ENABLE) != 0x80) {
            // enable the oscillator. setting trigger resets the phase of the
            // noise, so check if it's set first
            apu[channel].write(NintendoGBS::NOISE_TRIG_LENGTH_ENABLE, 0x80);
        }
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
        // process the CV inputs to the chip
        if (cvDivider.process()) {
            for (unsigned channel = 0; channel < channels; channel++) {
                processCV(channel);
            }
        }
        // set output polyphony channels
        for (unsigned oscillator = 0; oscillator < NintendoGBS::OSC_COUNT; oscillator++)
            outputs[OUTPUT_OSCILLATOR + oscillator].setChannels(channels);
        // process audio samples on the chip engine. keep a sum of the output
        // of each channel for the VU meters
        float sum[NintendoGBS::OSC_COUNT] = {0, 0, 0, 0};
        for (unsigned channel = 0; channel < channels; channel++) {
            apu[channel].end_frame(CLOCK_RATE / args.sampleRate);
            for (unsigned oscillator = 0; oscillator < NintendoGBS::OSC_COUNT; oscillator++) {
                const auto sample = buffers[channel][oscillator].read_sample_10V();
                sum[oscillator] += sample;
                outputs[OUTPUT_OSCILLATOR + oscillator].setVoltage(sample, channel);
            }
        }
        // process the VU meter for each oscillator based on the summed outputs
        for (unsigned oscillator = 0; oscillator < NintendoGBS::OSC_COUNT; oscillator++)
            chMeters[oscillator].process(args.sampleTime, sum[oscillator] / 5.f);
        // update the VU meter lights
        if (lightDivider.process()) {
            for (unsigned oscillator = 0; oscillator < NintendoGBS::OSC_COUNT; oscillator++) {
                float brightness = chMeters[oscillator].getBrightness(-24.f, 0.f);
                lights[LIGHTS_LEVEL + oscillator].setBrightness(brightness);
            }
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for GBS.
struct ChipGBSWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit ChipGBSWidget(ChipGBS *module) {
        setModule(module);
        static constexpr auto panel = "res/GBS.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // the fill colors for the wave-table editor lines
        static constexpr NVGcolor colors[ChipGBS::NUM_WAVEFORMS] = {
            {{{1.f, 0.f, 0.f, 1.f}}},  // red
            {{{0.f, 1.f, 0.f, 1.f}}},  // green
            {{{0.f, 0.f, 1.f, 1.f}}},  // blue
            {{{1.f, 1.f, 0.f, 1.f}}},  // yellow
            {{{1.f, 1.f, 1.f, 1.f}}}   // white
        };
        /// the default wave-table for each page of the wave-table editor
        static constexpr uint8_t* wavetables[ChipGBS::NUM_WAVEFORMS] = {
            SINE,
            PW5,
            RAMP_UP,
            TRIANGLE_DIST,
            RAMP_DOWN
        };
        // add wave-table editors
        for (int wave = 0; wave < ChipGBS::NUM_WAVEFORMS; wave++) {
            // get the wave-table buffer for this editor. if the module is
            // displaying in/being rendered for the library, the module will
            // be null and a dummy waveform is displayed
            uint8_t* wavetable = module ?
                &reinterpret_cast<ChipGBS*>(this->module)->wavetable[wave][0] :
                &wavetables[wave][0];
            // setup a table editor for the buffer
            auto table_editor = new WaveTableEditor<uint8_t>(
                wavetable,                       // wave-table buffer
                ChipGBS::SAMPLES_PER_WAVETABLE,  // wave-table length
                ChipGBS::BIT_DEPTH,              // waveform bit depth
                Vec(18, 26 + 67 * wave),         // position
                Vec(135, 60),                    // size
                colors[wave]                     // line fill color
            );
            // add the table editor to the module
            addChild(table_editor);
        }
        // oscillator components
        for (unsigned i = 0; i < NintendoGBS::OSC_COUNT; i++) {
            if (i < NintendoGBS::NOISE) {
                addInput(createInput<PJ301MPort>(             Vec(169, 75 + 85 * i), module, ChipGBS::INPUT_VOCT     + i));
                addInput(createInput<PJ301MPort>(             Vec(169, 26 + 85 * i), module, ChipGBS::INPUT_FM       + i));
                addParam(createParam<BefacoBigKnob>(          Vec(202, 25 + 85 * i), module, ChipGBS::PARAM_FREQ     + i));
                auto param = createParam<RoundSmallBlackKnob>(Vec(289, 38 + 85 * i), module, ChipGBS::PARAM_PW       + i);
                if (i < NintendoGBS::WAVETABLE) param->snap = true;
                addParam(param);
                addInput(createInput<PJ301MPort>(             Vec(288, 73 + 85 * i), module, ChipGBS::INPUT_PW       + i));
            }
            addParam(createLightParam<LEDLightSlider<GreenLight>>(Vec(316, 24 + 85 * i),  module, ChipGBS::PARAM_LEVEL + i, ChipGBS::LIGHTS_LEVEL + i));
            addInput(createInput<PJ301MPort>(                 Vec(346, 26 + 85 * i), module, ChipGBS::INPUT_LEVEL + i));
            addOutput(createOutput<PJ301MPort>(               Vec(346, 74 + 85 * i), module, ChipGBS::OUTPUT_OSCILLATOR + i));
        }
        // noise period
        auto param = createParam<Rogan3PWhite>(Vec(202, 298), module, ChipGBS::PARAM_NOISE_PERIOD);
        param->snap = true;
        addParam(param);
        addInput(createInput<PJ301MPort>(Vec(169, 329), module, ChipGBS::INPUT_NOISE_PERIOD));
        // LFSR switch
        addParam(createParam<CKSS>(Vec(280, 281), module, ChipGBS::PARAM_LFSR));
        addInput(createInput<PJ301MPort>(Vec(258, 329), module, ChipGBS::INPUT_LFSR));
    }
};

/// the global instance of the model
Model *modelChipGBS = createModel<ChipGBS, ChipGBSWidget>("GBS");
