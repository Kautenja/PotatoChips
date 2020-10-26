// A Eurorack module based on a Texas Instruments SN76489 chip emulation.
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
#include "dsp/texas_instruments_sn76489.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// @brief A Texas Instruments SN76489 chip emulator module.
struct MegaTone : ChipModule<TexasInstrumentsSN76489> {
 private:
    /// whether to update the noise control (based on LFSR update)
    bool update_noise_control[PORT_MAX_CHANNELS];
    /// the current noise period
    uint8_t noise_period[PORT_MAX_CHANNELS];
    /// a Schmitt Trigger for handling inputs to the LFSR port
    dsp::BooleanTrigger lfsr[PORT_MAX_CHANNELS];

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ, TexasInstrumentsSN76489::TONE_COUNT),
        PARAM_NOISE_PERIOD,
        ENUMS(PARAM_FM_ATT, TexasInstrumentsSN76489::TONE_COUNT),
        PARAM_LFSR,
        ENUMS(PARAM_LEVEL, TexasInstrumentsSN76489::OSC_COUNT),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT, TexasInstrumentsSN76489::TONE_COUNT),
        INPUT_NOISE_PERIOD,
        ENUMS(INPUT_FM, TexasInstrumentsSN76489::TONE_COUNT),
        INPUT_LFSR,
        ENUMS(INPUT_LEVEL, TexasInstrumentsSN76489::OSC_COUNT),
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_OSCILLATOR, TexasInstrumentsSN76489::OSC_COUNT),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(LIGHTS_LEVEL, 3 * TexasInstrumentsSN76489::OSC_COUNT),
        NUM_LIGHTS
    };

    /// @brief Initialize a new Mega Tone module.
    MegaTone() : ChipModule<TexasInstrumentsSN76489>() {
        normal_outputs = true;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned i = 0; i < TexasInstrumentsSN76489::OSC_COUNT; i++) {
            if (i < TexasInstrumentsSN76489::NOISE) {  // tone generator
                configParam(PARAM_FREQ   + i, -2.5f, 2.5f, 0.f, "Tone " + std::to_string(i + 1) + " Frequency",  " Hz", 2, dsp::FREQ_C4);
                configParam(PARAM_FM_ATT + i, -1,    1,    0,   "Tone " + std::to_string(i + 1) + " Fine Tune / FM Attenuverter");
                configParam(PARAM_LEVEL  + i,  0,   15,    7,   "Tone " + std::to_string(i + 1) + " Volume / Amplifier Attenuator");
            } else {  // noise generator
                configParam(PARAM_FREQ   + i, 0,  3, 0, "Noise Mode");
                configParam(PARAM_FM_ATT + i, 0,  1, 0, "LFSR");
                configParam(PARAM_LEVEL  + i, 0, 15, 7, "Noise Volume / Amplifier Attenuator");
            }
        }
        // setup the control register values
        memset(update_noise_control, true, sizeof update_noise_control);
        memset(noise_period, 0, sizeof noise_period);
    }

 protected:
    /// @brief Get the 10-bit frequency parameter for the given pulse voice.
    ///
    /// @param voice the voice to return the frequency for
    /// @param channel the polyphonic channel to return the frequency for
    /// @returns the 10 bit frequency value from the panel
    ///
    inline uint16_t getFrequency(unsigned voice, unsigned channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ10BIT_MIN = 9;
        // the maximal value for the frequency register
        static constexpr float FREQ10BIT_MAX = 1023;
        // the clock division of the voice relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + voice].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 0V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normalPitch = voice ? inputs[INPUT_VOCT + voice - 1].getVoltage(channel) : 0.f;
        const auto pitchCV = inputs[INPUT_VOCT + voice].getNormalVoltage(normalPitch, channel);
        inputs[INPUT_VOCT + voice].setVoltage(pitchCV, channel);
        pitch += pitchCV;
        // get the attenuverter parameter value
        const auto att = params[PARAM_FM_ATT + voice].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 5V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normalMod = voice ? inputs[INPUT_FM + voice - 1].getVoltage(channel) : 5.f;
        const auto mod = inputs[INPUT_FM + voice].getNormalVoltage(normalMod, channel);
        inputs[INPUT_FM + voice].setVoltage(mod, channel);
        pitch += att * mod / 5.f;
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to an 11-bit value
        freq = (buffers[channel][voice].get_clock_rate() / (CLOCK_DIVISION * freq));
        return rack::clamp(freq, FREQ10BIT_MIN, FREQ10BIT_MAX);
    }

    /// @brief Return the period of the noise voice from the panel controls.
    ///
    /// @param channel the polyphonic channel to return the noise period for
    /// @returns the period for the noise voice with given channel
    ///
    inline uint8_t getNoisePeriod(unsigned channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ_MIN = 0;
        // the maximal value for the frequency register
        static constexpr float FREQ_MAX = 3;
        // get the attenuation from the parameter knob
        float freq = params[PARAM_NOISE_PERIOD].getValue();
        // apply the control voltage to the attenuation
        if (inputs[INPUT_NOISE_PERIOD].isConnected())
            freq += inputs[INPUT_NOISE_PERIOD].getVoltage(channel) / 2.f;
        return FREQ_MAX - rack::clamp(floorf(freq), FREQ_MIN, FREQ_MAX);
    }

    /// @brief Return the volume level from the panel controls.
    ///
    /// @param voice the voice to return the volume level of
    /// @param channel the polyphonic channel to return the volume for
    /// @returns the volume level of the given voice
    ///
    inline uint8_t getVolume(unsigned voice, unsigned channel) {
        // the minimal value for the volume width register
        static constexpr float MIN = 0;
        // the maximal value for the volume width register
        static constexpr float MAX = 15;
        // get the level from the parameter knob
        auto level = params[PARAM_LEVEL + voice].getValue();
        // get the normalled input voltage based on the voice index. Voice 0
        // has no prior voltage, and is thus normalled to 10V. Reset this port's
        // voltage afterward to propagate the normalling chain forward.
        const auto normal = voice ? inputs[INPUT_LEVEL + voice - 1].getVoltage(channel) : 10.f;
        const auto voltage = inputs[INPUT_LEVEL + voice].getNormalVoltage(normal, channel);
        inputs[INPUT_LEVEL + voice].setVoltage(voltage, channel);
        // apply the control voltage to the level. Normal to a constant
        // 10V source instead of checking if the cable is connected
        level = roundf(level * voltage / 10.f);
        // get the 8-bit attenuation by inverting the level and clipping
        // to the legal bounds of the parameter
        return MAX - rack::clamp(level, MIN, MAX);
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    void processCV(const ProcessArgs &args, unsigned channel) final {
        lfsr[channel].process(rescale(inputs[INPUT_LFSR].getVoltage(channel), 0.f, 2.f, 0.f, 1.f));
        // ---------------------------------------------------------------
        // pulse voice (3)
        // ---------------------------------------------------------------
        for (unsigned voice = 0; voice < TexasInstrumentsSN76489::TONE_COUNT; voice++) {
            // 10-bit frequency
            auto freq = getFrequency(voice, channel);
            uint8_t lo = 0b00001111 & freq;
            uint8_t hi = 0b00111111 & (freq >> 4);
            auto offset = (2 * voice) << 4;
            apu[channel].write((TexasInstrumentsSN76489::TONE_0_FREQUENCY + offset) | lo);
            apu[channel].write(hi);
            // 4-bit attenuation
            apu[channel].write((TexasInstrumentsSN76489::TONE_0_ATTENUATION + offset) | getVolume(voice, channel));
        }
        // ---------------------------------------------------------------
        // noise voice
        // ---------------------------------------------------------------
        // 2-bit noise period
        auto period = getNoisePeriod(channel);
        // determine the state of the LFSR switch
        bool is_lfsr = !(params[PARAM_LFSR].getValue() - lfsr[channel].state);
        // update noise registers if a variable has changed
        if (period != noise_period[channel] or update_noise_control[channel] != is_lfsr) {
            apu[channel].write(
                TexasInstrumentsSN76489::NOISE_CONTROL |
                (0b00000011 & period) |
                is_lfsr * TexasInstrumentsSN76489::NOISE_FEEDBACK
            );
            noise_period[channel] = period;
            update_noise_control[channel] = is_lfsr;
        }
        // set the 4-bit attenuation value
        apu[channel].write(TexasInstrumentsSN76489::NOISE_ATTENUATION | getVolume(TexasInstrumentsSN76489::NOISE, channel));
    }

    /// @brief Process the lights on the module.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channels the number of active polyphonic channels
    ///
    inline void processLights(const ProcessArgs &args, unsigned channels) final {
        for (unsigned voice = 0; voice < TexasInstrumentsSN76489::OSC_COUNT; voice++) {
            // get the global brightness scale from -12 to 3
            auto brightness = vuMeter[voice].getBrightness(-12, 3);
            // set the red light based on total brightness and
            // brightness from 0dB to 3dB
            lights[voice * 3 + 0].setBrightness(brightness * vuMeter[voice].getBrightness(0, 3));
            // set the red light based on inverted total brightness and
            // brightness from -12dB to 0dB
            lights[voice * 3 + 1].setBrightness((1 - brightness) * vuMeter[voice].getBrightness(-12, 0));
            // set the blue light to off
            lights[voice * 3 + 2].setBrightness(0);
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// @brief The panel widget for the Mega Tone module.
struct MegaToneWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit MegaToneWidget(MegaTone *module) {
        setModule(module);
        static constexpr auto panel = "res/MegaTone.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // components
        for (unsigned i = 0; i < TexasInstrumentsSN76489::OSC_COUNT; i++) {
            // Frequency / Noise Period
            auto freq = createParam<Trimpot>(  Vec(12 + 35 * i, 45),  module, MegaTone::PARAM_FREQ        + i);
            if (i == TexasInstrumentsSN76489::NOISE)
                freq->snap = true;
            addParam(freq);
            addInput(createInput<PJ301MPort>(  Vec(10 + 35 * i, 85),  module, MegaTone::INPUT_VOCT        + i));
            // FM / LFSR
            addInput(createInput<PJ301MPort>(  Vec(10 + 35 * i, 129), module, MegaTone::INPUT_FM          + i));
            if (i < TexasInstrumentsSN76489::TONE_COUNT)
                addParam(createParam<Trimpot>( Vec(12 + 35 * i, 173), module, MegaTone::PARAM_FM_ATT      + i));
            else
                addParam(createParam<CKSS>(    Vec(120, 173), module, MegaTone::PARAM_FM_ATT              + i));
            // Level
            auto level = createParam<Trimpot>( Vec(12 + 35 * i, 221), module, MegaTone::PARAM_LEVEL       + i);
            level->snap = true;
            addParam(level);
            addInput(createInput<PJ301MPort>(  Vec(10 + 35 * i, 263), module, MegaTone::INPUT_LEVEL       + i));
            addChild(createLight<MediumLight<RedGreenBlueLight>>(Vec(17 + 35 * i, 297), module, MegaTone::LIGHTS_LEVEL + 3 * i));
            // Output
            addOutput(createOutput<PJ301MPort>(Vec(10 + 35 * i, 324), module, MegaTone::OUTPUT_OSCILLATOR + i));
        }
    }
};

/// the global instance of the model
Model *modelMegaTone = createModel<MegaTone, MegaToneWidget>("SN76489");
