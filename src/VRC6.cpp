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
#include "dsp/konami_vrc6_apu.hpp"

/// the IO registers on the VRC6 chip (altered for VRC6 implementation).
enum IORegisters {
    // pulse wave 1 registers
    PULSE0_DUTY_VOLUME = 0,
    PULSE0_PERIOD_LOW  = 1,
    PULSE0_PERIOD_HIGH = 2,
    // pulse wave 2 registers
    PULSE1_DUTY_VOLUME = 0,
    PULSE1_PERIOD_LOW  = 1,
    PULSE1_PERIOD_HIGH = 2,
    // saw wave registers
    SAW_VOLUME         = 0,
    SAW_PERIOD_LOW     = 1,
    SAW_PERIOD_HIGH    = 2,
};

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Konami VRC6 Chip module.
struct ChipVRC6 : Module {
    enum ParamIds {
        PARAM_FREQ0,
        PARAM_FREQ1,
        PARAM_FREQ2,
        PARAM_PW0,
        PARAM_PW1,
        PARAM_LEVEL0,
        PARAM_LEVEL1,
        PARAM_LEVEL2,
        PARAM_COUNT
    };
    enum InputIds {
        INPUT_VOCT0,
        INPUT_VOCT1,
        INPUT_VOCT2,
        INPUT_FM0,
        INPUT_FM1,
        INPUT_FM2,
        INPUT_PW0,
        INPUT_PW1,
        INPUT_LEVEL0,
        INPUT_LEVEL1,
        INPUT_LEVEL2,
        INPUT_COUNT
    };
    enum OutputIds {
        OUTPUT_CHANNEL0,
        OUTPUT_CHANNEL1,
        OUTPUT_CHANNEL2,
        OUTPUT_COUNT
    };
    enum LightIds {
        LIGHT_COUNT
    };

    /// the clock rate of the module
    static constexpr uint64_t CLOCK_RATE = 768000;

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[VRC6::OSC_COUNT];
    /// The VRC6 instance to synthesize sound with
    VRC6 apu;

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

