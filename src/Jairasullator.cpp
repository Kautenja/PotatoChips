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
#include "dsp/math.hpp"
#include "dsp/trigger.hpp"
#include "dsp/general_instrument_ay_3_8910.hpp"
#include "engine/chip_module.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A General Instrument AY-3-8910 chip emulator module.
struct Jairasullator : ChipModule<GeneralInstrumentAy_3_8910> {
 private:
    /// triggers for handling inputs to the tone and noise enable switches
    Trigger::Threshold mixerTriggers[PORT_MAX_CHANNELS][2 * GeneralInstrumentAy_3_8910::OSC_COUNT];

    /// triggers for handling inputs to the envelope enable switches
    Trigger::Threshold envTriggers[PORT_MAX_CHANNELS][GeneralInstrumentAy_3_8910::OSC_COUNT];

    /// triggers for handling inputs to the sync ports and the envelope trig
    Trigger::Threshold syncTriggers[PORT_MAX_CHANNELS][GeneralInstrumentAy_3_8910::OSC_COUNT + 1];

    /// a trigger for handling presses to the change mode button
    Trigger::Threshold envModeTrigger;

 public:
    /// the mode the envelope generator is in
    uint8_t envMode = 0;

    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ,        GeneralInstrumentAy_3_8910::OSC_COUNT),
        PARAM_ENVELOPE_FREQ,
        ENUMS(PARAM_FM,          GeneralInstrumentAy_3_8910::OSC_COUNT),
        PARAM_ENVELOPE_FM,
        ENUMS(PARAM_LEVEL,       GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(PARAM_TONE,        GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(PARAM_NOISE,       GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(PARAM_ENVELOPE_ON, GeneralInstrumentAy_3_8910::OSC_COUNT),
        PARAM_NOISE_PERIOD,
        PARAM_ENVELOPE_MODE,
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT,        GeneralInstrumentAy_3_8910::OSC_COUNT),
        INPUT_ENVELOPE_VOCT,
        ENUMS(INPUT_FM,          GeneralInstrumentAy_3_8910::OSC_COUNT),
        INPUT_ENVELOPE_FM,
        ENUMS(INPUT_LEVEL,       GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_TONE,        GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_NOISE,       GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_ENVELOPE_ON, GeneralInstrumentAy_3_8910::OSC_COUNT),
        INPUT_NOISE_PERIOD,
        INPUT_ENVELOPE_MODE,
        ENUMS(INPUT_RESET,       GeneralInstrumentAy_3_8910::OSC_COUNT),
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
            configParam<BooleanParamQuantity>(PARAM_TONE  + oscillator,  0,     1,    1,   name + " Tone");
            configParam<BooleanParamQuantity>(PARAM_NOISE + oscillator,  0,     1,    0,   name + " Noise");
            configParam<BooleanParamQuantity>(PARAM_ENVELOPE_ON + oscillator,  0,     1,    0,   name + " Envelope");
        }
        configParam(PARAM_NOISE_PERIOD, 0, 31, 0, "Noise Period");
        configParam(PARAM_ENVELOPE_FREQ, -5.5, 9, 1.75, "Envelope Frequency", " Hz", 2);
        configParam(PARAM_ENVELOPE_FM, -1, 1, 0, "Envelope FM");
        configParam<TriggerParamQuantity>(PARAM_ENVELOPE_MODE, 0, 1, 0, "Envelope Mode");
    }

    /// @brief Respond to the module being reset by the engine.
    inline void onReset() override {
        ChipModule<GeneralInstrumentAy_3_8910>::onReset();
        envMode = 0;
    }

    /// @brief Respond to the module being randomized by the engine.
    inline void onRandomize() override { envMode = random::u32() % 8; }

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
        freq = Math::clip(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = buffers[channel][oscillator].get_clock_rate() / (CLOCK_DIVISION * freq);
        return Math::clip(freq, FREQ12BIT_MIN, FREQ12BIT_MAX);
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
        auto voltage = inputs[INPUT_LEVEL + oscillator].getNormalVoltage(normal, channel);
        if (apu[channel].is_dac_enabled(oscillator)) {
            // NOTE: voltage will be normalled by a previous call to getFrequency
            // get the offset control from the frequency input
            auto offset = rescale(params[PARAM_FREQ + oscillator].getValue(), -5.f, 5.f, 0.f, 5.f);
            offset += inputs[INPUT_VOCT + oscillator].getVoltage(channel) / 2.f;
            // get the scale control from the FM input
            auto scale = rescale(params[PARAM_FM + oscillator].getValue(), -1.f, 1.f, 0.f, 2.f);
            scale += -1 + inputs[INPUT_FM + oscillator].getVoltage(channel) / 5.f;
            // apply the scaling and offset to the voltage before normalling
            voltage = scale * (offset + voltage);
        }
        // normal the voltage forward in the chain by resetting the voltage
        // for this oscillator's level input
        inputs[INPUT_LEVEL + oscillator].setVoltage(voltage, channel);
        // apply the control voltage to the level. Normal to a constant
        // 10V source instead of checking if the cable is connected
        level = roundf(level * Math::Eurorack::fromDC(voltage));
        // get the 8-bit attenuation by inverting the level and clipping
        // to the legal bounds of the parameter
        // // the maximal value for the volume width register
        static constexpr float MAX = 15;
        return Math::clip(level, 0.f, MAX);
    }

    /// @brief Return whether the given oscillator has the envelope enabled.
    ///
    /// @param channel the polyphonic channel to return the envelope enabled
    /// parameter of
    /// @param oscillator the index of the oscillator to return the envelope
    /// enabled parameter of
    /// @returns true if the oscillator has the envelope generator enabled
    ///
    inline bool isEnvelopeOn(unsigned osc, unsigned channel) {
        // clamp the input within [0, 10]. this allows bipolar signals to
        // be interpreted as unipolar signals for the trigger input
        auto cv = Math::clip(inputs[INPUT_ENVELOPE_ON + osc].getVoltage(channel), 0.f, 10.f);
        envTriggers[channel][osc].process(rescale(cv, 0.01f, 2.f, 0.f, 1.f));
        // return the state of the switch based on the parameter and trig input
        return params[PARAM_ENVELOPE_ON + osc].getValue() - envTriggers[channel][osc].isHigh();
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
        return MAX - Math::clip(floorf(param + mod), 0.f, MAX);
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
        // const auto att = params[PARAM_ENVELOPE_FM].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 5V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        // const auto normalMod = inputs[INPUT_ENVELOPE_FM - 1].getVoltage(channel);
        // const auto mod = inputs[INPUT_ENVELOPE_FM].getNormalVoltage(normalMod, channel);
        // pitch += att * mod / 5.f;
        // convert the pitch to frequency based on standard exponential scale
        float freq = 1 * powf(2.0, pitch);
        freq = Math::clip(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = buffers[channel][0].get_clock_rate() / (CLOCK_DIVISION * freq);
        return Math::clip(freq, FREQ16BIT_MIN, FREQ16BIT_MAX);
    }

    /// @brief Return the envelope mode.
    ///
    /// @param channel the polyphonic channel to return the envelope mode for
    /// @returns the 4-bit envelope mode from parameters and CV inputs
    ///
    inline uint8_t getEnvelopeMode(unsigned channel) {
        // TODO: don't process this in a channel-wise processing block
        // detect presses to the trigger and cycle the mode
        if (envModeTrigger.process(params[PARAM_ENVELOPE_MODE].getValue()))
            envMode = (envMode + 1) % 8;
        // map the envelope modes to new values
        // Bit 4: Continue
        // Bit 3: Attack
        // Bit 2: Alternate
        // Bit 1: Hold
        static constexpr uint8_t ENV_MODE_MAP[8] = {
            0b1111,  //  /_____   |
            0b1001,  //  \_____   |
            0b1101,  //  /-----   |
            0b1011,  //  \-----   |
            0b1100,  //  //////   |
            0b1000,  //  \\\\\\   |
            0b1110,  //  /\/\/\   |
            0b1010   //  \/\/\/   |
        };
        return ENV_MODE_MAP[envMode];
    }

    /// @brief Return the mixer byte.
    ///
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 6-bit mixer byte from parameters and CV inputs
    ///
    inline uint8_t getChannelEnables(unsigned channel) {
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
            auto cv = Math::clip(inputs[INPUT_TONE + i].getVoltage(channel), 0.f, 10.f);
            mixerTriggers[channel][i].process(rescale(cv, 0.01f, 2.f, 0.f, 1.f));
            // get the state of the tone based on the parameter and trig input
            bool toneState = params[PARAM_TONE + i].getValue() - mixerTriggers[channel][i].isHigh();
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
        return syncTriggers[channel][index].process(rescale(sync, 0.01f, 2.f, 0.f, 1.f));
    }

    /// @brief Process the audio rate inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the audio inputs to
    ///
    inline void processAudio(const ProcessArgs& args, const unsigned& channel) final {
        // oscillators (processed in order for port normalling)
        for (unsigned osc = 0; osc < GeneralInstrumentAy_3_8910::OSC_COUNT; osc++) {
            if (getReset(osc, channel)) apu[channel].reset_phase(osc);
            // get frequency before level to normal voltage for the bias and
            // amplifier feature of the DAC mode
            apu[channel].set_frequency(osc, getFrequency(osc, channel));
            apu[channel].set_voice_volume(osc, getLevel(osc, channel), isEnvelopeOn(osc, channel));
        }
        if (getReset(3, channel)) apu[channel].reset_envelope_phase();
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    inline void processCV(const ProcessArgs& args, const unsigned& channel) final {
        apu[channel].set_channel_enables(getChannelEnables(channel));
        // envelope (processed after oscillators for port normalling)
        apu[channel].set_envelope_mode(getEnvelopeMode(channel));
        // noise
        apu[channel].set_noise_period(getNoisePeriod(channel));
        apu[channel].set_envelope_period(getEnvelopePeriod(channel));
    }

    /// @brief Process the lights on the module.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channels the number of active polyphonic channels
    ///
    inline void processLights(const ProcessArgs& args, const unsigned& channels) final {
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
            addParam(createParam<Trimpot>(     Vec(12 + 70 * i, 45),  module, Jairasullator::PARAM_FREQ + i));
            addInput(createInput<PJ301MPort>(  Vec(10 + 70 * i, 85),  module, Jairasullator::INPUT_VOCT + i));
            // FM
            addInput(createInput<PJ301MPort>(  Vec(10 + 70 * i, 129), module, Jairasullator::INPUT_FM + i));
            addParam(createParam<Trimpot>(     Vec(12 + 70 * i, 173), module, Jairasullator::PARAM_FM + i));
            // Level
            addParam(createSnapParam<Trimpot>( Vec(12 + 70 * i, 221), module, Jairasullator::PARAM_LEVEL + i));
            addInput(createInput<PJ301MPort>(  Vec(10 + 70 * i, 263), module, Jairasullator::INPUT_LEVEL + i));
            // Hard Sync
            addInput(createInput<PJ301MPort>(Vec(10 + 70 * i, 316), module, Jairasullator::INPUT_RESET + i));
            // COLUMN 2
            // Tone Enable
            addParam(createParam<CKSS>(        Vec(49 + 70 * i, 44), module, Jairasullator::PARAM_TONE + i));
            addInput(createInput<PJ301MPort>(  Vec(45 + 70 * i, 86), module, Jairasullator::INPUT_TONE + i));
            // Noise Enable
            addInput(createInput<PJ301MPort>(  Vec(45 + 70 * i, 130), module, Jairasullator::INPUT_NOISE + i));
            addParam(createParam<CKSS>(        Vec(49 + 70 * i, 171), module, Jairasullator::PARAM_NOISE + i));
            // Envelope Enables
            addParam(createParam<CKSS>(Vec(49 + 70 * i, 225), module, Jairasullator::PARAM_ENVELOPE_ON + i));
            addInput(createInput<PJ301MPort>(  Vec(45 + 70 * i, 264), module, Jairasullator::INPUT_ENVELOPE_ON + i));
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

    void appendContextMenu(Menu* menu) override {
        // get a pointer to the module
        Jairasullator* const module = dynamic_cast<Jairasullator*>(this->module);

        /// a structure for holding changes to the model items
        struct EnvelopeModeItem : MenuItem {
            /// the module to update
            Jairasullator* module;

            /// the currently selected envelope mode
            int envMode;

            /// Response to an action update to this item
            void onAction(const event::Action& e) override {
                module->envMode = envMode;
            }
        };

        // string representations of the envelope modes
        static const std::string LABELS[8] = {
            "/_____ (Attack)",
            "\\_____ (Decay)",
            "/----- (Attack & Max)",
            "\\----- (Decay & Max)",
            "////// (Attack LFO)",
            "\\\\\\\\\\\\ (Decay LFO)",
            "/\\/\\/\\ (Attack-Decay LFO)",
            "\\/\\/\\/ (Decay-Attack LFO)"
        };

        // add the envelope mode selection item to the menu
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Envelope Mode"));
        for (int i = 0; i < 8; i++) {
            auto item = createMenuItem<EnvelopeModeItem>(LABELS[i], CHECKMARK(module->envMode == i));
            item->module = module;
            item->envMode = i;
            menu->addChild(item);
        }
    }

};

/// the global instance of the model
Model *modelJairasullator = createModel<Jairasullator, JairasullatorWidget>("AY_3_8910");
