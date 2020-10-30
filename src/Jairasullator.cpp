// A General Instrument AY-3-8910 Chip module.
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
#include "dsp/general_instrument_ay_3_8910.hpp"

// TODO: document both modes off (4-bit dac based on amp port!)
// TODO: -   freq knob to input gain for DAC mode
// TODO: -   mod knob to DC offset for DAC mode
// TODO: End of Rise / End of Fall output

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A General Instrument AY-3-8910 chip emulator module.
struct Jairasullator : ChipModule<GeneralInstrumentAy_3_8910> {
 private:
    /// triggers for handling inputs to the tone and noise enable switches
    rack::dsp::BooleanTrigger mixerTriggers[2 * GeneralInstrumentAy_3_8910::OSC_COUNT];

    /// triggers for handling inputs to the sync ports and the envelope trig
    rack::dsp::SchmittTrigger syncTriggers[GeneralInstrumentAy_3_8910::OSC_COUNT + 1];

    /// the mode the envelope generator is in
    uint8_t envMode = 0;

    /// a trigger for handling presses to the change mode button
    rack::dsp::SchmittTrigger envModeTrigger;

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ,     GeneralInstrumentAy_3_8910::OSC_COUNT),
        PARAM_ENVELOPE_FREQ,
        ENUMS(PARAM_FM,       GeneralInstrumentAy_3_8910::OSC_COUNT),
        PARAM_ENVELOPE_FM,
        ENUMS(PARAM_LEVEL,    GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(PARAM_TONE,     GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(PARAM_NOISE,    GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(PARAM_ENVELOPE, GeneralInstrumentAy_3_8910::OSC_COUNT),
        PARAM_NOISE_PERIOD,
        PARAM_ENVELOPE_MODE,
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT,     GeneralInstrumentAy_3_8910::OSC_COUNT),
        INPUT_ENVELOPE_VOCT,
        ENUMS(INPUT_FM,       GeneralInstrumentAy_3_8910::OSC_COUNT),
        INPUT_ENVELOPE_FM,
        ENUMS(INPUT_LEVEL,    GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_TONE,     GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_NOISE,    GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_ENVELOPE, GeneralInstrumentAy_3_8910::OSC_COUNT),
        INPUT_NOISE_PERIOD,
        INPUT_ENVELOPE_MODE,
        ENUMS(INPUT_RESET, GeneralInstrumentAy_3_8910::OSC_COUNT),
        INPUT_ENVELOPE_RESET,
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_OSCILLATOR, GeneralInstrumentAy_3_8910::OSC_COUNT),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(LIGHTS_LEVEL, 3 * GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(LIGHTS_ENV_MODE, 3),
        NUM_LIGHTS
    };

    /// @brief Initialize a new Jairasullator module.
    Jairasullator() : ChipModule<GeneralInstrumentAy_3_8910>(2.5) {
        normal_outputs = true;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned oscillator = 0; oscillator < GeneralInstrumentAy_3_8910::OSC_COUNT; oscillator++) {
            // get the channel name starting with ACII code 65 (A)
            auto name = "Pulse " + std::string(1, static_cast<char>(65 + oscillator));
            configParam(PARAM_FREQ  + oscillator, -5.f,   5.f,  0.f, name + " Frequency", " Hz", 2, dsp::FREQ_C4);
            configParam(PARAM_FM    + oscillator, -1.f,   1.f,  0.f, name + " FM");
            configParam(PARAM_LEVEL + oscillator,  0,    15,   10,   name + " Level");
            configParam(PARAM_TONE  + oscillator,  0,     1,    1,   name + " Tone Enabled");
            configParam(PARAM_NOISE + oscillator,  0,     1,    0,   name + " Noise Enabled");
            configParam(PARAM_ENVELOPE + oscillator,  0,     1,    0,   name + " Envelope Enabled");
        }
        configParam(PARAM_NOISE_PERIOD, 0, 31, 0, "Noise Period");
        configParam(PARAM_ENVELOPE_FREQ, -5.5, 9, 1.75, "Envelope Frequency", " Hz", 2);
        configParam(PARAM_ENVELOPE_FM, -1, 1, 0, "Envelope FM");
        configParam(PARAM_ENVELOPE_MODE, 0, 1, 0, "Envelope Mode");
        // TODO: change amplifier level to accept audio rate. maybe condition
        // on when dac mode is active (i.e., neither tone nor noise are active).
        // this can then be removed
        cvDivider.setDivision(1);
    }

    /// @brief Respond to the module being reset by the engine.
    inline void onReset() override {
        ChipModule<GeneralInstrumentAy_3_8910>::onReset();
        envMode = 0;
    }

    /// @brief Return a JSON representation of this module's state
    ///
    /// @returns a new JSON object with this object's serialized state data
    ///
    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "envMode", json_integer(envMode));
        return rootJ;
    }

    /// @brief Return the object to the given serialized state.
    ///
    /// @returns a JSON object with object serialized state data to restore
    ///
    void dataFromJson(json_t* rootJ) override {
        json_t* envModeObject = json_object_get(rootJ, "envMode");
        if (envModeObject) envMode = json_integer_value(envModeObject);
    }

 protected:
    /// @brief Return the frequency for the given channel.
    ///
    /// @param oscillator the oscillator to return the frequency for
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 12-bit frequency in a 16-bit container
    ///
    inline uint16_t getFrequency(unsigned oscillator, unsigned channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ12BIT_MIN = 2;
        // the maximal value for the frequency register
        static constexpr float FREQ12BIT_MAX = 4095;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 2 * 16;
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
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = buffers[channel][oscillator].get_clock_rate() / (CLOCK_DIVISION * freq);
        return rack::clamp(freq, FREQ12BIT_MIN, FREQ12BIT_MAX);
    }

    /// @brief Return the level for the given channel.
    ///
    /// @param oscillator the oscillator to return the frequency for
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 4-bit level value in an 8-bit container
    ///
    inline uint8_t getLevel(unsigned oscillator, unsigned channel) {
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
        level = roundf(level * voltage / 10.f);
        // get the 8-bit attenuation by inverting the level and clipping
        // to the legal bounds of the parameter
        // // the maximal value for the volume width register
        static constexpr float MAX = 15;
        return rack::clamp(level, 0.f, MAX);
    }

    /// @brief Return whether the given oscillator has the envelope enabled.
    ///
    /// @param channel the polyphonic channel to return the envelope enabled
    /// parameter of
    /// @param oscillator the index of the oscillator to return the envelope
    /// enabled parameter of
    /// @returns true if the oscillator has the envelope generator enabled
    ///
    inline bool isEnvelopeOn(unsigned oscillator, unsigned channel) {
        return params[PARAM_ENVELOPE + oscillator].getValue();
    }

    /// @brief Return the noise period.
    ///
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the period for the noise oscillator
    ///
    inline uint8_t getNoisePeriod(unsigned channel) {
        // the maximal value for the frequency register
        static constexpr float MAX = 31;
        // get the attenuation from the parameter knob
        const float param = params[PARAM_NOISE_PERIOD].getValue();
        // apply the control voltage to the attenuation.
        const float cv = inputs[INPUT_NOISE_PERIOD].getNormalVoltage(0.f, channel);
        // scale such that [0,7]V controls the full range of the parameter
        const float mod = rescale(cv, 0.f, 7.f, 0.f, MAX);
        // invert the parameter so larger values have higher frequencies
        return MAX - rack::clamp(floorf(param + mod), 0.f, MAX);
    }

    /// @brief Return the envelope period.
    ///
    /// @param channel the polyphonic channel to return the envelope period for
    /// @returns the 16-bit envelope period from parameters and CV inputs
    ///
    inline uint16_t getEnvelopePeriod(unsigned channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ16BIT_MIN = 1;
        // the maximal value for the frequency register
        static constexpr float FREQ16BIT_MAX = 0xffff;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 2 * 256;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_ENVELOPE_FREQ].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 0V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normalPitch = inputs[INPUT_ENVELOPE_VOCT - 1].getVoltage(channel);
        const auto pitchCV = inputs[INPUT_ENVELOPE_VOCT].getNormalVoltage(normalPitch, channel);
        pitch += pitchCV;
        // get the attenuverter parameter value
        const auto att = params[PARAM_ENVELOPE_FM].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 5V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normalMod = inputs[INPUT_ENVELOPE_FM - 1].getVoltage(channel);
        const auto mod = inputs[INPUT_ENVELOPE_FM].getNormalVoltage(normalMod, channel);
        pitch += att * mod / 5.f;
        // convert the pitch to frequency based on standard exponential scale
        float freq = 1 * powf(2.0, pitch);
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = buffers[channel][0].get_clock_rate() / (CLOCK_DIVISION * freq);
        return rack::clamp(freq, FREQ16BIT_MIN, FREQ16BIT_MAX);
    }

    /// @brief Return the envelope mode.
    ///
    /// @param channel the polyphonic channel to return the envelope mode for
    /// @returns the 4-bit envelope mode from parameters and CV inputs
    ///
    inline uint8_t getEnvelopeMode(unsigned channel) {
        if (envModeTrigger.process(params[PARAM_ENVELOPE_MODE].getValue()))
            envMode = (envMode + 1) % 8;
        return 0x8 | envMode;
    }

    /// @brief Return the mixer byte.
    ///
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 6-bit mixer byte from parameters and CV inputs
    ///
    inline uint8_t getMixer(unsigned channel) {
        uint8_t mixerByte = 0;
        // iterate over the oscillators to set the mixer tone and noise flags.
        // there is a set of 3 flags for both tone and noise. start with
        // iterating over the tone inputs and parameters, but fall through to
        // getting the noise inputs and parameters, which immediately follow
        // those of the tone. I.e., INPUT_TONE = INPUT_NOISE and
        // PARAM_TONE = PARAM_NOISE when i > 2.
        for (unsigned i = 0; i < 2 * GeneralInstrumentAy_3_8910::OSC_COUNT; i++) {
            // clamp the input within [0, 10]. this allows bipolar signals to
            // be interpreted as unipolar signals for the trigger input
            auto cv = math::clamp(inputs[INPUT_TONE + i].getVoltage(channel), 0.f, 10.f);
            mixerTriggers[i].process(rescale(cv, 0.f, 2.f, 0.f, 1.f));
            // get the state of the tone based on the parameter and trig input
            bool toneState = params[PARAM_TONE + i].getValue() - mixerTriggers[i].state;
            // invert the state to indicate "OFF" semantics instead of "ON"
            mixerByte |= !toneState << i;
        }
        return mixerByte;
    }

    /// @brief Return the hard sync boolean for given index.
    ///
    /// @param index the index of the oscillator to get the hard sync flag of
    /// @param channel the polyphonic channel of the engine to use
    /// @returns true if the voice with given index is being hard synced by an
    /// external input on this frame
    /// @details
    /// Index 3 returns the value of the envelope generators sync input
    ///
    inline bool getReset(unsigned index, unsigned channel) {
        // get the normalled input from the last voice's port
        const auto normal = inputs[INPUT_RESET + index - 1].getVoltage(channel);
        // get the input to this port, defaulting to the normalled input
        const auto sync = inputs[INPUT_RESET + index].getNormalVoltage(normal, channel);
        // reset the voltage on this port for the next voice to normal to
        inputs[INPUT_RESET + index].setVoltage(sync, channel);
        // process the sync trigger and return the result
        return syncTriggers[index].process(rescale(sync, 0.f, 2.f, 0.f, 1.f));
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processCV(const ProcessArgs &args, unsigned channel) final {
        // oscillators (processed in order for port normalling)
        for (unsigned osc = 0; osc < GeneralInstrumentAy_3_8910::OSC_COUNT; osc++) {
            apu[channel].set_frequency(osc, getFrequency(osc, channel));
            apu[channel].set_voice_volume(osc, getLevel(osc, channel), isEnvelopeOn(osc, channel));
            if (getReset(osc, channel)) apu[channel].reset_phase(osc);
        }
        // envelope (processed after oscillators for port normalling)
        apu[channel].set_envelope_mode(getEnvelopeMode(channel));
        apu[channel].set_channel_enables(getMixer(channel));
        if (getReset(3, channel)) apu[channel].reset_envelope_phase();
        // noise
        apu[channel].set_noise_period(getNoisePeriod(channel));
        apu[channel].set_envelope_period(getEnvelopePeriod(channel));
    }

    /// @brief Process the lights on the module.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channels the number of active polyphonic channels
    ///
    inline void processLights(const ProcessArgs &args, unsigned channels) final {
        for (unsigned voice = 0; voice < GeneralInstrumentAy_3_8910::OSC_COUNT; voice++) {
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
            // set the envelope mode light in RGB order
            auto deltaTime = args.sampleTime * lightDivider.getDivision();
            bool red = envMode & 0x4;
            lights[LIGHTS_ENV_MODE + 0].setSmoothBrightness(red, deltaTime);
            bool green = envMode & 0x2;
            lights[LIGHTS_ENV_MODE + 1].setSmoothBrightness(green, deltaTime);
            bool blue = envMode & 0x1;
            lights[LIGHTS_ENV_MODE + 2].setSmoothBrightness(blue, deltaTime);
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// @brief The panel widget for Jairasullator.
struct JairasullatorWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit JairasullatorWidget(Jairasullator *module) {
        setModule(module);
        static constexpr auto panel = "res/Jairasullator.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        for (unsigned i = 0; i < GeneralInstrumentAy_3_8910::OSC_COUNT; i++) {
            // COLUMN 1
            // Frequency
            addParam(createParam<Trimpot>(     Vec(12 + 70 * i, 45),  module, Jairasullator::PARAM_FREQ  + i));
            addInput(createInput<PJ301MPort>(  Vec(10 + 70 * i, 85),  module, Jairasullator::INPUT_VOCT  + i));
            // FM
            addInput(createInput<PJ301MPort>(  Vec(10 + 70 * i, 129), module, Jairasullator::INPUT_FM    + i));
            addParam(createParam<Trimpot>(     Vec(12 + 70 * i, 173), module, Jairasullator::PARAM_FM    + i));
            // Level
            addParam(createSnapParam<Trimpot>( Vec(12 + 70 * i, 221), module, Jairasullator::PARAM_LEVEL + i));
            addInput(createInput<PJ301MPort>(  Vec(10 + 70 * i, 263), module, Jairasullator::INPUT_LEVEL + i));
            // Hard Sync
            addInput(createInput<PJ301MPort>(Vec(10 + 70 * i, 316), module, Jairasullator::INPUT_RESET + i));
            // COLUMN 2
            // Tone Enable
            addParam(createParam<CKSS>(        Vec(49 + 70 * i, 44), module, Jairasullator::PARAM_TONE     + i));
            addInput(createInput<PJ301MPort>(  Vec(45 + 70 * i, 86), module, Jairasullator::INPUT_TONE     + i));
            // Noise Enable
            addInput(createInput<PJ301MPort>(  Vec(45 + 70 * i, 130), module, Jairasullator::INPUT_NOISE    + i));
            addParam(createParam<CKSS>(        Vec(49 + 70 * i, 171), module, Jairasullator::PARAM_NOISE    + i));
            // Envelope Enables
            addParam(createParam<CKSS>(Vec(49 + 70 * i, 225), module, Jairasullator::PARAM_ENVELOPE + i));
            addInput(createInput<PJ301MPort>(  Vec(45 + 70 * i, 264), module, Jairasullator::INPUT_ENVELOPE    + i));
            // Output
            addChild(createLight<MediumLight<RedGreenBlueLight>>(Vec(52 + 70 * i, 297), module, Jairasullator::LIGHTS_LEVEL + 3 * i));
            addOutput(createOutput<PJ301MPort>(Vec(45 + 70 * i, 324), module, Jairasullator::OUTPUT_OSCILLATOR + i));
        }
        // Envelope / LFO Frequency
        addParam(createParam<Trimpot>(Vec(222, 47), module, Jairasullator::PARAM_ENVELOPE_FREQ));
        addInput(createInput<PJ301MPort>(Vec(220, 86), module, Jairasullator::INPUT_ENVELOPE_VOCT));
        // Envelope / LFO Frequency Mod (NOTE: DISABLED)
        // addInput(createInput<PJ301MPort>(Vec(220, 130), module, Jairasullator::INPUT_ENVELOPE_FM));
        // addParam(createParam<Trimpot>(Vec(222, 175), module, Jairasullator::PARAM_ENVELOPE_FM));
        // Noise Period
        addInput(createInput<PJ301MPort>(Vec(220, 130), module, Jairasullator::INPUT_NOISE_PERIOD));
        addParam(createSnapParam<Trimpot>(Vec(222, 175), module, Jairasullator::PARAM_NOISE_PERIOD));
        // Envelope Mode
        addParam(createParam<TL1105>(Vec(222, 228), module, Jairasullator::PARAM_ENVELOPE_MODE));
        addChild(createLight<MediumLight<RedGreenBlueLight>>(Vec(227, 272), module, Jairasullator::LIGHTS_ENV_MODE));
        // Envelope Reset / Hard Sync
        addInput(createInput<PJ301MPort>(Vec(220, 316), module, Jairasullator::INPUT_ENVELOPE_RESET));
    }
};

/// the global instance of the model
Model *modelJairasullator = createModel<Jairasullator, JairasullatorWidget>("AY_3_8910");
