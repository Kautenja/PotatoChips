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

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[GeneralInstrumentAy_3_8910::OSC_COUNT];
    /// The General Instrument AY-3-8910 instance to synthesize sound with
    GeneralInstrumentAy_3_8910 apu;

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

    /// triggers for handling inputs to the tone and noise enable switches
    dsp::BooleanTrigger mixerTriggers[2 * GeneralInstrumentAy_3_8910::OSC_COUNT];

    /// Initialize a new FME7 Chip module.
    ChipAY_3_8910() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        cvDivider.setDivision(16);
        for (unsigned i = 0; i < GeneralInstrumentAy_3_8910::OSC_COUNT; i++) {
            // get the channel name starting with ACII code 65 (A)
            auto channel_name = std::string(1, static_cast<char>(65 + i));
            configParam(PARAM_FREQ  + i, -60.f, 60.f, 0.f,  "Pulse " + channel_name + " Frequency",     " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
            configParam(PARAM_LEVEL + i,  0.f,   1.f, 0.9f, "Pulse " + channel_name + " Level",         "%",   0.f,                100.f       );
            configParam(PARAM_TONE  + i,  0,     1,   1,    "Pulse " + channel_name + " Tone Enabled",  "");
            configParam(PARAM_NOISE + i,  0,     1,   0,    "Pulse " + channel_name + " Noise Enabled", "");
            apu.set_output(i, &buf[i]);
        }
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.set_volume(3.f);
        onSampleRateChange();
    }

    /// Return the frequency for the given channel.
    ///
    /// @param channel the index of the channel to get the frequency of
    /// @returns the 12-bit frequency in a 16-bit container
    ///
    inline uint16_t getFrequency(int channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ12BIT_MIN = 2;
        // the maximal value for the frequency register
        static constexpr float FREQ12BIT_MAX = 4095;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + channel].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + channel].getVoltage();
        pitch += inputs[INPUT_FM + channel].getVoltage() / 5.f;
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
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
    /// Returns a frequency based on the knob for channel 3 (index 2)
    ///
    inline uint8_t getNoise() {
        // use the frequency control knob from channel index 2
        static constexpr unsigned channel = 2;
        // the minimal value for the noise frequency register to produce sound
        static constexpr float FREQ_MIN = 0;
        // the maximal value for the noise frequency register
        static constexpr float FREQ_MAX = 31;
        // get the parameter value from the UI knob. the knob represents
        // frequency, so translate to a simple [0, 1] scale.
        auto param = (0.5f + params[PARAM_FREQ + channel].getValue() / 120.f);
        // 5V scale for V/OCT input
        param += inputs[INPUT_VOCT + channel].getVoltage() / 5.f;
        // 10V scale for mod input
        param += inputs[INPUT_FM + channel].getVoltage() / 10.f;
        // clamp the parameter within its legal limits
        param = rack::clamp(param, 0.f, 1.f);
        // get the 5-bit noise period clamped within legal limits. invert the
        // value about the maximal to match directionality of channel frequency
        return FREQ_MAX - rack::clamp(FREQ_MAX * param, FREQ_MIN, FREQ_MAX);
    }

    /// Return the mixer byte.
    ///
    /// @returns the 8-bit mixer byte from parameters and CV inputs
    ///
    inline uint8_t getMixer() {
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
            auto cv = math::clamp(inputs[INPUT_TONE + i].getVoltage(), 0.f, 10.f);
            mixerTriggers[i].process(rescale(cv, 0.f, 2.f, 0.f, 1.f));
            // get the state of the tone based on the parameter and trig input
            bool toneState = params[PARAM_TONE + i].getValue() - mixerTriggers[i].state;
            // invert the state to indicate "OFF" semantics instead of "ON"
            mixerByte |= !toneState << i;
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
        if (cvDivider.process()) {  // process the CV inputs to the chip
            for (unsigned i = 0; i < GeneralInstrumentAy_3_8910::OSC_COUNT; i++) {
                // 2 frequency registers per voice, shift over by 1 instead of
                // multiplying
                auto offset = i << 1;
                auto freq = getFrequency(i);
                auto lo =  freq & 0b0000000011111111;
                apu.write(GeneralInstrumentAy_3_8910::PERIOD_CH_A_LO + offset, lo);
                auto hi = (freq & 0b0000111100000000) >> 8;
                apu.write(GeneralInstrumentAy_3_8910::PERIOD_CH_A_HI + offset, hi);
                // volume
                auto level = getLevel(i);
                apu.write(GeneralInstrumentAy_3_8910::VOLUME_CH_A + i, level);
            }
            // set the 5-bit noise value based on the channel 3 parameter
            apu.write(GeneralInstrumentAy_3_8910::NOISE_PERIOD, getNoise());
            // set the 6-channel boolean mixer (tone and noise for each channel)
            apu.write(GeneralInstrumentAy_3_8910::CHANNEL_ENABLES, getMixer());
            // envelope period (TODO: fix envelope in engine)
            // apu.write(GeneralInstrumentAy_3_8910::PERIOD_ENVELOPE_LO, 0b10101011);
            // apu.write(GeneralInstrumentAy_3_8910::PERIOD_ENVELOPE_HI, 0b00000011);
            // envelope shape bits (TODO: fix envelope in engine)
            // apu.write(
            //     GeneralInstrumentAy_3_8910::ENVELOPE_SHAPE,
            //     GeneralInstrumentAy_3_8910::ENVELOPE_SHAPE_NONE
            // );
        }
        // process audio samples on the chip engine
        apu.end_frame(CLOCK_RATE / args.sampleRate);
        for (unsigned i = 0; i < GeneralInstrumentAy_3_8910::OSC_COUNT; i++)
            outputs[OUTPUT_CHANNEL + i].setVoltage(getAudioOut(i));
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override {
        // update the buffer for each channel
        for (unsigned i = 0; i < GeneralInstrumentAy_3_8910::OSC_COUNT; i++)
            buf[i].set_sample_rate(APP->engine->getSampleRate(), CLOCK_RATE);
    }
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
        for (unsigned i = 0; i < GeneralInstrumentAy_3_8910::OSC_COUNT; i++) {
            addInput(createInput<PJ301MPort>(    Vec(18,  27  + i * 111), module, ChipAY_3_8910::INPUT_FM       + i));
            addInput(createInput<PJ301MPort>(    Vec(18,  100 + i * 111), module, ChipAY_3_8910::INPUT_VOCT     + i));
            addParam(createParam<Rogan6PSWhite>( Vec(47,  29  + i * 111), module, ChipAY_3_8910::PARAM_FREQ     + i));
            addParam(createParam<CKSS>(          Vec(144, 29  + i * 111), module, ChipAY_3_8910::PARAM_TONE     + i));
            addInput(createInput<PJ301MPort>(    Vec(147, 53  + i * 111), module, ChipAY_3_8910::INPUT_TONE     + i));
            addParam(createParam<CKSS>(          Vec(138, 105 + i * 111), module, ChipAY_3_8910::PARAM_NOISE    + i));
            addInput(createInput<PJ301MPort>(    Vec(175, 65  + i * 111), module, ChipAY_3_8910::INPUT_NOISE    + i));
            addInput(createInput<PJ301MPort>(    Vec(182, 35  + i * 111), module, ChipAY_3_8910::INPUT_LEVEL    + i));
            addParam(createParam<BefacoSlidePot>(Vec(211, 21  + i * 111), module, ChipAY_3_8910::PARAM_LEVEL    + i));
            addOutput(createOutput<PJ301MPort>(  Vec(180, 100 + i * 111), module, ChipAY_3_8910::OUTPUT_CHANNEL + i));
        }
    }
};

/// the global instance of the model
Model *modelChipAY_3_8910 = createModel<ChipAY_3_8910, ChipAY_3_8910Widget>("AY_3_8910");
