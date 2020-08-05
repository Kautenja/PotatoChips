// A Konami VRC6 Chip module.
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
#include "dsp/konami_vrc6.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Konami VRC6 Chip module.
struct ChipVRC6 : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, KonamiVRC6::OSC_COUNT),
        ENUMS(PARAM_PW, KonamiVRC6::OSC_COUNT - 1),  // pulse wave only
        ENUMS(PARAM_LEVEL, KonamiVRC6::OSC_COUNT),
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, KonamiVRC6::OSC_COUNT),
        ENUMS(INPUT_FM, KonamiVRC6::OSC_COUNT),
        ENUMS(INPUT_PW, KonamiVRC6::OSC_COUNT - 1),  // pulse wave only
        ENUMS(INPUT_LEVEL, KonamiVRC6::OSC_COUNT),
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, KonamiVRC6::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds {
        LIGHT_COUNT
    };

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[KonamiVRC6::OSC_COUNT];
    /// The VRC6 instance to synthesize sound with
    KonamiVRC6 apu;

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

    /// Initialize a new VRC6 Chip module.
    ChipVRC6() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ + 0, -30.f, 30.f, 0.f,   "Pulse 1 Frequency",        " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -30.f, 30.f, 0.f,   "Pulse 2 Frequency",        " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2, -30.f, 30.f, 0.f,   "Saw Frequency",            " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_PW + 0,     0,    7,   4,     "Pulse 1 Duty Cycle"                                               );
        configParam(PARAM_PW + 1,     0,    7,   4,     "Pulse 1 Duty Cycle"                                               );
        configParam(PARAM_LEVEL + 0,  0.f,  1.f, 0.8f,  "Pulse 1 Level",            "%",   0.f,                100.f       );
        configParam(PARAM_LEVEL + 1,  0.f,  1.f, 0.8f,  "Pulse 2 Level",            "%",   0.f,                100.f       );
        configParam(PARAM_LEVEL + 2,  0.f,  1.f, 0.5f, "Saw Level / Quantization", "%",   0.f,                100.f       );
        cvDivider.setDivision(16);
        // set the output buffer for each individual voice
        for (unsigned i = 0; i < KonamiVRC6::OSC_COUNT; i++)
            apu.set_output(i, &buf[i]);
        // global volume of 3 produces a roughly 5Vpp signal from all voices
        apu.set_volume(3.f);
        onSampleRateChange();
    }

    /// Get the frequency for the given channel
    ///
    /// @param freq_min the minimal value for the frequency register to
    /// produce sound
    /// @param freq_max the maximal value for the frequency register
    /// @param clock_division the clock division of the oscillator relative
    /// to the CPU
    /// @returns the 12 bit frequency value from the panel
    /// @details
    /// parameters for pulse wave:
    /// freq_min = 4, freq_max = 4095, clock_division = 16
    /// parameters for triangle wave:
    /// freq_min = 3, freq_max = 4095, clock_division = 14
    ///
    inline uint16_t getFrequency(
        unsigned channel,
        float freq_min,
        float freq_max,
        float clock_division
    ) {
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + channel].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + channel].getVoltage();
        pitch += inputs[INPUT_FM + channel].getVoltage() / 5.f;
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to an 11-bit value
        freq = (buf[channel].get_clock_rate() / (clock_division * freq)) - 1;
        return rack::clamp(freq, freq_min, freq_max);
    }

    /// Return the pulse width parameter for the given channel.
    ///
    /// @param channel the channel to return the pulse width value for
    /// @returns the pulse width value in an 8-bit container in the high 4 bits.
    /// if channel == 2, i.e., saw channel, returns 0 (no PW for saw wave)
    ///
    inline uint8_t getPW(unsigned channel) {
        // the minimal value for the pulse width register
        static constexpr float PW_MIN = 0;
        // the maximal value for the pulse width register (before shift)
        static constexpr float PW_MAX = 0b00000111;
        if (channel == KonamiVRC6::SAW) return 0;  // no PW for saw wave
        // get the pulse width from the parameter knob
        auto pwParam = params[PARAM_PW + channel].getValue();
        // get the control voltage to the pulse width with 1V/step
        auto pwCV = inputs[INPUT_PW + channel].getVoltage() / 2.f;
        // get the 8-bit pulse width clamped within legal limits
        uint8_t pw = rack::clamp(pwParam + pwCV, PW_MIN, PW_MAX);
        // shift the pulse width over into the high 4 bits
        return pw << 4;
    }

    /// Return the level parameter for the given channel.
    ///
    /// @param channel the channel to return the pulse width value for
    /// @returns the level value in an 8-bit container in the low 4 bits
    ///
    inline uint8_t getLevel(unsigned channel, float max_level) {
        // get the level from the parameter knob
        auto levelParam = params[PARAM_LEVEL + channel].getValue();
        // apply the control voltage to the level
        auto levelCV = inputs[INPUT_LEVEL + channel].getVoltage() / 2.f;
        // get the 8-bit level clamped within legal limits
        return rack::clamp(max_level * (levelParam + levelCV), 0.f, max_level);
    }

    /// Return a 10V signed sample from the APU.
    ///
    /// @param channel the channel to get the audio sample for
    ///
    inline float getAudioOut(unsigned channel) {
        // the peak to peak output of the voltage
        static constexpr float Vpp = 10.f;
        // the amount of voltage per increment of 16-bit fidelity volume
        static constexpr float divisor = std::numeric_limits<int16_t>::max();
        // convert the 16-bit sample to 10Vpp floating point
        return Vpp * buf[channel].read_sample() / divisor;
    }

    /// Process a sample.
    void process(const ProcessArgs &args) override {
        static constexpr float freq_low[KonamiVRC6::OSC_COUNT] =       { 4,  4,  3};
        static constexpr float clock_division[KonamiVRC6::OSC_COUNT] = {16, 16, 14};
        static constexpr float max_level[KonamiVRC6::OSC_COUNT] =      {15, 15, 63};
        if (cvDivider.process()) {  // process the CV inputs to the chip
            for (unsigned i = 0; i < KonamiVRC6::OSC_COUNT; i++) {
                // frequency (max frequency is same for pulses and saw, 4095)
                uint16_t freq = getFrequency(i, freq_low[i], 4095, clock_division[i]);
                uint8_t lo =  freq & 0b0000000011111111;
                uint8_t hi = (freq & 0b0000111100000000) >> 8;
                hi |= KonamiVRC6::PERIOD_HIGH_ENABLED;  // enable the channel
                apu.write(KonamiVRC6::PULSE0_PERIOD_LOW + KonamiVRC6::REGS_PER_OSC * i, lo);
                apu.write(KonamiVRC6::PULSE0_PERIOD_HIGH + KonamiVRC6::REGS_PER_OSC * i, hi);
                // level
                uint8_t level = getPW(i) | getLevel(i, max_level[i]);
                apu.write(KonamiVRC6::PULSE0_DUTY_VOLUME + KonamiVRC6::REGS_PER_OSC * i, level);
            }
        }
        // process audio samples on the chip engine
        apu.end_frame(CLOCK_RATE / args.sampleRate);
        for (unsigned i = 0; i < KonamiVRC6::OSC_COUNT; i++)
            outputs[OUTPUT_CHANNEL + i].setVoltage(getAudioOut(i));
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override {
        // update the buffer for each channel
        for (unsigned i = 0; i < KonamiVRC6::OSC_COUNT; i++)
            buf[i].set_sample_rate(APP->engine->getSampleRate(), CLOCK_RATE);
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The widget structure that lays out the panel of the module and the UI menus.
struct ChipVRC6Widget : ModuleWidget {
    ChipVRC6Widget(ChipVRC6 *module) {
        setModule(module);
        static constexpr auto panel = "res/VRC6.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        for (unsigned i = 0; i < KonamiVRC6::OSC_COUNT - 1; i++) {
            addInput(createInput<PJ301MPort>(    Vec(18,  69  + i * 111), module, ChipVRC6::INPUT_VOCT     + i));
            addInput(createInput<PJ301MPort>(    Vec(18,  34  + i * 111), module, ChipVRC6::INPUT_FM       + i));
            addParam(createParam<Rogan6PSWhite>( Vec(47,  29  + i * 111), module, ChipVRC6::PARAM_FREQ     + i));
            auto pw = createParam<RoundSmallBlackKnob>(Vec(146, 35 + i * 111), module, ChipVRC6::PARAM_PW + i);
            pw->snap = true;
            addParam(pw);
            addInput(createInput<PJ301MPort>(Vec(145, 70 + i * 111), module, ChipVRC6::INPUT_PW + i));
            addInput(createInput<PJ301MPort>(    Vec(18, 104  + i * 111), module, ChipVRC6::INPUT_LEVEL    + i));
            addParam(createParam<BefacoSlidePot>(Vec(180, 21  + i * 111), module, ChipVRC6::PARAM_LEVEL    + i));
            addOutput(createOutput<PJ301MPort>(  Vec(150, 100 + i * 111), module, ChipVRC6::OUTPUT_CHANNEL + i));
        }
        int i = 2;
        addInput(createInput<PJ301MPort>(    Vec(18,  322), module, ChipVRC6::INPUT_VOCT     + i));
        addInput(createInput<PJ301MPort>(    Vec(18,  249), module, ChipVRC6::INPUT_FM       + i));
        addParam(createParam<Rogan6PSWhite>( Vec(47,  29  + i * 111), module, ChipVRC6::PARAM_FREQ     + i));
        addInput(createInput<PJ301MPort>(    Vec(152, 257), module, ChipVRC6::INPUT_LEVEL    + i));
        addParam(createParam<BefacoSlidePot>(Vec(180, 21  + i * 111), module, ChipVRC6::PARAM_LEVEL    + i));
        addOutput(createOutput<PJ301MPort>(  Vec(150, 100 + i * 111), module, ChipVRC6::OUTPUT_CHANNEL + i));
    }
};

/// the global instance of the model
Model *modelChipVRC6 = createModel<ChipVRC6, ChipVRC6Widget>("VRC6");
