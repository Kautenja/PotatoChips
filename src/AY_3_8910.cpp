// A General Instrument AY-3-8910 Chip module.
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
#include "dsp/general_instrument_ay_3_8910.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A General Instrument AY-3-8910 Chip module.
struct ChipAY_3_8910 : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(PARAM_LEVEL, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(PARAM_TONE, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(PARAM_NOISE, GeneralInstrumentAy_3_8910::OSC_COUNT),
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_FM, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_LEVEL, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_TONE, GeneralInstrumentAy_3_8910::OSC_COUNT),
        ENUMS(INPUT_NOISE, GeneralInstrumentAy_3_8910::OSC_COUNT),
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, GeneralInstrumentAy_3_8910::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds { LIGHT_COUNT };

    /// a Schmitt Trigger for handling player 1 button inputs
    dsp::BooleanTrigger mixerTriggers[2 * GeneralInstrumentAy_3_8910::OSC_COUNT];

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[GeneralInstrumentAy_3_8910::OSC_COUNT];
    /// The General Instrument AY-3-8910 instance to synthesize sound with
    GeneralInstrumentAy_3_8910 apu;

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

    /// Initialize a new FME7 Chip module.
    ChipAY_3_8910() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ + 0, -48.f, 48.f, 0.f,  "Pulse A Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -48.f, 48.f, 0.f,  "Pulse B Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2, -48.f, 48.f, 0.f,  "Pulse C Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_LEVEL + 0,  0.f,  1.f, 0.9f, "Pulse A Level",     "%",   0.f,                100.f       );
        configParam(PARAM_LEVEL + 1,  0.f,  1.f, 0.9f, "Pulse B Level",     "%",   0.f,                100.f       );
        configParam(PARAM_LEVEL + 2,  0.f,  1.f, 0.9f, "Pulse C Level",     "%",   0.f,                100.f       );
        configParam(PARAM_TONE + 0, 0, 1, 0, "Pulse A Tone Enabled", "");
        configParam(PARAM_TONE + 1, 0, 1, 0, "Pulse B Tone Enabled", "");
        configParam(PARAM_TONE + 2, 0, 1, 0, "Pulse C Tone Enabled", "");
        configParam(PARAM_NOISE + 0, 0, 1, 1, "Pulse A Noise Enabled", "");
        configParam(PARAM_NOISE + 1, 0, 1, 1, "Pulse B Noise Enabled", "");
        configParam(PARAM_NOISE + 2, 0, 1, 1, "Pulse C Noise Enabled", "");
        cvDivider.setDivision(16);
        // set the output buffer for each individual voice
        for (int i = 0; i < GeneralInstrumentAy_3_8910::OSC_COUNT; i++)
            apu.set_output(i, &buf[i]);
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.volume(3.f);
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
        freq = buf[channel].get_clock_rate() / (CLOCK_DIVISION * freq);
        return rack::clamp(freq, FREQ12BIT_MIN, FREQ12BIT_MAX);
    }

    /// Return the level for the given channel.
    ///
    /// @param channel the index of the channel to get the level of
    /// @returns the 4-bit level value in an 8-bit container
    ///
    inline uint8_t getLevel(int channel) {
        // the minimal value for the volume width register
        static constexpr float LEVEL_MIN = 0;
        // the maximal value for the volume width register
        static constexpr float LEVEL_MAX = 15;
        // get the level from the parameter knob
        auto levelParam = params[PARAM_LEVEL + channel].getValue();
        // apply the control voltage to the level
        if (inputs[INPUT_LEVEL + channel].isConnected())
            levelParam *= inputs[INPUT_LEVEL + channel].getVoltage() / 2.f;
        // get the 8-bit level clamped within legal limits
        uint8_t level = rack::clamp(LEVEL_MAX * levelParam, LEVEL_MIN, LEVEL_MAX);
        return level;
    }

    /// Return the noise period.
    ///
    /// @returns the period for the noise oscillator
    /// @details
    /// Returns a frequency based on the knob for channel 3
    ///
    inline uint8_t getNoise() { return getFrequency(2) >> 3; }

    /// Return the mixer byte.
    ///
    /// @returns the 8-bit mixer byte from parameters and CV inputs
    ///
    inline uint8_t getMixer() {
        uint8_t mixerByte = 0;
        for (std::size_t i = 0; i < GeneralInstrumentAy_3_8910::OSC_COUNT; i++) {
            // process the tone trigger
            mixerTriggers[2 * i].process(rescale(inputs[INPUT_TONE + i].getVoltage(), 0.f, 2.f, 0.f, 1.f));
            bool toneState = (1 - params[PARAM_TONE + i].getValue()) - !mixerTriggers[2 * i].state;
            mixerByte |= toneState << i;
            // process the noise trigger
            mixerTriggers[2 * i + 1].process(rescale(inputs[INPUT_NOISE + i].getVoltage(), 0.f, 2.f, 0.f, 1.f));
            bool noiseState = (1 - params[PARAM_NOISE + i].getValue()) - !mixerTriggers[2 * i + 1].state;
            mixerByte |= noiseState << (i + 3);
        }
        return mixerByte;
    }

    /// Return a 10V signed sample from the FME7.
    ///
    /// @param channel the channel to get the audio sample for
    ///
    inline float getAudioOut(int channel) {
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
            for (int i = 0; i < GeneralInstrumentAy_3_8910::OSC_COUNT; i++)
                buf[i].set_sample_rate(args.sampleRate, CLOCK_RATE);
            // clear the new sample rate flag
            new_sample_rate = false;
        }
        if (cvDivider.process()) {  // process the CV inputs to the chip
            // frequency
            for (int i = 0; i < GeneralInstrumentAy_3_8910::OSC_COUNT; i++) {
                auto freq = getFrequency(i);
                auto lo =  freq & 0b0000000011111111;
                apu.write(GeneralInstrumentAy_3_8910::PERIOD_CH_A_LO + 2 * i, lo);
                auto hi = (freq & 0b0000111100000000) >> 8;
                apu.write(GeneralInstrumentAy_3_8910::PERIOD_CH_A_HI + 2 * i, hi);
                auto level = getLevel(i);
                apu.write(GeneralInstrumentAy_3_8910::VOLUME_CH_A + i, level);
            }
            // set the 5-bit noise value based on the channel 3 parameter
            apu.write(GeneralInstrumentAy_3_8910::NOISE_PERIOD, getNoise());
            // set the 6-channel boolean mixer (tone and noise for each channel)
            apu.write(GeneralInstrumentAy_3_8910::CHANNEL_ENABLES, getMixer());
            // envelope period (TODO: fix envelop in engine)
            // apu.write(GeneralInstrumentAy_3_8910::PERIOD_ENVELOPE_LO, 0b10101011);
            // apu.write(GeneralInstrumentAy_3_8910::PERIOD_ENVELOPE_HI, 0b00000011);
            // envelope shape bits (TODO: fix envelop in engine)
            // apu.write(
            //     GeneralInstrumentAy_3_8910::ENVELOPE_SHAPE,
            //     GeneralInstrumentAy_3_8910::ENVELOPE_SHAPE_NONE
            // );
        }
        // process audio samples on the chip engine
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < GeneralInstrumentAy_3_8910::OSC_COUNT; i++)
            outputs[OUTPUT_CHANNEL + i].setVoltage(getAudioOut(i));
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override { new_sample_rate = true; }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The widget structure that lays out the panel of the module and the UI menus.
struct ChipAY_3_8910Widget : ModuleWidget {
    ChipAY_3_8910Widget(ChipAY_3_8910 *module) {
        setModule(module);
        static constexpr auto panel = "res/AY_3_8910.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        for (int i = 0; i < GeneralInstrumentAy_3_8910::OSC_COUNT; i++) {
            addInput(createInput<PJ301MPort>(    Vec(18,  27  + i * 111),  module, ChipAY_3_8910::INPUT_FM       + i));
            addInput(createInput<PJ301MPort>(    Vec(18,  100 + i * 111), module, ChipAY_3_8910::INPUT_VOCT      + i));
            addParam(createParam<Rogan6PSWhite>( Vec(47,  29  + i * 111),  module, ChipAY_3_8910::PARAM_FREQ     + i));
            addParam(createParam<CKSS>(          Vec(144,  29  + i * 111),  module, ChipAY_3_8910::PARAM_TONE    + i));
            addInput(createInput<PJ301MPort>(    Vec(147,  53 + i * 111), module, ChipAY_3_8910::INPUT_TONE      + i));
            addParam(createParam<CKSS>(          Vec(138,  105  + i * 111),  module, ChipAY_3_8910::PARAM_NOISE  + i));
            addInput(createInput<PJ301MPort>(    Vec(175,  65 + i * 111), module, ChipAY_3_8910::INPUT_NOISE     + i));
            addInput(createInput<PJ301MPort>(    Vec(182, 35  + i * 111),  module, ChipAY_3_8910::INPUT_LEVEL    + i));
            addParam(createParam<BefacoSlidePot>(Vec(211, 21  + i * 111),  module, ChipAY_3_8910::PARAM_LEVEL    + i));
            addOutput(createOutput<PJ301MPort>(  Vec(180, 100 + i * 111), module, ChipAY_3_8910::OUTPUT_CHANNEL  + i));
        }
    }
};

/// the global instance of the model
Model *modelChipAY_3_8910 = createModel<ChipAY_3_8910, ChipAY_3_8910Widget>("AY_3_8910");
