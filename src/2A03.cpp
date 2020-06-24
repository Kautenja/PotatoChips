// A Ricoh 2A03 Chip module.
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
#include "dsp/apu.hpp"

/// the IO registers on the APU
enum IORegisters {
    PULSE0_VOL =     0x4000,
    PULSE0_SWEEP =   0x4001,
    PULSE0_LO =      0x4002,
    PULSE0_HI =      0x4003,
    PULSE1_VOL =     0x4004,
    PULSE1_SWEEP =   0x4005,
    PULSE1_LO =      0x4006,
    PULSE1_HI =      0x4007,
    TRI_LINEAR =     0x4008,
    // APU_UNUSED1 =    0x4009,  // may be used for memory clearing loops
    TRI_LO =         0x400A,
    TRI_HI =         0x400B,
    NOISE_VOL =      0x400C,
    // APU_UNUSED2 =    0x400D,  // may be used for memory clearing loops
    NOISE_LO =       0x400E,
    NOISE_HI =       0x400F,
    DMC_FREQ =       0x4010,
    DMC_RAW =        0x4011,
    DMC_START =      0x4012,
    DMC_LEN =        0x4013,
    SND_CHN =        0x4015,
    // JOY1 =           0x4016,  // unused for APU
    JOY2 =           0x4017,
};

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Ricoh 2A03 Chip module.
struct Chip2A03 : Module {
    enum ParamIds {
        PARAM_FREQ0,
        PARAM_FREQ1,
        PARAM_FREQ2,
        PARAM_FREQ3,
        PARAM_PW0,
        PARAM_PW1,
        PARAM_COUNT
    };
    enum InputIds {
        INPUT_VOCT0,
        INPUT_VOCT1,
        INPUT_VOCT2,
        INPUT_VOCT3,
        INPUT_FM0,
        INPUT_FM1,
        INPUT_FM2,
        INPUT_LFSR,
        INPUT_COUNT
    };
    enum OutputIds {
        OUTPUT_CHANNEL0,
        OUTPUT_CHANNEL1,
        OUTPUT_CHANNEL2,
        OUTPUT_CHANNEL3,
        OUTPUT_COUNT
    };
    enum LightIds { LIGHT_COUNT };

    /// the clock rate of the module
    static constexpr uint64_t CLOCK_RATE = 768000;

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[APU::OSC_COUNT];
    /// The 2A03 instance to synthesize sound with
    APU apu;

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    /// a Schmitt Trigger for handling inputs to the LFSR port
    dsp::SchmittTrigger lfsr;

