// A Nintendo GBS Chip module.
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
#include "dsp/math.hpp"
#include "dsp/trigger.hpp"
#include "dsp/nintendo_gameboy.hpp"
#include "dsp/wavetable4bit.hpp"
#include "engine/chip_module.hpp"
#include "widget/wavetable_editor.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Nintendo GameBoy Sound System chip emulator module.
struct PalletTownWavesSystem : ChipModule<NintendoGBS> {
 private:
    /// a Trigger for handling inputs to the LFSR port
    Trigger::Threshold lfsr[PORT_MAX_CHANNELS];

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ, 3),
        PARAM_NOISE_PERIOD,
        ENUMS(PARAM_FM, 3),
        PARAM_LFSR,
        ENUMS(PARAM_PW, 2),
        PARAM_WAVETABLE,
        ENUMS(PARAM_LEVEL, NintendoGBS::OSC_COUNT),
        NUM_PARAMS
    };
    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT, 3),
        INPUT_NOISE_PERIOD,
        ENUMS(INPUT_FM, 3),
        INPUT_LFSR,
        ENUMS(INPUT_PW, 2),
        INPUT_WAVETABLE,
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
        ENUMS(LIGHTS_LEVEL, 3 * NintendoGBS::OSC_COUNT),
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
    PalletTownWavesSystem() : ChipModule<NintendoGBS>() {
        normal_outputs = true;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(PARAM_FREQ + 0, -2.5f, 2.5f, 0.f, "Pulse 1 Frequency", " Hz", 2, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -2.5f, 2.5f, 0.f, "Pulse 2 Frequency", " Hz", 2, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2, -2.5f, 2.5f, 0.f, "Wave Frequency",    " Hz", 2, dsp::FREQ_C4);
        configParam(PARAM_FM + 0, -1.f, 1.f, 0.f, "Pulse 1 FM");
        configParam(PARAM_FM + 1, -1.f, 1.f, 0.f, "Pulse 2 FM");
        configParam(PARAM_FM + 2, -1.f, 1.f, 0.f, "Wave FM");
        configParam(PARAM_NOISE_PERIOD, 0, 7, 0, "Noise Period");
        configParam(PARAM_PW + 0, 0, 3, 2, "Pulse 1 Duty Cycle");
        configParam(PARAM_PW + 1, 0, 3, 2, "Pulse 2 Duty Cycle");
        configParam(PARAM_WAVETABLE, 0, NUM_WAVEFORMS, 0, "Waveform morph");
        configParam<BooleanParamQuantity>(PARAM_LFSR, 0, 1, 0, "Linear Feedback Shift Register");
        configParam(PARAM_LEVEL + 0, 0, 15, 10, "Pulse 1 Volume");
        configParam(PARAM_LEVEL + 1, 0, 15, 10, "Pulse 2 Volume");
        configParam(PARAM_LEVEL + 2, 0, 3,  3,  "Wave Volume");
        configParam(PARAM_LEVEL + 3, 0, 15, 10, "Noise Volume");
        resetWavetable();
    }

    /// Reset the waveform table to the default state.
    void resetWavetable() {
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

    /// @brief Respond to the module being reset by the host environment.
    void onReset() final {
        ChipModule<NintendoGBS>::onReset();
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
    ///
    /// @returns a new JSON object with this module's state stored into it
    ///
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
    ///
    /// @param rootJ a JSON object with state data to load into this module
    ///
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
    /// @brief Get the frequency for the given oscillator
    ///
    /// @param oscillator the oscillator to return the frequency for
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 11 bit frequency value from the panel
    ///
    inline uint16_t getFrequency(unsigned oscillator, unsigned channel) {
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
        // convert the frequency to an 11-bit value
        freq = 2048.f - (static_cast<uint32_t>(buffers[oscillator][channel].get_clock_rate() / freq) >> 5);
        return Math::clip(freq, 8.f, 2035.f);
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
        auto param = params[PARAM_PW + oscillator].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 5V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normalMod = oscillator ? inputs[INPUT_PW + oscillator - 1].getVoltage(channel) : 0.f;
        const auto mod = inputs[INPUT_PW + oscillator].getNormalVoltage(normalMod, channel);
        inputs[INPUT_PW + oscillator].setVoltage(mod, channel);
        // get the 8-bit pulse width clamped within legal limits
        uint8_t pw = Math::clip(param + rescale(mod, 0.f, 7.f, 0, 4), PW_MIN, PW_MAX);
        // shift the pulse width over into the high 2 bits
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
        auto normalMod = inputs[INPUT_WAVETABLE - 1].getVoltage(channel);
        // rescale from a 7V range to the parameter space in [-5, 5]
        auto cv = rescale(inputs[INPUT_WAVETABLE].getNormalVoltage(normalMod, channel), -7.f, 7.f, -5.f, 5.f);
        // wave-tables are indexed maths style on panel, subtract 1 for CS style
        return Math::clip(param + /*att * */ cv, 1.f, 5.f) - 1;
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
            freq += inputs[INPUT_NOISE_PERIOD].getVoltage(channel) / 2.f;
        return FREQ_MAX - Math::clip(floorf(freq), FREQ_MIN, FREQ_MAX);
    }

    /// @brief Return the volume parameter for the given oscillator.
    ///
    /// @param oscillator the oscillator to get the volume parameter for
    /// @param channel the polyphonic channel to return the volume for
    /// @param max_ the maximal value for the volume width register
    /// @returns the volume parameter for the given oscillator. This includes
    /// the value of the knob and any CV modulation.
    ///
    inline uint8_t getVolume(unsigned oscillator, unsigned channel, uint8_t max) {
        // get the level from the parameter knob
        auto level = params[PARAM_LEVEL + oscillator].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 10V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normal = oscillator ? inputs[INPUT_LEVEL + oscillator - 1].getVoltage(channel) : 10.f;
        const auto voltage = inputs[INPUT_LEVEL + oscillator].getNormalVoltage(normal, channel);
        inputs[INPUT_LEVEL + oscillator].setVoltage(voltage, channel);
        // apply the control voltage to the level. Normal to a constant
        // 10V source instead of checking if the cable is connected
        uint8_t volume = Math::clip(roundf(level * Math::Eurorack::fromDC(voltage)), 0.f, static_cast<float>(max));
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

    /// @brief Process the audio rate inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the audio inputs to
    ///
    inline void processAudio(const ProcessArgs& args, const unsigned& channel) final {
        for (unsigned oscillator = 0; oscillator < 2; oscillator++) {
            auto freq = getFrequency(oscillator, channel);
            apu[channel].write(NintendoGBS::PULSE0_FREQ_LO               + NintendoGBS::REGS_PER_VOICE * oscillator,
                         freq & 0b0000000011111111
            );
            apu[channel].write(NintendoGBS::PULSE0_TRIG_LENGTH_ENABLE_HI + NintendoGBS::REGS_PER_VOICE * oscillator,
                0x80 | ((freq & 0b0000011100000000) >> 8)
            );
        }
        auto freq = getFrequency(2, channel);
        apu[channel].write(NintendoGBS::WAVE_FREQ_LO,
                     freq & 0b0000000011111111
        );
        apu[channel].write(NintendoGBS::WAVE_TRIG_LENGTH_ENABLE_FREQ_HI,
            0x80 | ((freq & 0b0000011100000000) >> 8)
        );
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processCV(const ProcessArgs& args, const unsigned& channel) final {
        lfsr[channel].process(rescale(inputs[INPUT_LFSR].getVoltage(channel), 0.01f, 2.f, 0.f, 1.f));
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
        }
        // ---------------------------------------------------------------
        // wave
        // ---------------------------------------------------------------
        apu[channel].write(NintendoGBS::WAVE_DAC_POWER, 0b10000000);
        apu[channel].write(NintendoGBS::WAVE_VOLUME_CODE, getVolume(NintendoGBS::WAVETABLE, channel, 3));
        // ---------------------------------------------------------------
        // noise
        // ---------------------------------------------------------------
        // set the period and LFSR
        bool is_lfsr = (1 - params[PARAM_LFSR].getValue()) - !lfsr[channel].isHigh();
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
        // WAVE-TABLE
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
    }

    /// @brief Process the lights on the module.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channels the number of active polyphonic channels
    ///
    inline void processLights(const ProcessArgs& args, const unsigned& channels) final {
        for (unsigned voice = 0; voice < NintendoGBS::OSC_COUNT; voice++) {
            // get the global brightness scale from -12 to 3
            auto brightness = vuMeter[voice].getBrightness(-12, 3);
            // set the red light based on total brightness and
            // brightness from 0dB to 3dB
            lights[LIGHTS_LEVEL + voice * 3 + 0].setBrightness(brightness * vuMeter[voice].getBrightness(0, 3));
            // set the red light based on inverted total brightness and
            // brightness from -12dB to 0dB
            lights[LIGHTS_LEVEL + voice * 3 + 1].setBrightness((1 - brightness) * vuMeter[voice].getBrightness(-12, 0));
            // set the blue light to off
            lights[LIGHTS_LEVEL + voice * 3 + 2].setBrightness(0);
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for GBS.
struct PalletTownWavesSystemWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit PalletTownWavesSystemWidget(PalletTownWavesSystem *module) {
        setModule(module);
        static constexpr auto panel = "res/PalletTownWavesSystem.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // the fill colors for the wave-table editor lines
        static constexpr NVGcolor colors[PalletTownWavesSystem::NUM_WAVEFORMS] = {
            {{{1.f, 0.f, 0.f, 1.f}}},  // red
            {{{0.f, 1.f, 0.f, 1.f}}},  // green
            {{{0.f, 0.f, 1.f, 1.f}}},  // blue
            {{{1.f, 1.f, 0.f, 1.f}}},  // yellow
            {{{1.f, 1.f, 1.f, 1.f}}}   // white
        };
        /// the default wave-table for each page of the wave-table editor
        static constexpr uint8_t* wavetables[PalletTownWavesSystem::NUM_WAVEFORMS] = {
            SINE,
            PW5,
            RAMP_UP,
            TRIANGLE_DIST,
            RAMP_DOWN
        };
        // add wave-table editors
        for (int wave = 0; wave < PalletTownWavesSystem::NUM_WAVEFORMS; wave++) {
            // get the wave-table buffer for this editor. if the module is
            // displaying in/being rendered for the library, the module will
            // be null and a dummy waveform is displayed
            uint8_t* wavetable = module ? &module->wavetable[wave][0] : &wavetables[wave][0];
            // setup a table editor for the buffer
            auto table_editor = new WaveTableEditor<uint8_t>(
                wavetable,                       // wave-table buffer
                PalletTownWavesSystem::SAMPLES_PER_WAVETABLE,  // wave-table length
                PalletTownWavesSystem::BIT_DEPTH,              // waveform bit depth
                Vec(11, 26 + 67 * wave),         // position
                Vec(136, 60),                    // size
                colors[wave]                     // line fill color
            );
            // add the table editor to the module
            addChild(table_editor);
        }
        for (unsigned i = 0; i < NintendoGBS::OSC_COUNT; i++) {
            // Frequency / Noise Period
            auto freq = createParam<Trimpot>(  Vec(162 + 35 * i, 32),  module, PalletTownWavesSystem::PARAM_FREQ        + i);
            freq->snap = i == 3;
            addParam(freq);
            addInput(createInput<PJ301MPort>(  Vec(160 + 35 * i, 71),  module, PalletTownWavesSystem::INPUT_VOCT        + i));
            // FM / LFSR
            addInput(createInput<PJ301MPort>(  Vec(160 + 35 * i, 99), module, PalletTownWavesSystem::INPUT_FM          + i));
            if (i < 3)
                addParam(createParam<Trimpot>( Vec(162 + 35 * i, 144), module, PalletTownWavesSystem::PARAM_FM          + i));
            else
                addParam(createParam<CKSS>(    Vec(269, 141), module, PalletTownWavesSystem::PARAM_FM                  + i));
            // Level
            addParam(createSnapParam<Trimpot>( Vec(162 + 35 * i, 170), module, PalletTownWavesSystem::PARAM_LEVEL       + i));
            addInput(createInput<PJ301MPort>(  Vec(160 + 35 * i, 210), module, PalletTownWavesSystem::INPUT_LEVEL       + i));
            // PW
            if (i < 3) {  // Pulse Width / Waveform
                auto pw = createParam<Trimpot>(Vec(162 + 35 * i, 241), module, PalletTownWavesSystem::PARAM_PW + i);
                pw->snap = i < 2;
                addParam(pw);
                addInput(createInput<PJ301MPort>(Vec(160 + 35 * i, 281), module, PalletTownWavesSystem::INPUT_PW + i));
            } else {  // LFSR Reset
                // addInput(createInput<PJ301MPort>(Vec(160 + 35 * i, 264), module, PalletTownWavesSystem::INPUT_PW + i));
            }
            // Output
            addChild(createLight<SmallLight<RedGreenBlueLight>>(Vec(179 + 35 * i, 326), module, PalletTownWavesSystem::LIGHTS_LEVEL + 3 * i));
            addOutput(createOutput<PJ301MPort>(Vec(160 + 35 * i, 331), module, PalletTownWavesSystem::OUTPUT_OSCILLATOR + i));
        }
    }
};

/// the global instance of the model
Model *modelPalletTownWavesSystem = createModel<PalletTownWavesSystem, PalletTownWavesSystemWidget>("GBS");
