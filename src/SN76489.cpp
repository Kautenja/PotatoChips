// A Ricoh SN76489 Chip module.
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
#include "dsp/texas_instruments_sn76489_apu.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Ricoh SN76489 Chip module.
struct ChipSN76489 : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, Sms_Apu::OSC_COUNT),
        ENUMS(PARAM_ATTENUATION, Sms_Apu::OSC_COUNT),
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, Sms_Apu::OSC_COUNT),
        ENUMS(INPUT_FM, 3),
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, Sms_Apu::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds { LIGHT_COUNT };

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[Sms_Apu::OSC_COUNT];
    /// The SN76489 instance to synthesize sound with
    Sms_Apu apu;

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

    /// Initialize a new SN76489 Chip module.
    ChipSN76489() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ + 0, -30.f, 30.f, 0.f, "Tone 1 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -30.f, 30.f, 0.f, "Tone 2 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2, -30.f, 30.f, 0.f, "Tone 3 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 3,   0,   15,   7,   "Noise Control", "", 0, 1, -15);
        // configParam(PARAM_ATTENUATION + 0,   0,   1,    0.5, "Tone 1 Attenuation",  "%", 0, 100);
        // configParam(PARAM_ATTENUATION + 1,   0,   1,    0.5, "Tone 2 Attenuation",  "%", 0, 100);
        // configParam(PARAM_ATTENUATION + 2,   0,   1,    0.5, "Tone 3 Attenuation",  "%", 0, 100);
        cvDivider.setDivision(16);
        // set the output buffer for each individual voice
        for (int i = 0; i < Sms_Apu::OSC_COUNT; i++) apu.osc_output(i, &buf[i]);
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.volume(3.f);
    }

    /// Process pulse wave for given channel.
    ///
    /// @param channel the channel to process the pulse wave for
    ///
    void channel_pulse(int channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ11BIT_MIN = 8;
        // the maximal value for the frequency register
        static constexpr float FREQ11BIT_MAX = 1023;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 16;
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
        freq = (buf[channel].get_clock_rate() / (CLOCK_DIVISION * freq)) - 1;
        uint16_t freq11bit = rack::clamp(freq, FREQ11BIT_MIN, FREQ11BIT_MAX);
        // write the frequency to the low and high registers
        // - there are 4 registers per pulse channel, multiply channel by 4 to
        //   produce an offset between registers based on channel index
        // apu.write_register(0, PULSE0_LO + 4 * channel, freq11bit & 0b11111111);
        // apu.write_register(0, PULSE0_HI + 4 * channel, (freq11bit & 0b0000011100000000) >> 8);
    }

    /// Process noise (channel 3).
    void channel_noise() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ_MIN = 0;
        // the maximal value for the frequency register
        static constexpr float FREQ_MAX = 15;
        // get the pitch / frequency of the oscillator
        auto sign = sgn(inputs[INPUT_VOCT + 3].getVoltage());
        auto pitch = abs(inputs[INPUT_VOCT + 3].getVoltage() / 100.f);
        // convert the pitch to frequency based on standard exponential scale
        auto freq = rack::dsp::FREQ_C4 * sign * (powf(2.0, pitch) - 1.f);
        freq += params[PARAM_FREQ + 3].getValue();
        uint8_t period = FREQ_MAX - rack::clamp(freq, FREQ_MIN, FREQ_MAX);
        // apu.write_register(0, NOISE_LO, 0b10000000 + period);
        // apu.write_register(0, NOISE_HI, 0);
        // // set the volume to a constant level
        // apu.write_register(0, NOISE_VOL, 0b00011111);
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
        // calculate the number of clock cycles on the chip per audio sample
        uint32_t cycles_per_sample = CLOCK_RATE / args.sampleRate;
        // check for sample rate changes from the engine to send to the chip
        if (new_sample_rate) {
            // update the buffer for each channel
            for (int i = 0; i < Sms_Apu::OSC_COUNT; i++)
                buf[i].set_sample_rate(args.sampleRate, CLOCK_RATE);
            // clear the new sample rate flag
            new_sample_rate = false;
        }
        if (cvDivider.process()) {  // process the CV inputs to the chip
            // process the data on the chip
            channel_pulse(0);
            channel_pulse(1);
            channel_pulse(2);
            channel_noise();
            // enable all four channels
            // apu.write_register(0, SND_CHN, 0b00001111);
        }
        // process audio samples on the chip engine
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < Sms_Apu::OSC_COUNT; i++)
            outputs[OUTPUT_CHANNEL + i].setVoltage(getAudioOut(i));
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
        addInput(createInput<PJ301MPort>(Vec(20, 74), module, ChipSN76489::INPUT_VOCT + 0));
        addInput(createInput<PJ301MPort>(Vec(20, 159), module, ChipSN76489::INPUT_VOCT + 1));
        addInput(createInput<PJ301MPort>(Vec(20, 244), module, ChipSN76489::INPUT_VOCT + 2));
        addInput(createInput<PJ301MPort>(Vec(20, 329), module, ChipSN76489::INPUT_VOCT + 3));
        // FM inputs
        addInput(createInput<PJ301MPort>(Vec(25, 32), module, ChipSN76489::INPUT_FM + 0));
        addInput(createInput<PJ301MPort>(Vec(25, 118), module, ChipSN76489::INPUT_FM + 1));
        addInput(createInput<PJ301MPort>(Vec(25, 203), module, ChipSN76489::INPUT_FM + 2));
        // Frequency parameters
        addParam(createParam<Rogan3PSNES>(Vec(54, 42), module, ChipSN76489::PARAM_FREQ + 0));
        addParam(createParam<Rogan3PSNES>(Vec(54, 126), module, ChipSN76489::PARAM_FREQ + 1));
        addParam(createParam<Rogan3PSNES>(Vec(54, 211), module, ChipSN76489::PARAM_FREQ + 2));
        addParam(createParam<Rogan3PSNES_Snap>(Vec(54, 297), module, ChipSN76489::PARAM_FREQ + 3));
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(106, 74),  module, ChipSN76489::OUTPUT_CHANNEL + 0));
        addOutput(createOutput<PJ301MPort>(Vec(106, 159), module, ChipSN76489::OUTPUT_CHANNEL + 1));
        addOutput(createOutput<PJ301MPort>(Vec(106, 244), module, ChipSN76489::OUTPUT_CHANNEL + 2));
        addOutput(createOutput<PJ301MPort>(Vec(106, 329), module, ChipSN76489::OUTPUT_CHANNEL + 3));
    }
};

/// the global instance of the model
Model *modelChipSN76489 = createModel<ChipSN76489, ChipSN76489Widget>("SN76489");
