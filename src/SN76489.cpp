// A Texas Instruments SN76489 Chip module.
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
#include "dsp/texas_instruments_sn76489.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Texas Instruments SN76489 Chip module.
struct ChipSN76489 : Module {
 private:
    /// whether to update the noise control (based on LFSR update)
    bool update_noise_control = true;
    /// the current noise period
    uint8_t noise_period = 0;

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[TexasInstrumentsSN76489::OSC_COUNT];
    /// The SN76489 instance to synthesize sound with
    TexasInstrumentsSN76489 apu;

    /// a Schmitt Trigger for handling inputs to the LFSR port
    dsp::BooleanTrigger lfsr;

    /// a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

    /// a VU meter for keeping track of the oscillator levels
    dsp::VuMeter2 chMeters[TexasInstrumentsSN76489::OSC_COUNT];
    /// a clock divider for updating the mixer LEDs
    dsp::ClockDivider lightDivider;

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ, TexasInstrumentsSN76489::OSC_COUNT - 1),
        PARAM_NOISE_PERIOD,
        PARAM_LFSR,
        ENUMS(PARAM_LEVEL, TexasInstrumentsSN76489::OSC_COUNT),
        PARAM_COUNT
    };
    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT, TexasInstrumentsSN76489::OSC_COUNT - 1),
        INPUT_NOISE_PERIOD,
        INPUT_LFSR,
        ENUMS(INPUT_FM, TexasInstrumentsSN76489::OSC_COUNT - 1),
        ENUMS(INPUT_LEVEL, TexasInstrumentsSN76489::OSC_COUNT),
        INPUT_COUNT
    };
    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, TexasInstrumentsSN76489::OSC_COUNT),
        OUTPUT_COUNT
    };
    /// the indexes of lights on the module
    enum LightIds {
        ENUMS(LIGHTS_LEVEL, TexasInstrumentsSN76489::OSC_COUNT),
        LIGHT_COUNT
    };

    /// Initialize a new SN76489 Chip module.
    ChipSN76489() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        for (unsigned i = 0; i < TexasInstrumentsSN76489::OSC_COUNT; i++) {
            if (i < TexasInstrumentsSN76489::NOISE)
                configParam(PARAM_FREQ + i, -30.f, 30.f, 0.f, "Tone " + std::to_string(i + 1) + " Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
            configParam(PARAM_LEVEL + i, 0, 1, 0.8, "Tone " + std::to_string(i + 1) + " Level", "%", 0, 100);
        }
        configParam(PARAM_NOISE_PERIOD, 0, 4, 0, "Noise Control", "");
        configParam(PARAM_LFSR, 0, 1, 1, "LFSR Polarity", "");
        cvDivider.setDivision(16);
        lightDivider.setDivision(512);
        // set the output buffer for each individual voice
        for (int i = 0; i < TexasInstrumentsSN76489::OSC_COUNT; i++)
            apu.set_output(i, &buf[i]);
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.set_volume(3.f);
        onSampleRateChange();
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override {
        // update the buffer for each oscillator
        for (unsigned oscillator = 0; oscillator < TexasInstrumentsSN76489::OSC_COUNT; oscillator++)
            buf[oscillator].set_sample_rate(APP->engine->getSampleRate(), CLOCK_RATE);
    }

    /// Get the 10-bit frequency parameter for the given pulse oscillator.
    ///
    /// @param oscillator the oscillator to return the frequency for
    /// @returns the 10 bit frequency value from the panel
    ///
    inline uint16_t getFrequency(unsigned oscillator) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ10BIT_MIN = 9;
        // the maximal value for the frequency register
        static constexpr float FREQ10BIT_MAX = 1023;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + oscillator].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + oscillator].getVoltage();
        pitch += inputs[INPUT_FM + oscillator].getVoltage() / 5.f;
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to an 11-bit value
        freq = (buf[oscillator].get_clock_rate() / (CLOCK_DIVISION * freq));
        return rack::clamp(freq, FREQ10BIT_MIN, FREQ10BIT_MAX);
    }

    /// Return the period of the noise oscillator from the panel controls.
    inline uint8_t getNoisePeriod() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ_MIN = 0;
        // the maximal value for the frequency register
        static constexpr float FREQ_MAX = 3;
        // get the attenuation from the parameter knob
        float freq = params[PARAM_NOISE_PERIOD].getValue();
        // apply the control voltage to the attenuation
        if (inputs[INPUT_NOISE_PERIOD].isConnected())
            freq += inputs[INPUT_NOISE_PERIOD].getVoltage() / 2.f;
        return FREQ_MAX - rack::clamp(floorf(freq), FREQ_MIN, FREQ_MAX);
    }

    /// Return the volume level from the panel controls.
    ///
    /// @param oscillator the oscillator to return the volume level of
    /// @returns the volume level of the given oscillator
    ///
    inline uint8_t getVolume(unsigned oscillator) {
        // the minimal value for the volume width register
        static constexpr float ATT_MIN = 0;
        // the maximal value for the volume width register
        static constexpr float ATT_MAX = 15;
        // get the attenuation from the parameter knob
        auto attenuationParam = params[PARAM_LEVEL + oscillator].getValue();
        // apply the control voltage to the attenuation
        if (inputs[INPUT_LEVEL + oscillator].isConnected()) {
            auto cv = inputs[INPUT_LEVEL + oscillator].getVoltage();
            cv = rack::clamp(cv / 10.f, 0.f, 1.f);
            cv = roundf(100.f * cv) / 100.f;
            attenuationParam *= 2 * cv;
        }
        // get the 8-bit attenuation clamped within legal limits
        return ATT_MAX - rack::clamp(ATT_MAX * attenuationParam, ATT_MIN, ATT_MAX);
    }

    /// Return a 10V signed sample from the APU.
    ///
    /// @param oscillator the oscillator to get the audio sample for
    ///
    inline float getAudioOut(unsigned oscillator) {
        // the peak to peak output of the voltage
        static constexpr float Vpp = 10.f;
        // the amount of voltage per increment of 16-bit fidelity volume
        static constexpr float divisor = std::numeric_limits<int16_t>::max();
        // convert the 16-bit sample to 10Vpp floating point
        return Vpp * buf[oscillator].read_sample() / divisor;
    }

    /// @brief Process a sample.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    void process(const ProcessArgs &args) override {
        if (cvDivider.process()) {  // process the CV inputs to the chip
            auto lfsrV = math::clamp(inputs[INPUT_LFSR].getVoltage(), 0.f, 10.f);
            lfsr.process(rescale(lfsrV, 0.f, 2.f, 0.f, 1.f));
            // process the data on the chip
            for (unsigned i = 0; i < TexasInstrumentsSN76489::OSC_COUNT - 1; i++) {
                // 10-bit frequency
                auto freq = getFrequency(i);
                uint8_t lo = 0b00001111 & freq;
                uint8_t hi = 0b00111111 & (freq >> 4);
                const auto offset = (2 * i) << 4;
                apu.write((TexasInstrumentsSN76489::TONE_0_FREQUENCY + offset) | lo);
                apu.write(hi);
                // 4-bit attenuation
                apu.write((TexasInstrumentsSN76489::TONE_0_ATTENUATION + offset) | getVolume(i));
            }
            // 2-bit noise period
            auto period = getNoisePeriod();
            bool state = (1 - params[PARAM_LFSR].getValue()) - !lfsr.state;
            if (period != noise_period or update_noise_control != state) {
                apu.write(
                    TexasInstrumentsSN76489::NOISE_CONTROL |
                    (0b00000011 & period) |
                    state * TexasInstrumentsSN76489::NOISE_FEEDBACK
                );
                noise_period = period;
                update_noise_control = state;
            }
            // 4-bit attenuation
            apu.write(TexasInstrumentsSN76489::NOISE_ATTENUATION | getVolume(TexasInstrumentsSN76489::NOISE));
        }
        // process audio samples on the chip engine
        apu.end_frame(CLOCK_RATE / args.sampleRate);
        // set the output voltages (and process the levels in VU)
        for (unsigned oscillator = 0; oscillator < TexasInstrumentsSN76489::OSC_COUNT; oscillator++) {
            auto output = getAudioOut(oscillator);
            chMeters[oscillator].process(args.sampleTime, output / 5.f);
            outputs[OUTPUT_CHANNEL + oscillator].setVoltage(output);
        }
        if (lightDivider.process()) {  // update the mixer lights
            for (unsigned oscillator = 0; oscillator < TexasInstrumentsSN76489::OSC_COUNT; oscillator++) {
                float b = chMeters[oscillator].getBrightness(-24.f, 0.f);
                lights[LIGHTS_LEVEL + oscillator].setBrightness(b);
            }
        }
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The widget structure that lays out the panel of the module and the UI menus.
struct ChipSN76489Widget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    ChipSN76489Widget(ChipSN76489 *module) {
        setModule(module);
        static constexpr auto panel = "res/SN76489.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // components
        for (unsigned i = 0; i < TexasInstrumentsSN76489::OSC_COUNT; i++) {
            if (i < TexasInstrumentsSN76489::NOISE) {
                addInput(createInput<PJ301MPort>(  Vec(19, 73 + i * 85),  module, ChipSN76489::INPUT_VOCT     + i));
                addInput(createInput<PJ301MPort>(  Vec(19, 38 + i * 85),  module, ChipSN76489::INPUT_FM       + i));
                addParam(createParam<Rogan5PSGray>(Vec(46, 39 + i * 85),  module, ChipSN76489::PARAM_FREQ     + i));
            }
            addParam(createLightParam<LEDLightSlider<GreenLight>>(Vec(107, 24 + i * 85),  module, ChipSN76489::PARAM_LEVEL + i, ChipSN76489::LIGHTS_LEVEL + i));
            addInput(createInput<PJ301MPort>(      Vec(135, 28 + i * 85), module, ChipSN76489::INPUT_LEVEL    + i));
            addOutput(createOutput<PJ301MPort>(    Vec(137, 74 + i * 85), module, ChipSN76489::OUTPUT_CHANNEL + i));
        }
        addParam(createParam<Rogan1PWhite>(Vec(64, 296), module, ChipSN76489::PARAM_NOISE_PERIOD));
        addInput(createInput<PJ301MPort>(  Vec(76, 332), module, ChipSN76489::INPUT_NOISE_PERIOD));
        addParam(createParam<CKSS>(        Vec(22, 288), module, ChipSN76489::PARAM_LFSR));
        addInput(createInput<PJ301MPort>(  Vec(19, 326), module, ChipSN76489::INPUT_LFSR));
    }
};

/// the global instance of the model
Model *modelChipSN76489 = createModel<ChipSN76489, ChipSN76489Widget>("SN76489");
