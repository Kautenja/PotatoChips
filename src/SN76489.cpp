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

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[TexasInstrumentsSN76489::OSC_COUNT];
    /// The SN76489 instance to synthesize sound with
    TexasInstrumentsSN76489 apu;

    /// a Schmitt Trigger for handling inputs to the LFSR port
    dsp::BooleanTrigger lfsr;

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

    /// a VU meter for keeping track of the channel levels
    dsp::VuMeter2 chMeters[TexasInstrumentsSN76489::OSC_COUNT];
    /// a clock divider for updating the mixer LEDs
    dsp::ClockDivider lightDivider;

 public:
    enum ParamIds {
        ENUMS(PARAM_FREQ, TexasInstrumentsSN76489::OSC_COUNT - 1),
        PARAM_NOISE_PERIOD,
        PARAM_LFSR,
        ENUMS(PARAM_LEVEL, TexasInstrumentsSN76489::OSC_COUNT),
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, TexasInstrumentsSN76489::OSC_COUNT - 1),
        INPUT_NOISE_PERIOD,
        INPUT_LFSR,
        ENUMS(INPUT_FM, TexasInstrumentsSN76489::OSC_COUNT - 1),
        ENUMS(INPUT_LEVEL, TexasInstrumentsSN76489::OSC_COUNT),
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, TexasInstrumentsSN76489::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds {
        ENUMS(LIGHTS_LEVEL, TexasInstrumentsSN76489::OSC_COUNT),
        LIGHT_COUNT
    };

    /// Initialize a new SN76489 Chip module.
    ChipSN76489() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ + 0, -30.f, 30.f, 0.f, "Tone 1 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -30.f, 30.f, 0.f, "Tone 2 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2, -30.f, 30.f, 0.f, "Tone 3 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_NOISE_PERIOD, 0, 4, 0, "Noise Control", "");
        configParam(PARAM_LFSR, 0, 1, 1, "LFSR Polarity", "");
        configParam(PARAM_LEVEL + 0, 0, 1, 0.5, "Tone 1 Level", "%", 0, 100);
        configParam(PARAM_LEVEL + 1, 0, 1, 0.5, "Tone 2 Level", "%", 0, 100);
        configParam(PARAM_LEVEL + 2, 0, 1, 0.5, "Tone 3 Level", "%", 0, 100);
        configParam(PARAM_LEVEL + 3, 0, 1, 0.5, "Noise Level",  "%", 0, 100);
        cvDivider.setDivision(16);
        lightDivider.setDivision(512);
        // set the output buffer for each individual voice
        for (int i = 0; i < TexasInstrumentsSN76489::OSC_COUNT; i++)
            apu.set_output(i, &buf[i]);
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.volume(3.f);
    }

    /// Process pulse wave for given channel.
    ///
    /// @param channel the channel to process the pulse wave for
    ///
    void channel_pulse(int channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ10BIT_MIN = 0;
        // the maximal value for the frequency register
        static constexpr float FREQ10BIT_MAX = 1023;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // the constant modulation factor
        static constexpr auto MOD_FACTOR = 10.f;
        // the minimal value for the volume width register
        static constexpr float ATT_MIN = 0;
        // the maximal value for the volume width register
        static constexpr float ATT_MAX = 15;

        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + channel].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + channel].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM + channel].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to an 11-bit value
        freq = (buf[channel].get_clock_rate() / (CLOCK_DIVISION * freq));
        uint16_t freq10bit = rack::clamp(freq, FREQ10BIT_MIN, FREQ10BIT_MAX);
        // calculate the high and low bytes from the 10-bit frequency
        uint8_t lo = 0b00001111 & freq10bit;
        uint8_t hi = 0b00111111 & (freq10bit >> 4);
        // write the data to the chip
        const auto channel_opcode_offset = (2 * channel) << 4;
        apu.write_data((TONE_1_FREQUENCY + channel_opcode_offset) | lo);
        apu.write_data(hi);

        // get the attenuation from the parameter knob
        auto attenuationParam = params[PARAM_LEVEL + channel].getValue();
        // apply the control voltage to the attenuation
        if (inputs[INPUT_LEVEL + channel].isConnected()) {
            auto cv = rack::clamp(inputs[INPUT_LEVEL + 3].getVoltage() / 10.f, 0.f, 1.f);
            cv = roundf(100.f * cv) / 100.f;
            attenuationParam *= 2 * cv;
        }
        // get the 8-bit attenuation clamped within legal limits
        uint8_t attenuation = ATT_MAX - rack::clamp(ATT_MAX * attenuationParam, ATT_MIN, ATT_MAX);
        apu.write_data((TONE_1_ATTENUATION + channel_opcode_offset) | attenuation);
    }

    /// Process noise (channel 3).
    void channel_noise() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ_MIN = 0;
        // the maximal value for the frequency register
        static constexpr float FREQ_MAX = 3;
        // the minimal value for the volume width register
        static constexpr float ATT_MIN = 0;
        // the maximal value for the volume width register
        static constexpr float ATT_MAX = 15;

        // get the attenuation from the parameter knob
        float freq = params[PARAM_NOISE_PERIOD].getValue();
        // apply the control voltage to the attenuation
        if (inputs[INPUT_NOISE_PERIOD].isConnected())
            freq += inputs[INPUT_NOISE_PERIOD].getVoltage() / 2.f;
        uint8_t period = FREQ_MAX - rack::clamp(floorf(freq), FREQ_MIN, FREQ_MAX);
        bool state = (1 - params[PARAM_LFSR].getValue()) - !lfsr.state;
        if (period != noise_period or update_noise_control != state) {
            apu.write_data(NOISE_CONTROL | (0b00000011 & period) | state * NOISE_FEEDBACK);
            noise_period = period;
            update_noise_control = state;
        }

        // get the attenuation from the parameter knob
        auto attenuationParam = params[PARAM_LEVEL + 3].getValue();
        // apply the control voltage to the attenuation
        if (inputs[INPUT_LEVEL + 3].isConnected()) {
            auto cv = rack::clamp(inputs[INPUT_LEVEL + 3].getVoltage() / 10.f, 0.f, 1.f);
            cv = roundf(100.f * cv) / 100.f;
            attenuationParam *= 2 * cv;
        }
        // get the 8-bit attenuation clamped within legal limits
        uint8_t attenuation = ATT_MAX - rack::clamp(ATT_MAX * attenuationParam, ATT_MIN, ATT_MAX);
        apu.write_data(NOISE_ATTENUATION | attenuation);
    }

    /// Return a 10V signed sample from the APU.
    ///
    /// @param channel the channel to get the audio sample for
    ///
    float getAudioOut(int channel) {
        // the peak to peak output of the voltage
        static constexpr float Vpp = 10.f;
        // the amount of voltage per increment of 16-bit fidelity volume
        static constexpr float divisor = std::numeric_limits<int16_t>::max();
        // convert the 16-bit sample to 10Vpp floating point
        return Vpp * buf[channel].read_sample() / divisor;
    }

    /// Process a sample.
    void process(const ProcessArgs &args) override {
        // check for sample rate changes from the engine to send to the chip
        if (new_sample_rate) {
            // update the buffer for each channel
            for (int i = 0; i < TexasInstrumentsSN76489::OSC_COUNT; i++)
                buf[i].set_sample_rate(args.sampleRate, CLOCK_RATE);
            // clear the new sample rate flag
            new_sample_rate = false;
        }
        if (cvDivider.process()) {  // process the CV inputs to the chip
            lfsr.process(rescale(inputs[INPUT_LFSR].getVoltage(), 0.f, 2.f, 0.f, 1.f));
            // process the data on the chip
            channel_pulse(0);
            channel_pulse(1);
            channel_pulse(2);
            channel_noise();
        }
        // process audio samples on the chip engine
        apu.end_frame(CLOCK_RATE / args.sampleRate);
        // set the output voltages (and process the levels in VU)
        for (int i = 0; i < TexasInstrumentsSN76489::OSC_COUNT; i++) {
            auto channelOutput = getAudioOut(i);
            chMeters[i].process(args.sampleTime, channelOutput / 5.f);
            outputs[OUTPUT_CHANNEL + i].setVoltage(channelOutput);
        }
        if (lightDivider.process()) {  // update the mixer lights
            for (int i = 0; i < TexasInstrumentsSN76489::OSC_COUNT; i++) {
                float b = chMeters[i].getBrightness(-24.f, 0.f);
                lights[LIGHTS_LEVEL + i].setBrightness(b);
            }
        }
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override { new_sample_rate = true; }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The widget structure that lays out the panel of the module and the UI menus.
struct ChipSN76489Widget : ModuleWidget {
    ChipSN76489Widget(ChipSN76489 *module) {
        setModule(module);
        static constexpr auto panel = "res/SN76489.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // V/OCT inputs
        addInput(createInput<PJ301MPort>(Vec(19, 73),  module, ChipSN76489::INPUT_VOCT + 0));
        addInput(createInput<PJ301MPort>(Vec(19, 158), module, ChipSN76489::INPUT_VOCT + 1));
        addInput(createInput<PJ301MPort>(Vec(19, 243), module, ChipSN76489::INPUT_VOCT + 2));
        // FM inputs
        addInput(createInput<PJ301MPort>(Vec(19, 38),  module, ChipSN76489::INPUT_FM + 0));
        addInput(createInput<PJ301MPort>(Vec(19, 123), module, ChipSN76489::INPUT_FM + 1));
        addInput(createInput<PJ301MPort>(Vec(19, 208), module, ChipSN76489::INPUT_FM + 2));
        // Frequency parameters
        addParam(createParam<Rogan5PSGray>(Vec(46, 39),  module, ChipSN76489::PARAM_FREQ + 0));
        addParam(createParam<Rogan5PSGray>(Vec(46, 124), module, ChipSN76489::PARAM_FREQ + 1));
        addParam(createParam<Rogan5PSGray>(Vec(46, 209), module, ChipSN76489::PARAM_FREQ + 2));
        // noise period
        addParam(createParam<Rogan1PWhite>(Vec(64, 296), module, ChipSN76489::PARAM_NOISE_PERIOD));
        addInput(createInput<PJ301MPort>(Vec(76, 332),   module, ChipSN76489::INPUT_NOISE_PERIOD));
        // LFSR switch
        addParam(createParam<CKSS>(Vec(22, 288),       module, ChipSN76489::PARAM_LFSR));
        addInput(createInput<PJ301MPort>(Vec(19, 326), module, ChipSN76489::INPUT_LFSR));
        // Level
        addParam(createLightParam<LEDLightSlider<GreenLight>>(Vec(107, 24),  module, ChipSN76489::PARAM_LEVEL + 0, ChipSN76489::LIGHTS_LEVEL + 0));
        addParam(createLightParam<LEDLightSlider<GreenLight>>(Vec(107, 109), module, ChipSN76489::PARAM_LEVEL + 1, ChipSN76489::LIGHTS_LEVEL + 1));
        addParam(createLightParam<LEDLightSlider<GreenLight>>(Vec(107, 194), module, ChipSN76489::PARAM_LEVEL + 2, ChipSN76489::LIGHTS_LEVEL + 2));
        addParam(createLightParam<LEDLightSlider<GreenLight>>(Vec(107, 279), module, ChipSN76489::PARAM_LEVEL + 3, ChipSN76489::LIGHTS_LEVEL + 3));
        addInput(createInput<PJ301MPort>(Vec(135, 28),   module, ChipSN76489::INPUT_LEVEL + 0));
        addInput(createInput<PJ301MPort>(Vec(135, 113),  module, ChipSN76489::INPUT_LEVEL + 1));
        addInput(createInput<PJ301MPort>(Vec(135, 198),  module, ChipSN76489::INPUT_LEVEL + 2));
        addInput(createInput<PJ301MPort>(Vec(135, 283),  module, ChipSN76489::INPUT_LEVEL + 3));
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(137, 74),  module, ChipSN76489::OUTPUT_CHANNEL + 0));
        addOutput(createOutput<PJ301MPort>(Vec(137, 159), module, ChipSN76489::OUTPUT_CHANNEL + 1));
        addOutput(createOutput<PJ301MPort>(Vec(137, 244), module, ChipSN76489::OUTPUT_CHANNEL + 2));
        addOutput(createOutput<PJ301MPort>(Vec(137, 329), module, ChipSN76489::OUTPUT_CHANNEL + 3));
    }
};

/// the global instance of the model
Model *modelChipSN76489 = createModel<ChipSN76489, ChipSN76489Widget>("SN76489");
