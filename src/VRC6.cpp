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
        ENUMS(PARAM_PW, 2),
        ENUMS(PARAM_LEVEL, KonamiVRC6::OSC_COUNT),
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, KonamiVRC6::OSC_COUNT),
        ENUMS(INPUT_FM, KonamiVRC6::OSC_COUNT),
        ENUMS(INPUT_PW, 2),
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
        configParam(PARAM_LEVEL + 0,  0.f,  1.f, 0.5f,  "Pulse 1 Level",            "%",   0.f,                100.f       );
        configParam(PARAM_LEVEL + 1,  0.f,  1.f, 0.5f,  "Pulse 2 Level",            "%",   0.f,                100.f       );
        configParam(PARAM_LEVEL + 2,  0.f,  1.f, 0.25f, "Saw Level / Quantization", "%",   0.f,                100.f       );
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
        int channel,
        float freq_min,
        float freq_max,
        float clock_division
    ) {
        // the constant modulation factor
        static constexpr auto MOD_FACTOR = 10.f;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + channel].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + channel].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM + channel].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to an 11-bit value
        freq = (buf[channel].get_clock_rate() / (clock_division * freq)) - 1;
        return rack::clamp(freq, freq_min, freq_max);
    }

    /// Process square wave for given channel.
    ///
    /// @param channel the pulse channel to process data for
    ///
    void channel_pulse(int channel) {
        // the minimal value for the pulse width register
        static constexpr float PW_MIN = 0;
        // the maximal value for the pulse width register (0b00000111)
        static constexpr float PW_MAX = 7;
        // the minimal value for the volume width register
        static constexpr float LEVEL_MIN = 0;
        // the maximal value for the volume width register (0b00001111)
        static constexpr float LEVEL_MAX = 15;

        uint16_t freq = getFrequency(channel, 4, 4095, 16);
        uint8_t lo =  freq & 0b0000000011111111;
        uint8_t hi = (freq & 0b0000111100000000) >> 8;
        // enable the channel
        hi |= 0b10000000;
        // write the register for the frequency
        apu.write(KonamiVRC6::PULSE0_PERIOD_LOW + KonamiVRC6::REGS_PER_OSC * channel, lo);
        apu.write(KonamiVRC6::PULSE0_PERIOD_HIGH + KonamiVRC6::REGS_PER_OSC * channel, hi);

        // get the pulse width from the parameter knob
        auto pwParam = params[PARAM_PW + channel].getValue();
        // get the control voltage to the pulse width with 1V/step
        auto pwCV = inputs[INPUT_PW + channel].getVoltage() / 2.f;
        // get the 8-bit pulse width clamped within legal limits
        uint8_t pw = rack::clamp(pwParam + pwCV, PW_MIN, PW_MAX);
        // get the level from the parameter knob
        auto levelParam = params[PARAM_LEVEL + channel].getValue();
        // apply the control voltage to the level
        auto levelCV = inputs[INPUT_LEVEL + channel].getVoltage() / 2.f;
        // get the 8-bit level clamped within legal limits
        uint8_t level = rack::clamp(LEVEL_MAX * (levelParam + levelCV), LEVEL_MIN, LEVEL_MAX);
        // write the register for the duty cycle and volume
        apu.write(KonamiVRC6::PULSE0_DUTY_VOLUME + KonamiVRC6::REGS_PER_OSC * channel, (pw << 4) + level);
    }

    /// Process saw wave (channel 2).
    void channel_saw() {
        // the minimal value for the volume width register
        static constexpr float LEVEL_MIN = 0;
        // the maximal value for the volume width register (0b00111111)
        // the actual max for volume is 42, but some distortion effects are
        // available on the chip from 42 to 63 (as a result of overflow)
        static constexpr float LEVEL_MAX = 63;

        uint16_t freq = getFrequency(2, 3, 4095, 14);
        // convert the frequency to a 12-bit value spanning two 8-bit registers
        uint8_t lo =  freq & 0b0000000011111111;
        uint8_t hi = (freq & 0b0000111100000000) >> 8;
        // enable the channel
        hi |= 0b10000000;
        // write the register for the frequency
        apu.write(KonamiVRC6::SAW_PERIOD_LOW, lo);
        apu.write(KonamiVRC6::SAW_PERIOD_HIGH, hi);

        // get the level from the parameter knob
        auto levelParam = params[PARAM_LEVEL + 2].getValue();
        // apply the control voltage to the level
        auto levelCV = inputs[INPUT_LEVEL + 2].getVoltage() / 2.f;
        // get the 8-bit level clamped within legal limits
        uint8_t level = rack::clamp(LEVEL_MAX * (levelParam + levelCV), LEVEL_MIN, LEVEL_MAX);
        // write the register for the volume
        apu.write(KonamiVRC6::SAW_VOLUME, level);
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
        if (cvDivider.process()) {  // process the CV inputs to the chip
            channel_pulse(0);
            channel_pulse(1);
            channel_saw();
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
        // V/OCT inputs
        addInput(createInput<PJ301MPort>(Vec(20, 78), module, ChipVRC6::INPUT_VOCT + 0));
        addInput(createInput<PJ301MPort>(Vec(20, 188), module, ChipVRC6::INPUT_VOCT + 1));
        addInput(createInput<PJ301MPort>(Vec(20, 298), module, ChipVRC6::INPUT_VOCT + 2));
        // FM inputs
        addInput(createInput<PJ301MPort>(Vec(26, 37), module, ChipVRC6::INPUT_FM + 0));
        addInput(createInput<PJ301MPort>(Vec(26, 149), module, ChipVRC6::INPUT_FM + 1));
        addInput(createInput<PJ301MPort>(Vec(26, 258), module, ChipVRC6::INPUT_FM + 2));
        // PW inputs
        { auto param = createParam<Rogan0PSNES>(Vec(30, 107), module, ChipVRC6::PARAM_PW + 0);
        param->snap = true;
        addParam(param); }
        { auto param = createParam<Rogan0PSNES>(Vec(30, 218), module, ChipVRC6::PARAM_PW + 1);
        param->snap = true;
        addParam(param); }
        addInput(createInput<PJ301MPort>(Vec(58, 104), module, ChipVRC6::INPUT_PW + 0));
        addInput(createInput<PJ301MPort>(Vec(58, 215), module, ChipVRC6::INPUT_PW + 1));
        // Frequency parameters
        addParam(createParam<Rogan3PSNES>(Vec(54, 42), module, ChipVRC6::PARAM_FREQ + 0));
        addParam(createParam<Rogan3PSNES>(Vec(54, 151), module, ChipVRC6::PARAM_FREQ + 1));
        addParam(createParam<Rogan3PSNES>(Vec(54, 266), module, ChipVRC6::PARAM_FREQ + 2));
        // Levels
        addInput(createInput<PJ301MPort>(Vec(102, 36), module, ChipVRC6::INPUT_LEVEL + 0));
        addInput(createInput<PJ301MPort>(Vec(102, 146), module, ChipVRC6::INPUT_LEVEL + 1));
        addInput(createInput<PJ301MPort>(Vec(102, 255), module, ChipVRC6::INPUT_LEVEL + 2));
        addParam(createParam<Rogan0PSNES>(Vec(103, 64), module, ChipVRC6::PARAM_LEVEL + 0));
        addParam(createParam<Rogan0PSNES>(Vec(103, 174), module, ChipVRC6::PARAM_LEVEL + 1));
        addParam(createParam<Rogan0PSNES>(Vec(103, 283), module, ChipVRC6::PARAM_LEVEL + 2));
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(107, 104), module, ChipVRC6::OUTPUT_CHANNEL + 0));
        addOutput(createOutput<PJ301MPort>(Vec(107, 214), module, ChipVRC6::OUTPUT_CHANNEL + 1));
        addOutput(createOutput<PJ301MPort>(Vec(107, 324), module, ChipVRC6::OUTPUT_CHANNEL + 2));
    }
};

/// the global instance of the model
Model *modelChipVRC6 = createModel<ChipVRC6, ChipVRC6Widget>("VRC6");