    /// Initialize a new 2A03 Chip module.
    Chip2A03() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ0, -30.f, 30.f, 0.f, "Pulse 1 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ1, -30.f, 30.f, 0.f, "Pulse 2 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ2, -30.f, 30.f, 0.f, "Triangle Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ3,   0,   15,   7,   "Noise Period", "", 0, 1, -15);
        configParam(PARAM_PW0,     0,    3,   2,   "Pulse 1 Duty Cycle");
        configParam(PARAM_PW1,     0,    3,   2,   "Pulse 2 Duty Cycle");
        // set the output buffer for each individual voice
        for (int i = 0; i < APU::OSC_COUNT; i++)
            apu.osc_output(i, &buf[i]);
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.volume(3.f);
    }

    /// Process pulse wave (channel 0).
    void channel0_pulse() {
        // the minimal value for the frequency register to produce sound
        static constexpr auto FREQ_MIN = 8;
        // the maximal value for the frequency register
        static constexpr auto FREQ_MAX = 1023;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 16;
        // get the pitch / frequency of the oscillator in 11-bit
        float pitch = params[PARAM_FREQ0].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT0].getVoltage();
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        uint16_t freq11bit = (CLOCK_RATE / (CLOCK_DIVISION * freq)) - 1;
        freq11bit += inputs[INPUT_FM0].getVoltage();
        // TODO: clamp before converting to 16 bit
        freq11bit = rack::clamp(freq11bit, FREQ_MIN, FREQ_MAX);
        apu.write_register(0, PULSE0_LO, freq11bit & 0b11111111);
        apu.write_register(0, PULSE0_HI, (freq11bit & 0b0000011100000000) >> 8);
        // set the pulse width of the pulse wave (high 2 bits) and set the
        // volume to a constant level
        auto pw = static_cast<uint8_t>(params[PARAM_PW0].getValue()) << 6;
        apu.write_register(0, PULSE0_VOL, pw + 0b00011111);
    }

    /// Process pulse wave (channel 1).
    void channel1_pulse() {
        // the minimal value for the frequency register to produce sound
        static constexpr auto FREQ_MIN = 8;
        // the maximal value for the frequency register
        static constexpr auto FREQ_MAX = 1023;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 16;
        // get the pitch / frequency of the oscillator in 11-bit
        float pitch = params[PARAM_FREQ1].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT1].getVoltage();
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        uint16_t freq11bit = (CLOCK_RATE / (CLOCK_DIVISION * freq)) - 1;
        freq11bit += inputs[INPUT_FM1].getVoltage();
        // TODO: clamp before converting to 16 bit
        freq11bit = rack::clamp(freq11bit, FREQ_MIN, FREQ_MAX);
        apu.write_register(0, PULSE1_LO, freq11bit & 0b11111111);
        apu.write_register(0, PULSE1_HI, (freq11bit & 0b0000011100000000) >> 8);
        // set the pulse width of the pulse wave (high 2 bits) and set the
        // volume to a constant level
        auto pw = static_cast<uint8_t>(params[PARAM_PW1].getValue()) << 6;
        apu.write_register(0, PULSE1_VOL, pw + 0b00011111);
    }

    /// Process triangle wave (channel 2).
    void channel2_triangle() {
        // the minimal value for the frequency register to produce sound
        static constexpr auto FREQ_MIN = 2;
        // the maximal value for the frequency register
        static constexpr auto FREQ_MAX = 2047;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // get the pitch / frequency of the oscillator in 11-bit
        float pitch = params[PARAM_FREQ2].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT2].getVoltage();
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        uint16_t freq11bit = (CLOCK_RATE / (CLOCK_DIVISION * freq)) - 1;
        freq11bit += inputs[INPUT_FM2].getVoltage();
        // TODO: clamp before converting to 16 bit
        freq11bit = rack::clamp(freq11bit, FREQ_MIN, FREQ_MAX);
        apu.write_register(0, TRI_LO, freq11bit & 0b11111111);
        apu.write_register(0, TRI_HI, (freq11bit & 0b0000011100000000) >> 8);
        // write the linear register to enable the oscillator
        apu.write_register(0, TRI_LINEAR, 0b01111111);
    }

    /// Process noise (channel 3).
    void channel3_noise() {
        // the minimal value for the frequency register to produce sound
        static constexpr auto FREQ_MIN = 0;
        // the maximal value for the frequency register
        static constexpr auto FREQ_MAX = 15;
        // get the pitch / frequency of the oscillator
        auto sign = sgn(inputs[INPUT_VOCT3].getVoltage());
        auto pitch = abs(inputs[INPUT_VOCT3].getVoltage() / 100.f);
        auto cv = rack::dsp::FREQ_C4 * sign * (powf(2.0, pitch) - 1.f);
        uint32_t param = params[PARAM_FREQ3].getValue() + cv;
        // TODO: clamp before converting to 32 bit
        param = FREQ_MAX - rack::clamp(param, FREQ_MIN, FREQ_MAX);
        apu.write_register(0, NOISE_LO, lfsr.isHigh() * 0b10000000 + param);
        apu.write_register(0, NOISE_HI, 0);
        // set the volume to a constant level
        apu.write_register(0, NOISE_VOL, 0b00011111);
    }

    /// Return a 10V signed sample from the APU.
    ///
    /// @param channel the channel to get the audio sample for
    ///
    float getAudioOut(int channel) {
        auto samples = buf[channel].samples_count();
        if (samples == 0) return 0.f;
        // copy the buffer to a local vector and return the first sample
        std::vector<int16_t> output_buffer(samples);
        buf[channel].read_samples(&output_buffer[0], samples);
        // convert the 16-bit sample to 10Vpp floating point
        return 10.f * output_buffer[0] / static_cast<float>(1 << 15);;
    }

    /// Process a sample.
    void process(const ProcessArgs &args) override {
        // calculate the number of clock cycles on the chip per audio sample
        uint32_t cycles_per_sample = CLOCK_RATE / args.sampleRate;
        // check for sample rate changes from the engine to send to the chip
        if (new_sample_rate) {
            // update the buffer for each channel
            for (int i = 0; i < APU::OSC_COUNT; i++) {
                buf[i].set_sample_rate(args.sampleRate);
                buf[i].set_clock_rate(cycles_per_sample * args.sampleRate);
            }
            // clear the new sample rate flag
            new_sample_rate = false;
        }
        // process input triggers
        lfsr.process(rescale(inputs[INPUT_LFSR].getVoltage(), 0.f, 2.f, 0.f, 1.f));
        // process the data on the chip
        channel0_pulse();
        channel1_pulse();
        channel2_triangle();
        channel3_noise();
        // enable all four channels
        apu.write_register(0, SND_CHN, 0b00001111);
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < APU::OSC_COUNT; i++)
            buf[i].end_frame(cycles_per_sample);
        // set the output from the oscillators
        outputs[OUTPUT_CHANNEL0].setVoltage(getAudioOut(0));
        outputs[OUTPUT_CHANNEL1].setVoltage(getAudioOut(1));
        outputs[OUTPUT_CHANNEL2].setVoltage(getAudioOut(2));
        outputs[OUTPUT_CHANNEL3].setVoltage(getAudioOut(3));
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override { new_sample_rate = true; }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The widget structure that lays out the panel of the module and the UI menus.
struct Chip2A03Widget : ModuleWidget {
    Chip2A03Widget(Chip2A03 *module) {
        setModule(module);
        static const auto panel = "res/2A03.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // V/OCT inputs
        addInput(createInput<PJ301MPort>(Vec(28, 74), module, Chip2A03::INPUT_VOCT0));
        addInput(createInput<PJ301MPort>(Vec(28, 159), module, Chip2A03::INPUT_VOCT1));
        addInput(createInput<PJ301MPort>(Vec(28, 244), module, Chip2A03::INPUT_VOCT2));
        addInput(createInput<PJ301MPort>(Vec(28, 329), module, Chip2A03::INPUT_VOCT3));
        // FM inputs
        addInput(createInput<PJ301MPort>(Vec(33, 32), module, Chip2A03::INPUT_FM0));
        addInput(createInput<PJ301MPort>(Vec(33, 118), module, Chip2A03::INPUT_FM1));
        addInput(createInput<PJ301MPort>(Vec(33, 203), module, Chip2A03::INPUT_FM2));
        // Frequency parameters
        addParam(createParam<Rogan3PSNES>(Vec(62, 42), module, Chip2A03::PARAM_FREQ0));
        addParam(createParam<Rogan3PSNES>(Vec(62, 126), module, Chip2A03::PARAM_FREQ1));
        addParam(createParam<Rogan3PSNES>(Vec(62, 211), module, Chip2A03::PARAM_FREQ2));
        addParam(createParam<Rogan3PSNES_Snap>(Vec(62, 297), module, Chip2A03::PARAM_FREQ3));
        // PW
        addParam(createParam<Rogan0PSNES_Snap>(Vec(109, 30), module, Chip2A03::PARAM_PW0));
        addParam(createParam<Rogan0PSNES_Snap>(Vec(109, 115), module, Chip2A03::PARAM_PW1));
        // LFSR switch
        addInput(createInput<PJ301MPort>(Vec(32, 284), module, Chip2A03::INPUT_LFSR));
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(114, 74), module, Chip2A03::OUTPUT_CHANNEL0));
        addOutput(createOutput<PJ301MPort>(Vec(114, 159), module, Chip2A03::OUTPUT_CHANNEL1));
        addOutput(createOutput<PJ301MPort>(Vec(114, 244), module, Chip2A03::OUTPUT_CHANNEL2));
        addOutput(createOutput<PJ301MPort>(Vec(114, 329), module, Chip2A03::OUTPUT_CHANNEL3));
    }
};

/// the global instance of the model
Model *modelChip2A03 = createModel<Chip2A03, Chip2A03Widget>("2A03");
