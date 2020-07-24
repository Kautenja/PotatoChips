// A Atari POKEY Chip module.
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
#include "dsp/atari_pokey.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Atari POKEY Chip module.
struct ChipPOKEY : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, AtariPOKEY::OSC_COUNT),
        ENUMS(PARAM_NOISE, AtariPOKEY::OSC_COUNT),
        ENUMS(PARAM_LEVEL, AtariPOKEY::OSC_COUNT),
        ENUMS(PARAM_CONTROL, 8),  // 1 button per bit (control flag)
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, AtariPOKEY::OSC_COUNT),
        ENUMS(INPUT_FM, AtariPOKEY::OSC_COUNT),
        ENUMS(INPUT_NOISE, AtariPOKEY::OSC_COUNT),
        ENUMS(INPUT_LEVEL, AtariPOKEY::OSC_COUNT),
        ENUMS(INPUT_CONTROL, 8),  // 1 input per bit (control flag)
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, AtariPOKEY::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds {
        ENUMS(LIGHTS_LEVEL, AtariPOKEY::OSC_COUNT),
        LIGHT_COUNT
    };

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[AtariPOKEY::OSC_COUNT];
    /// The POKEY instance to synthesize sound with
    AtariPOKEY apu;

    /// triggers for handling inputs to the control ports
    dsp::BooleanTrigger controlTriggers[8];

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

    /// a VU meter for keeping track of the channel levels
    dsp::VuMeter2 chMeters[AtariPOKEY::OSC_COUNT];
    /// a clock divider for updating the mixer LEDs
    dsp::ClockDivider lightDivider;

    /// Initialize a new POKEY Chip module.
    ChipPOKEY() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ + 0, -30.f, 30.f, 0.f, "Channel 1 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -30.f, 30.f, 0.f, "Channel 2 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2, -30.f, 30.f, 0.f, "Channel 3 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 3, -30.f, 30.f, 0.f, "Channel 4 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_NOISE + 0, 0, 7, 7, "Channel 1 Noise");
        configParam(PARAM_NOISE + 1, 0, 7, 7, "Channel 2 Noise");
        configParam(PARAM_NOISE + 2, 0, 7, 7, "Channel 3 Noise");
        configParam(PARAM_NOISE + 3, 0, 7, 7, "Channel 4 Noise");
        configParam(PARAM_LEVEL + 0, 0, 1, 0.5, "Channel 1 Level", "%", 0, 100);
        configParam(PARAM_LEVEL + 1, 0, 1, 0.5, "Channel 2 Level", "%", 0, 100);
        configParam(PARAM_LEVEL + 2, 0, 1, 0.5, "Channel 3 Level", "%", 0, 100);
        configParam(PARAM_LEVEL + 3, 0, 1, 0.5, "Channel 4 Level", "%", 0, 100);
        configParam(PARAM_CONTROL + 0, 0, 1, 0, "Frequency Division", "");
        configParam(PARAM_CONTROL + 1, 0, 1, 0, "High-Pass Channel 2 from 3", "");
        configParam(PARAM_CONTROL + 2, 0, 1, 0, "High-Pass Channel 1 from 3", "");
        // configParam(PARAM_CONTROL + 3, 0, 1, 0, "16-bit 4 + 3", "");
        // configParam(PARAM_CONTROL + 4, 0, 1, 0, "16-bit 1 + 2", "");
        configParam(PARAM_CONTROL + 5, 0, 1, 0, "Ch. 3 Base Frequency", "");
        configParam(PARAM_CONTROL + 6, 0, 1, 0, "Ch. 1 Base Frequency", "");
        configParam(PARAM_CONTROL + 7, 0, 1, 0, "LFSR", "");
        cvDivider.setDivision(16);
        lightDivider.setDivision(512);
        // set the output buffer for each individual voice
        for (unsigned i = 0; i < AtariPOKEY::OSC_COUNT; i++)
            apu.set_output(i, &buf[i]);
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.set_volume(3.f);
        onSampleRateChange();
    }

    // fix to return a better value when ch1 or ch3 freq is turned on
    /// Return the frequency for the given channel.
    ///
    /// @param channel the channel to return the frequency for
    /// @returns the 8-bit frequency value from parameters and CV inputs
    ///
    inline uint8_t getFrequency(int channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ8BIT_MIN = 2;
        // the maximal value for the frequency register
        static constexpr float FREQ8BIT_MAX = 0xFF;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 56;
        // the constant modulation factor
        static constexpr auto MOD_FACTOR = 10.f;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + channel].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + channel].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM + channel].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // calculate the frequency based on the clock division
        freq = (buf[channel].get_clock_rate() / (CLOCK_DIVISION * freq)) - 1;
        return rack::clamp(freq, FREQ8BIT_MIN, FREQ8BIT_MAX);
    }

    /// Return the noise for the given channel.
    ///
    /// @param channel the channel to return the noise for
    /// @returns the 3-bit noise value from parameters and CV inputs
    ///
    inline uint8_t getNoise(int channel) {
        // the minimal value for the noise register
        static constexpr float NOISE_MIN = 0;
        // the maximal value for the noise register
        static constexpr float NOISE_MAX = 7;
        // get the noise from the parameter knob
        auto noiseParam = params[PARAM_NOISE + channel].getValue();
        // apply the control voltage to the level
        if (inputs[INPUT_NOISE + channel].isConnected()) {
            auto cv = inputs[INPUT_NOISE + channel].getVoltage() / 10.f;
            cv = 1.f - rack::clamp(cv, 0.f, 1.f);
            cv = roundf(100.f * cv) / 100.f;
            noiseParam *= 2 * cv;
        }
        return rack::clamp(noiseParam, NOISE_MIN, NOISE_MAX);
    }

    /// Return the level for the given channel.
    ///
    /// @param channel the channel to return the level for
    /// @returns the 4-bit level value from parameters and CV inputs
    ///
    inline uint8_t getLevel(int channel) {
        // the minimal value for the volume register
        static constexpr float ATT_MIN = 0;
        // the maximal value for the volume register
        static constexpr float ATT_MAX = 15;
        // get the level from the parameter knob
        auto levelParam = params[PARAM_LEVEL + channel].getValue();
        // apply the control voltage to the level
        if (inputs[INPUT_LEVEL + channel].isConnected()) {
            auto cv = inputs[INPUT_LEVEL + channel].getVoltage();
            cv = rack::clamp(cv / 10.f, 0.f, 1.f);
            cv = roundf(100.f * cv) / 100.f;
            levelParam *= 2 * cv;
        }
        return rack::clamp(ATT_MAX * levelParam, ATT_MIN, ATT_MAX);
    }

    /// Return the control byte.
    ///
    /// @returns the 8-bit control byte from parameters and CV inputs
    ///
    inline uint8_t getControl() {
        uint8_t controlByte = 0;
        for (std::size_t bit = 0; bit < 8; bit++) {
            auto cv = inputs[INPUT_CONTROL + bit].getVoltage();
            controlTriggers[bit].process(rescale(cv, 0.f, 2.f, 0.f, 1.f));
            bool state = (1 - params[PARAM_CONTROL + bit].getValue()) - !controlTriggers[bit].state;
            // the position for the current button's index
            controlByte |= state << bit;
        }
        return controlByte;
    }

    /// Return a 10V signed sample from the APU.
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
            for (unsigned i = 0; i < AtariPOKEY::OSC_COUNT; i++) {
                // there are 2 registers per channel, multiply first channel
                // by 2 to produce an offset between registers based on channel
                // index. the 3 noise bit occupy the MSB of the control register
                apu.write(AtariPOKEY::AUDF1 + AtariPOKEY::REGS_PER_VOICE * i, getFrequency(i));
                apu.write(AtariPOKEY::AUDC1 + AtariPOKEY::REGS_PER_VOICE * i, (getNoise(i) << 5) | getLevel(i));
            }
            // write the control byte to the chip
            apu.write(AtariPOKEY::AUDCTL, getControl());
        }
        // process audio samples on the chip engine
        apu.end_frame(CLOCK_RATE / args.sampleRate);
        for (unsigned i = 0; i < AtariPOKEY::OSC_COUNT; i++) {  // set outputs
            auto channelOutput = getAudioOut(i);
            chMeters[i].process(args.sampleTime, channelOutput / 5.f);
            outputs[OUTPUT_CHANNEL + i].setVoltage(channelOutput);
        }
        if (lightDivider.process()) {  // update the mixer lights
            for (unsigned i = 0; i < AtariPOKEY::OSC_COUNT; i++) {
                float b = chMeters[i].getBrightness(-24.f, 0.f);
                lights[LIGHTS_LEVEL + i].setBrightness(b);
            }
        }
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override {
        // update the sample rate for each channel
        for (unsigned i = 0; i < AtariPOKEY::OSC_COUNT; i++)
            buf[i].set_sample_rate(APP->engine->getSampleRate(), CLOCK_RATE);
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The widget structure that lays out the panel of the module and the UI menus.
struct ChipPOKEYWidget : ModuleWidget {
    ChipPOKEYWidget(ChipPOKEY *module) {
        setModule(module);
        static constexpr auto panel = "res/POKEY.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // the vertical spacing between the same component on different channels
        static constexpr float VERT_SEP = 85.f;
        // channel control
        for (unsigned i = 0; i < AtariPOKEY::OSC_COUNT; i++) {
            addInput(createInput<PJ301MPort>(  Vec(19,  73 + i * VERT_SEP), module, ChipPOKEY::INPUT_VOCT + i));
            addInput(createInput<PJ301MPort>(  Vec(19,  38 + i * VERT_SEP), module, ChipPOKEY::INPUT_FM + i));
            addParam(createParam<Rogan5PSGray>(Vec(46,  39 + i * VERT_SEP), module, ChipPOKEY::PARAM_FREQ + i));
            auto noise = createParam<Rogan1PRed>(  Vec(109, 30 + i * VERT_SEP), module, ChipPOKEY::PARAM_NOISE + i);
            noise->snap = true;
            addParam(noise);
            addInput(createInput<PJ301MPort>(  Vec(116, 71 + i * VERT_SEP), module, ChipPOKEY::INPUT_NOISE + i));
            addParam(createLightParam<LEDLightSlider<GreenLight>>(Vec(144, 24 + i * VERT_SEP),  module, ChipPOKEY::PARAM_LEVEL + i, ChipPOKEY::LIGHTS_LEVEL + i));
            addInput(createInput<PJ301MPort>(  Vec(172, 28 + i * VERT_SEP), module, ChipPOKEY::INPUT_LEVEL + i));
            addOutput(createOutput<PJ301MPort>(Vec(175, 74 + i * VERT_SEP), module, ChipPOKEY::OUTPUT_CHANNEL + i));
        }
        // global control
        for (int i = 0; i < 8; i++) {
            if (i == 3 or i == 4) continue;
            addParam(createParam<CKSS>(Vec(213, 33 + i * (VERT_SEP / 2)), module, ChipPOKEY::PARAM_CONTROL + i));
            addInput(createInput<PJ301MPort>(Vec(236, 32 + i * (VERT_SEP / 2)), module, ChipPOKEY::INPUT_CONTROL + i));
        }
    }
};

/// the global instance of the model
Model *modelChipPOKEY = createModel<ChipPOKEY, ChipPOKEYWidget>("POKEY");
