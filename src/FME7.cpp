// A Sunsoft 5B FME7 Chip module.
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
#include "dsp/fme7.h"

/// the IO registers on the FME7.
enum IORegisters {
    PULSE_A_LO   = 0x00,
    PULSE_A_HI   = 0x01,
    PULSE_B_LO   = 0x02,
    PULSE_B_HI   = 0x03,
    PULSE_C_LO   = 0x04,
    PULSE_C_HI   = 0x05,
    NOISE_PERIOD = 0x06,
    NOISE_TONE   = 0x07,
    PULSE_A_ENV  = 0x08,
    PULSE_B_ENV  = 0x09,
    PULSE_C_ENV  = 0x0A,
    ENV_LO       = 0x0B,
    ENV_HI       = 0x0C,
    ENV_RESET    = 0x0D,
    IO_PORT_A    = 0x0E,  // unused
    IO_PORT_B    = 0x0F   // unused
};

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Sunsoft 5B (FME7) Chip module.
struct ChipFME7 : Module {
    enum ParamIds {
        PARAM_FREQ_A,
        PARAM_FREQ_B,
        PARAM_FREQ_C,
        PARAM_FREQ_NOISE,
        PARAM_COUNT
    };
    enum InputIds {
        INPUT_VOCT_A,
        INPUT_VOCT_B,
        INPUT_VOCT_C,
        INPUT_VOCT_NOISE,
        INPUT_FM_A,
        INPUT_FM_B,
        INPUT_FM_C,
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, 3),
        OUTPUT_COUNT
    };
    enum LightIds { LIGHT_COUNT };

    /// the clock rate of the module
    static constexpr uint64_t CLOCK_RATE = 768000;

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[FME7::OSC_COUNT];
    /// The FME7 instance to synthesize sound with
    FME7 apu;

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    /// Initialize a new FME7 Chip module.
    ChipFME7() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ_A,     -30.f, 30.f, 0.f, "Pulse A Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ_B,     -30.f, 30.f, 0.f, "Pulse B Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ_C,     -30.f, 30.f, 0.f, "Pulse C Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ_NOISE, -30.f, 30.f, 0.f, "Noise Frequency",   " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        // set the output buffer for each individual voice
        for (int i = 0; i < FME7::OSC_COUNT; i++) {
            apu.osc_output(i, &buf[i]);
            buf[i].set_clock_rate(CLOCK_RATE);
        }
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.volume(3.f);
    }

    /// Process pulse wave (channel A).
    void channelA_pulse() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ12BIT_MIN = 8;
        // the maximal value for the frequency register
        static constexpr float FREQ12BIT_MAX = 8192;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // the constant modulation factor
        static constexpr auto MOD_FACTOR = 10.f;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ_A].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT_A].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM_A].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to 12-bit
        freq = CLOCK_RATE / (CLOCK_DIVISION * freq);
        uint16_t freq12bit = rack::clamp(freq, FREQ12BIT_MIN, FREQ12BIT_MAX);
        // write the registers with the frequency data
        apu.write_latch(PULSE_A_LO);
        apu.write_data(0, freq12bit & 0b11111111);
        apu.write_latch(PULSE_A_HI);
        apu.write_data(0, (freq12bit & 0b0000111100000000) >> 8);
        // set the volume to a constant level
        apu.write_latch(PULSE_A_ENV);
        apu.write_data(0, 0b00000111);
    }

    /// Process pulse wave (channel B).
    void channelB_pulse() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ12BIT_MIN = 8;
        // the maximal value for the frequency register
        static constexpr float FREQ12BIT_MAX = 8192;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // the constant modulation factor
        static constexpr auto MOD_FACTOR = 10.f;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ_B].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT_B].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM_B].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to an 12-bit value
        freq = CLOCK_RATE / (CLOCK_DIVISION * freq);
        uint16_t freq12bit = rack::clamp(freq, FREQ12BIT_MIN, FREQ12BIT_MAX);
        // write the registers with the frequency data
        apu.write_latch(PULSE_B_LO);
        apu.write_data(0, freq12bit & 0b11111111);
        apu.write_latch(PULSE_B_HI);
        apu.write_data(0, (freq12bit & 0b0000111100000000) >> 8);
        // set the volume to a constant level
        apu.write_latch(PULSE_B_ENV);
        apu.write_data(0, 0b00000111);
    }

    /// Process pulse wave (channel C).
    void channelC_pulse() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ12BIT_MIN = 2;
        // the maximal value for the frequency register
        static constexpr float FREQ12BIT_MAX = 8192;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // the constant modulation factor
        static constexpr auto MOD_FACTOR = 10.f;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ_C].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT_C].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM_C].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to an 12-bit value
        freq = CLOCK_RATE / (CLOCK_DIVISION * freq);
        uint16_t freq12bit = rack::clamp(freq, FREQ12BIT_MIN, FREQ12BIT_MAX);
        // write the registers with the frequency data
        apu.write_latch(PULSE_C_LO);
        apu.write_data(0, freq12bit & 0b11111111);
        apu.write_latch(PULSE_C_HI);
        apu.write_data(0, (freq12bit & 0b0000111100000000) >> 8);
        // set the volume to a constant level
        apu.write_latch(PULSE_C_ENV);
        apu.write_data(0, 0b00000111);
    }

    /// Process noise (channel D).
    void channelD_noise() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ5BIT_MIN = 2;
        // the maximal value for the frequency register
        static constexpr float FREQ5BIT_MAX = 0b00011111;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // the constant modulation factor
        static constexpr auto MOD_FACTOR = 10.f;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ_C].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT_C].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM_C].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to an 12-bit value
        freq = CLOCK_RATE / (CLOCK_DIVISION * freq);
        uint16_t freq12bit = rack::clamp(freq, FREQ5BIT_MIN, FREQ5BIT_MAX);
        apu.write_latch(NOISE_PERIOD);
        apu.write_data(0, freq12bit & 0b00011111);
    }

    /// Return a 10V signed sample from the FME7.
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
        // copy the buffer to a local vector and return the first sample
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
            for (int i = 0; i < FME7::OSC_COUNT; i++) {
                buf[i].set_sample_rate(args.sampleRate);
                buf[i].set_clock_rate(cycles_per_sample * args.sampleRate);
            }
            // clear the new sample rate flag
            new_sample_rate = false;
        }
        // process the data on the chip
        // apu.write_latch(NOISE_TONE);
        // apu.write_data(0, 0);
        channelA_pulse();
        channelB_pulse();
        channelC_pulse();
        channelD_noise();
        // enable all four channels
        // apu.write_register(0, SND_CHN, 0b00001111);
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < FME7::OSC_COUNT; i++) {
            buf[i].end_frame(cycles_per_sample);
            outputs[OUTPUT_CHANNEL + i].setVoltage(getAudioOut(i));
        }
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override { new_sample_rate = true; }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The widget structure that lays out the panel of the module and the UI menus.
struct ChipFME7Widget : ModuleWidget {
    ChipFME7Widget(ChipFME7 *module) {
        setModule(module);
        static const auto panel = "res/FME7.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // V/OCT inputs
        addInput(createInput<PJ301MPort>(Vec(20, 74), module, ChipFME7::INPUT_VOCT_A));
        addInput(createInput<PJ301MPort>(Vec(20, 159), module, ChipFME7::INPUT_VOCT_B));
        addInput(createInput<PJ301MPort>(Vec(20, 244), module, ChipFME7::INPUT_VOCT_C));
        addInput(createInput<PJ301MPort>(Vec(20, 329), module, ChipFME7::INPUT_VOCT_NOISE));
        // FM inputs
        addInput(createInput<PJ301MPort>(Vec(25, 32), module, ChipFME7::INPUT_FM_A));
        addInput(createInput<PJ301MPort>(Vec(25, 118), module, ChipFME7::INPUT_FM_B));
        addInput(createInput<PJ301MPort>(Vec(25, 203), module, ChipFME7::INPUT_FM_C));
        // Frequency parameters
        addParam(createParam<Rogan3PSNES>(Vec(54, 42), module, ChipFME7::PARAM_FREQ_A));
        addParam(createParam<Rogan3PSNES>(Vec(54, 126), module, ChipFME7::PARAM_FREQ_B));
        addParam(createParam<Rogan3PSNES>(Vec(54, 211), module, ChipFME7::PARAM_FREQ_C));
        addParam(createParam<Rogan3PSNES_Snap>(Vec(54, 297), module, ChipFME7::PARAM_FREQ_NOISE));
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(106, 74), module, ChipFME7::OUTPUT_CHANNEL + 0));
        addOutput(createOutput<PJ301MPort>(Vec(106, 159), module, ChipFME7::OUTPUT_CHANNEL + 1));
        addOutput(createOutput<PJ301MPort>(Vec(106, 244), module, ChipFME7::OUTPUT_CHANNEL + 2));
    }
};

/// the global instance of the model
Model *modelChipFME7 = createModel<ChipFME7, ChipFME7Widget>("FME7");