    /// Initialize a new VRC6 Chip module.
    ChipVRC6() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ0, -30.f, 30.f, 0.f,   "Pulse 1 Frequency",        " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ1, -30.f, 30.f, 0.f,   "Pulse 2 Frequency",        " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ2, -30.f, 30.f, 0.f,   "Saw Frequency",            " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_PW0,     0,    7,   4,     "Pulse 1 Duty Cycle"                                               );
        configParam(PARAM_PW1,     0,    7,   4,     "Pulse 1 Duty Cycle"                                               );
        configParam(PARAM_LEVEL0,  0.f,  1.f, 0.5f,  "Pulse 1 Level",            "%",   0.f,                100.f       );
        configParam(PARAM_LEVEL1,  0.f,  1.f, 0.5f,  "Pulse 2 Level",            "%",   0.f,                100.f       );
        configParam(PARAM_LEVEL2,  0.f,  1.f, 0.25f, "Saw Level / Quantization", "%",   0.f,                100.f       );
        cvDivider.setDivision(16);
        // set the output buffer for each individual voice
        for (int i = 0; i < VRC6::OSC_COUNT; i++) {
            apu.osc_output(i, &buf[i]);
            buf[i].set_clock_rate(CLOCK_RATE);
        }
        // global volume of 3 produces a roughly 5Vpp signal from all voices
        apu.volume(3.f);
    }

    /// Process square wave (channel 0).
    void channel0_pulse() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ_MIN = 4;
        // the maximal value for the frequency register
        static constexpr float FREQ_MAX = 4095;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 16;
        // the constant modulation factor
        static constexpr float MOD_FACTOR = 10;
        // the minimal value for the pulse width register
        static constexpr float PW_MIN = 0;
        // the maximal value for the pulse width register (0b00000111)
        static constexpr float PW_MAX = 7;
        // the minimal value for the volume width register
        static constexpr float LEVEL_MIN = 0;
        // the maximal value for the volume width register (0b00001111)
        static constexpr float LEVEL_MAX = 15;
        // the index of the channel
        static constexpr int CHANNEL_INDEX = 0;

        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ0].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT0].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM0].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = (CLOCK_RATE / (CLOCK_DIVISION * freq)) - 1;
        uint16_t freq12bit = rack::clamp(freq, FREQ_MIN, FREQ_MAX);
        // convert the frequency to a 12-bit value spanning two 8-bit registers
        uint8_t lo = freq12bit & 0b11111111;
        uint8_t hi = (freq12bit & 0b0000111100000000) >> 8;
        // enable the channel
        hi |= 0b10000000;
        // write the register for the frequency
        apu.write_osc(0, CHANNEL_INDEX, PULSE0_PERIOD_LOW, lo);
        apu.write_osc(0, CHANNEL_INDEX, PULSE0_PERIOD_HIGH, hi);

        // get the pulse width from the parameter knob
        auto pwParam = params[PARAM_PW0].getValue();
        // get the control voltage to the pulse width with 1V/step
        auto pwCV = inputs[INPUT_PW0].getVoltage() / 2.f;
        // get the 8-bit pulse width clamped within legal limits
        uint8_t pw = rack::clamp(pwParam + pwCV, PW_MIN, PW_MAX);
        // get the level from the parameter knob
        auto levelParam = params[PARAM_LEVEL0].getValue();
        // apply the control voltage to the level
        auto levelCV = inputs[INPUT_LEVEL0].getVoltage() / 2.f;
        // get the 8-bit level clamped within legal limits
        uint8_t level = rack::clamp(LEVEL_MAX * (levelParam + levelCV), LEVEL_MIN, LEVEL_MAX);
        // write the register for the duty cycle and volume
        apu.write_osc(0, CHANNEL_INDEX, PULSE0_DUTY_VOLUME, (pw << 4) + level);
    }

    /// Process square wave (channel 1).
    void channel1_pulse() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ_MIN = 4;
        // the maximal value for the frequency register
        static constexpr float FREQ_MAX = 4095;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 16;
        // the constant modulation factor
        static constexpr float MOD_FACTOR = 10;
        // the minimal value for the pulse width register
        static constexpr float PW_MIN = 0;
        // the maximal value for the pulse width register (0b00000111)
        static constexpr float PW_MAX = 7;
        // the minimal value for the volume width register
        static constexpr float LEVEL_MIN = 0;
        // the maximal value for the volume width register (0b00001111)
        static constexpr float LEVEL_MAX = 15;
        // the index of the channel
        static constexpr int CHANNEL_INDEX = 1;

        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ1].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT1].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM1].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = (CLOCK_RATE / (CLOCK_DIVISION * freq)) - 1;
        uint16_t freq12bit = rack::clamp(freq, FREQ_MIN, FREQ_MAX);
        // convert the frequency to a 12-bit value spanning two 8-bit registers
        uint8_t lo = freq12bit & 0b11111111;
        uint8_t hi = (freq12bit & 0b0000111100000000) >> 8;
        // enable the channel
        hi |= 0b10000000;
        // write the register for the frequency
        apu.write_osc(0, CHANNEL_INDEX, PULSE1_PERIOD_LOW, lo);
        apu.write_osc(0, CHANNEL_INDEX, PULSE1_PERIOD_HIGH, hi);

        // get the pulse width from the parameter knob
        auto pwParam = params[PARAM_PW1].getValue();
        // get the control voltage to the pulse width with 1V/step
        auto pwCV = inputs[INPUT_PW1].getVoltage() / 2.f;
        // get the 8-bit pulse width clamped within legal limits
        uint8_t pw = rack::clamp(pwParam + pwCV, PW_MIN, PW_MAX);
        // get the level from the parameter knob
        auto levelParam = params[PARAM_LEVEL1].getValue();
        // apply the control voltage to the level
        auto levelCV = inputs[INPUT_LEVEL1].getVoltage() / 2.f;
        // get the 8-bit level clamped within legal limits
        uint8_t level = rack::clamp(LEVEL_MAX * (levelParam + levelCV), LEVEL_MIN, LEVEL_MAX);
        // write the register for the duty cycle and volume
        apu.write_osc(0, CHANNEL_INDEX, PULSE1_DUTY_VOLUME, (pw << 4) + level);
    }

    /// Process saw wave (channel 2).
    void channel2_saw() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ_MIN = 3;
        // the maximal value for the frequency register
        static constexpr float FREQ_MAX = 4095;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 14;
        // the constant modulation factor
        static constexpr float MOD_FACTOR = 10;
        // the minimal value for the volume width register
        static constexpr float LEVEL_MIN = 0;
        // the maximal value for the volume width register (0b00111111)
        // the actual max for volume is 42, but some distortion effects are
        // available on the chip from 42 to 63 (as a result of overflow)
        static constexpr float LEVEL_MAX = 63;
        // the index of the channel
        static constexpr int CHANNEL_INDEX = 2;

        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ2].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT2].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM2].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = (CLOCK_RATE / (CLOCK_DIVISION * freq)) - 1;
        uint16_t freq12bit = rack::clamp(freq, FREQ_MIN, FREQ_MAX);
        // convert the frequency to a 12-bit value spanning two 8-bit registers
        uint8_t lo = freq12bit & 0b11111111;
        uint8_t hi = (freq12bit & 0b0000111100000000) >> 8;
        // enable the channel
        hi |= 0b10000000;
        // write the register for the frequency
        apu.write_osc(0, CHANNEL_INDEX, SAW_PERIOD_LOW, lo);
        apu.write_osc(0, CHANNEL_INDEX, SAW_PERIOD_HIGH, hi);

        // get the level from the parameter knob
        auto levelParam = params[PARAM_LEVEL2].getValue();
        // apply the control voltage to the level
        auto levelCV = inputs[INPUT_LEVEL2].getVoltage() / 2.f;
        // get the 8-bit level clamped within legal limits
        uint8_t level = rack::clamp(LEVEL_MAX * (levelParam + levelCV), LEVEL_MIN, LEVEL_MAX);
        // write the register for the volume
        apu.write_osc(0, CHANNEL_INDEX, SAW_VOLUME, level);
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
        auto samples = buf[channel].samples_count();
        if (samples == 0) return 0.f;
        // copy the buffer to  a local vector and return the first sample
        std::vector<int16_t> output_buffer(samples);
        buf[channel].read_samples(&output_buffer[0], samples);
        // convert the 16-bit sample to 10Vpp floating point
        return Vpp * output_buffer[0] / divisor;
    }

    /// Process a sample.
    void process(const ProcessArgs &args) override {
        // calculate the number of clock cycles on the chip per audio sample
        uint32_t cycles_per_sample = CLOCK_RATE / args.sampleRate;
        // check for sample rate changes from the engine to send to the chip
        if (new_sample_rate) {
            // update the buffer for each channel
            for (int i = 0; i < VRC6::OSC_COUNT; i++) {
                buf[i].set_sample_rate(args.sampleRate);
                buf[i].set_clock_rate(cycles_per_sample * args.sampleRate);
            }
            // clear the new sample rate flag
            new_sample_rate = false;
        }
        if (cvDivider.process()) {  // process the CV inputs to the chip
            channel0_pulse();
            channel1_pulse();
            channel2_saw();
        }
        // process audio samples on the chip engine
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < VRC6::OSC_COUNT; i++)
            buf[i].end_frame(cycles_per_sample);
        // set the output from the oscillators
        outputs[OUTPUT_CHANNEL0].setVoltage(getAudioOut(0));
        outputs[OUTPUT_CHANNEL1].setVoltage(getAudioOut(1));
        outputs[OUTPUT_CHANNEL2].setVoltage(getAudioOut(2));
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override { new_sample_rate = true; }
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
        addInput(createInput<PJ301MPort>(Vec(20, 78), module, ChipVRC6::INPUT_VOCT0));
        addInput(createInput<PJ301MPort>(Vec(20, 188), module, ChipVRC6::INPUT_VOCT1));
        addInput(createInput<PJ301MPort>(Vec(20, 298), module, ChipVRC6::INPUT_VOCT2));
        // FM inputs
        addInput(createInput<PJ301MPort>(Vec(26, 37), module, ChipVRC6::INPUT_FM0));
        addInput(createInput<PJ301MPort>(Vec(26, 149), module, ChipVRC6::INPUT_FM1));
        addInput(createInput<PJ301MPort>(Vec(26, 258), module, ChipVRC6::INPUT_FM2));
        // PW inputs
        addParam(createParam<Rogan0PSNES_Snap>(Vec(30, 107), module, ChipVRC6::PARAM_PW0));
        addParam(createParam<Rogan0PSNES_Snap>(Vec(30, 218), module, ChipVRC6::PARAM_PW1));
        addInput(createInput<PJ301MPort>(Vec(58, 104), module, ChipVRC6::INPUT_PW0));
        addInput(createInput<PJ301MPort>(Vec(58, 215), module, ChipVRC6::INPUT_PW1));
        // Frequency parameters
        addParam(createParam<Rogan3PSNES>(Vec(54, 42), module, ChipVRC6::PARAM_FREQ0));
        addParam(createParam<Rogan3PSNES>(Vec(54, 151), module, ChipVRC6::PARAM_FREQ1));
        addParam(createParam<Rogan3PSNES>(Vec(54, 266), module, ChipVRC6::PARAM_FREQ2));
        // Levels
        addInput(createInput<PJ301MPort>(Vec(102, 36), module, ChipVRC6::INPUT_LEVEL0));
        addInput(createInput<PJ301MPort>(Vec(102, 146), module, ChipVRC6::INPUT_LEVEL1));
        addInput(createInput<PJ301MPort>(Vec(102, 255), module, ChipVRC6::INPUT_LEVEL2));
        addParam(createParam<Rogan0PSNES>(Vec(103, 64), module, ChipVRC6::PARAM_LEVEL0));
        addParam(createParam<Rogan0PSNES>(Vec(103, 174), module, ChipVRC6::PARAM_LEVEL1));
        addParam(createParam<Rogan0PSNES>(Vec(103, 283), module, ChipVRC6::PARAM_LEVEL2));
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(107, 104), module, ChipVRC6::OUTPUT_CHANNEL0));
        addOutput(createOutput<PJ301MPort>(Vec(107, 214), module, ChipVRC6::OUTPUT_CHANNEL1));
        addOutput(createOutput<PJ301MPort>(Vec(107, 324), module, ChipVRC6::OUTPUT_CHANNEL2));
    }
};

/// the global instance of the model
Model *modelChipVRC6 = createModel<ChipVRC6, ChipVRC6Widget>("VRC6");
