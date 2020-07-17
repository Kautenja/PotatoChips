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
#include "dsp/ricoh_2a03_apu.hpp"

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
    APU_UNUSED1 =    0x4009,  // may be used for memory clearing loops
    TRI_LO =         0x400A,
    TRI_HI =         0x400B,
    NOISE_VOL =      0x400C,
    APU_UNUSED2 =    0x400D,  // may be used for memory clearing loops
    NOISE_LO =       0x400E,
    NOISE_HI =       0x400F,
    DMC_FREQ =       0x4010,
    DMC_RAW =        0x4011,
    DMC_START =      0x4012,
    DMC_LEN =        0x4013,
    SND_CHN =        0x4015,
    JOY1 =           0x4016,  // unused for APU
    STATUS =         0x4017,
};

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Ricoh 2A03 Chip module.
struct Chip2A03 : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, APU::OSC_COUNT),
        ENUMS(PARAM_PW, 2),
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, APU::OSC_COUNT),
        ENUMS(INPUT_FM, 3),
        INPUT_LFSR,
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, APU::OSC_COUNT),
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

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

    /// a Schmitt Trigger for handling inputs to the LFSR port
    dsp::SchmittTrigger lfsr;

    /// Initialize a new 2A03 Chip module.
    Chip2A03() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ + 0, -30.f, 30.f, 0.f, "Pulse 1 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -30.f, 30.f, 0.f, "Pulse 2 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2, -30.f, 30.f, 0.f, "Triangle Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 3,   0,   15,   7,   "Noise Period", "", 0, 1, -15);
        configParam(PARAM_PW + 0,     0,    3,   2,   "Pulse 1 Duty Cycle");
        configParam(PARAM_PW + 1,     0,    3,   2,   "Pulse 2 Duty Cycle");
        cvDivider.setDivision(16);
        // set the output buffer for each individual voice
        for (int i = 0; i < APU::OSC_COUNT; i++) {
            apu.osc_output(i, &buf[i]);
        }
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
        apu.write_register(0, PULSE0_LO + 4 * channel, freq11bit & 0b11111111);
        apu.write_register(0, PULSE0_HI + 4 * channel, (freq11bit & 0b0000011100000000) >> 8);
        // set the pulse width of the pulse wave (high 2 bits) and set the
        // volume to a constant level
        auto pw = static_cast<uint8_t>(params[PARAM_PW + channel].getValue()) << 6;
        apu.write_register(0, PULSE0_VOL + 4 * channel, pw + 0b00011111);
    }

    /// Process triangle wave (channel 2).
    void channel_triangle() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ11BIT_MIN = 2;
        // the maximal value for the frequency register
        static constexpr float FREQ11BIT_MAX = 2047;
        // the clock division of the oscillator relative to the CPU
        static constexpr auto CLOCK_DIVISION = 32;
        // the constant modulation factor
        static constexpr auto MOD_FACTOR = 10.f;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + 2].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + 2].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM + 2].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to an 11-bit value
        freq = (buf[2].get_clock_rate() / (CLOCK_DIVISION * freq)) - 1;
        uint16_t freq11bit = rack::clamp(freq, FREQ11BIT_MIN, FREQ11BIT_MAX);
        // write the frequency to the low and high registers
        apu.write_register(0, TRI_LO, freq11bit & 0b11111111);
        apu.write_register(0, TRI_HI, (freq11bit & 0b0000011100000000) >> 8);
        // write the linear register to enable the oscillator
        apu.write_register(0, TRI_LINEAR, 0b01111111);
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
        apu.write_register(0, NOISE_LO, lfsr.isHigh() * 0b10000000 + period);
        apu.write_register(0, NOISE_HI, 0);
        // set the volume to a constant level
        apu.write_register(0, NOISE_VOL, 0b00011111);
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
        // copy the buffer to a local vector and return the first sample
        std::vector<int16_t> output_buffer(1);
        buf[channel].read_samples(&output_buffer[0]);
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
            for (int i = 0; i < APU::OSC_COUNT; i++)
                buf[i].set_sample_rate(args.sampleRate);
            // clear the new sample rate flag
            new_sample_rate = false;
        }
        if (cvDivider.process()) {  // process the CV inputs to the chip
            lfsr.process(rescale(inputs[INPUT_LFSR].getVoltage(), 0.f, 2.f, 0.f, 1.f));
            // process the data on the chip
            channel_pulse(0);
            channel_pulse(1);
            channel_triangle();
            channel_noise();
            // enable all four channels
            apu.write_register(0, SND_CHN, 0b00001111);
        }
        // process audio samples on the chip engine
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < APU::OSC_COUNT; i++) {
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
struct Chip2A03Widget : ModuleWidget {
    Chip2A03Widget(Chip2A03 *module) {
        setModule(module);
        static constexpr auto panel = "res/2A03.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // V/OCT inputs
        addInput(createInput<PJ301MPort>(Vec(20, 74), module, Chip2A03::INPUT_VOCT + 0));
        addInput(createInput<PJ301MPort>(Vec(20, 159), module, Chip2A03::INPUT_VOCT + 1));
        addInput(createInput<PJ301MPort>(Vec(20, 244), module, Chip2A03::INPUT_VOCT + 2));
        addInput(createInput<PJ301MPort>(Vec(20, 329), module, Chip2A03::INPUT_VOCT + 3));
        // FM inputs
        addInput(createInput<PJ301MPort>(Vec(25, 32), module, Chip2A03::INPUT_FM + 0));
        addInput(createInput<PJ301MPort>(Vec(25, 118), module, Chip2A03::INPUT_FM + 1));
        addInput(createInput<PJ301MPort>(Vec(25, 203), module, Chip2A03::INPUT_FM + 2));
        // Frequency parameters
        addParam(createParam<Rogan3PSNES>(Vec(54, 42), module, Chip2A03::PARAM_FREQ + 0));
        addParam(createParam<Rogan3PSNES>(Vec(54, 126), module, Chip2A03::PARAM_FREQ + 1));
        addParam(createParam<Rogan3PSNES>(Vec(54, 211), module, Chip2A03::PARAM_FREQ + 2));
        addParam(createParam<Rogan3PSNES_Snap>(Vec(54, 297), module, Chip2A03::PARAM_FREQ + 3));
        // PW
        addParam(createParam<Rogan0PSNES_Snap>(Vec(102, 30), module, Chip2A03::PARAM_PW + 0));
        addParam(createParam<Rogan0PSNES_Snap>(Vec(102, 115), module, Chip2A03::PARAM_PW + 1));
        // LFSR switch
        addInput(createInput<PJ301MPort>(Vec(24, 284), module, Chip2A03::INPUT_LFSR));
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(106, 74),  module, Chip2A03::OUTPUT_CHANNEL + 0));
        addOutput(createOutput<PJ301MPort>(Vec(106, 159), module, Chip2A03::OUTPUT_CHANNEL + 1));
        addOutput(createOutput<PJ301MPort>(Vec(106, 244), module, Chip2A03::OUTPUT_CHANNEL + 2));
        addOutput(createOutput<PJ301MPort>(Vec(106, 329), module, Chip2A03::OUTPUT_CHANNEL + 3));
    }
};

/// the global instance of the model
Model *modelChip2A03 = createModel<Chip2A03, Chip2A03Widget>("2A03");
