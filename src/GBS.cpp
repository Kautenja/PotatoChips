// A Nintendo GBS Chip module.
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
#include "dsp/nintendo_gameboy_apu.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Nintendo GBS Chip module.
struct ChipGBS : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, Gb_Apu::OSC_COUNT),
        ENUMS(PARAM_PW, 2),
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, Gb_Apu::OSC_COUNT),
        ENUMS(INPUT_FM, 3),
        INPUT_LFSR,
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, Gb_Apu::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds { LIGHT_COUNT };

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[Gb_Apu::OSC_COUNT];
    /// The GBS instance to synthesize sound with
    Gb_Apu apu;

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

    /// a Schmitt Trigger for handling inputs to the LFSR port
    dsp::SchmittTrigger lfsr;

    /// Initialize a new GBS Chip module.
    ChipGBS() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ + 0, -30.f, 30.f, 0.f, "Pulse 1 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -30.f, 30.f, 0.f, "Pulse 2 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2, -30.f, 30.f, 0.f, "Triangle Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 3,   0,    7,   4,   "Noise Period", "", 0, 1, -7);
        configParam(PARAM_PW + 0,     0,    3,   2,   "Pulse 1 Duty Cycle");
        configParam(PARAM_PW + 1,     0,    3,   2,   "Pulse 2 Duty Cycle");
        cvDivider.setDivision(16);
        // set the output buffer for each individual voice
        for (int i = 0; i < Gb_Apu::OSC_COUNT; i++) apu.osc_output(i, &buf[i]);
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.volume(3.f);
    }

    void channel_pulse(int channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ11BIT_MIN = 8;
        // the maximal value for the frequency register
        static constexpr float FREQ11BIT_MAX = 2035;
        // the constant modulation factor
        static constexpr auto MOD_FACTOR = 10.f;
        // set the pulse width of the pulse wave (high 2 bits)
        auto pw = static_cast<uint8_t>(params[PARAM_PW + channel].getValue()) << 6;
        apu.write_register(0, PULSE0_DUTY_LENGTH_LOAD + 5 * channel, pw);
        // set the volume of the pulse wave, high 4 bits, envelope add mode on
        apu.write_register(0, PULSE0_START_VOLUME + 5 * channel, 0b11111000);
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + channel].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + channel].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM + channel].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        uint16_t freq11bit = rack::clamp(freq, FREQ11BIT_MIN, FREQ11BIT_MAX);
        // write the frequency to the low and high registers
        // - there are 4 registers per pulse channel, multiply channel by 4 to
        //   produce an offset between registers based on channel index
        apu.write_register(0, PULSE0_FREQ_LO               + 5 * channel, freq11bit & 0xff );
        apu.write_register(0, PULSE0_TRIG_LENGTH_ENABLE_HI + 5 * channel, (freq11bit >> 8) | 0x80 );
    }

    void channel_wave() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ11BIT_MIN = 8;
        // the maximal value for the frequency register
        static constexpr float FREQ11BIT_MAX = 2035;
        // the constant modulation factor
        static constexpr auto MOD_FACTOR = 10.f;
        // turn on the DAC for the channel
        apu.write_register(0, WAVE_DAC_POWER, 0b10000000);
        // set the volume
        apu.write_register(0, WAVE_VOLUME_CODE, 0b00100000);
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + 2].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + 2].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += MOD_FACTOR * inputs[INPUT_FM + 2].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to an 11-bit value
        uint16_t freq11bit = rack::clamp(freq, FREQ11BIT_MIN, FREQ11BIT_MAX);
        // write the frequency to the low and high registers
        // - there are 4 registers per pulse channel, multiply channel by 4 to
        //   produce an offset between registers based on channel index
        apu.write_register(0, WAVE_FREQ_LO,  freq11bit & 0xff);
        apu.write_register(0, WAVE_TRIG_LENGTH_ENABLE_FREQ_HI, freq11bit >> 8 | 0x80);
        // write the wave-table for the channel
        for (int i = 0; i < 32 / 2; i++) {
            uint8_t nibbleHi = sine_wave[2 * i];
            uint8_t nibbleLo = sine_wave[2 * i + 1];
            // combine the two nibbles into a byte for the RAM
            apu.write_register(0, WAVE_TABLE_VALUES + i, (nibbleHi << 4) | nibbleLo);
        }
    }

    void channel_noise() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ_MIN = 0;
        // the maximal value for the frequency register
        static constexpr float FREQ_MAX = 7;
        // set the volume for the channel
        apu.write_register(0, NOISE_START_VOLUME, 0b11111000);
        // get the pitch / frequency of the oscillator
        auto sign = sgn(inputs[INPUT_VOCT + 3].getVoltage());
        auto pitch = abs(inputs[INPUT_VOCT + 3].getVoltage() / 100.f);
        // convert the pitch to frequency based on standard exponential scale
        auto freq = rack::dsp::FREQ_C4 * sign * (powf(2.0, pitch) - 1.f);
        freq += params[PARAM_FREQ + 3].getValue();
        uint8_t period = FREQ_MAX - rack::clamp(freq, FREQ_MIN, FREQ_MAX);
        apu.write_register(0, NOISE_CLOCK_SHIFT, lfsr.isHigh() * 0b00001000 | period);
        // enable the channel
        apu.write_register(0, NOISE_TRIG_LENGTH_ENABLE, 0x80);
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
        uint32_t cycles_per_sample = 4194304 / args.sampleRate;
        // check for sample rate changes from the engine to send to the chip
        if (new_sample_rate) {
            // update the buffer for each channel
            for (int i = 0; i < Gb_Apu::OSC_COUNT; i++)
                buf[i].set_sample_rate(args.sampleRate, 4194304);
            // clear the new sample rate flag
            new_sample_rate = false;
        }
        if (cvDivider.process()) {  // process the CV inputs to the chip
            lfsr.process(rescale(inputs[INPUT_LFSR].getVoltage(), 0.f, 2.f, 0.f, 1.f));
            // process the data on the chip
            apu.write_register(0, POWER_CONTROL_STATUS, 0b10000000);
            apu.write_register(0, STEREO_ENABLES, 0b11111111);
            apu.write_register(0, STEREO_VOLUME, 0b11111111);
            channel_pulse(0);
            channel_pulse(1);
            channel_wave();
            channel_noise();
        }
        // process audio samples on the chip engine
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < Gb_Apu::OSC_COUNT; i++)
            outputs[OUTPUT_CHANNEL + i].setVoltage(getAudioOut(i));
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override { new_sample_rate = true; }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The widget structure that lays out the panel of the module and the UI menus.
struct ChipGBSWidget : ModuleWidget {
    ChipGBSWidget(ChipGBS *module) {
        setModule(module);
        static constexpr auto panel = "res/GBS.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // V/OCT inputs
        addInput(createInput<PJ301MPort>(Vec(20, 74), module, ChipGBS::INPUT_VOCT + 0));
        addInput(createInput<PJ301MPort>(Vec(20, 159), module, ChipGBS::INPUT_VOCT + 1));
        addInput(createInput<PJ301MPort>(Vec(20, 244), module, ChipGBS::INPUT_VOCT + 2));
        addInput(createInput<PJ301MPort>(Vec(20, 329), module, ChipGBS::INPUT_VOCT + 3));
        // FM inputs
        addInput(createInput<PJ301MPort>(Vec(25, 32), module, ChipGBS::INPUT_FM + 0));
        addInput(createInput<PJ301MPort>(Vec(25, 118), module, ChipGBS::INPUT_FM + 1));
        addInput(createInput<PJ301MPort>(Vec(25, 203), module, ChipGBS::INPUT_FM + 2));
        // Frequency parameters
        addParam(createParam<Rogan3PSNES>(Vec(54, 42), module, ChipGBS::PARAM_FREQ + 0));
        addParam(createParam<Rogan3PSNES>(Vec(54, 126), module, ChipGBS::PARAM_FREQ + 1));
        addParam(createParam<Rogan3PSNES>(Vec(54, 211), module, ChipGBS::PARAM_FREQ + 2));
        addParam(createParam<Rogan3PSNES_Snap>(Vec(54, 297), module, ChipGBS::PARAM_FREQ + 3));
        // PW
        addParam(createParam<Rogan0PSNES_Snap>(Vec(102, 30), module, ChipGBS::PARAM_PW + 0));
        addParam(createParam<Rogan0PSNES_Snap>(Vec(102, 115), module, ChipGBS::PARAM_PW + 1));
        // LFSR switch
        addInput(createInput<PJ301MPort>(Vec(24, 284), module, ChipGBS::INPUT_LFSR));
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(106, 74),  module, ChipGBS::OUTPUT_CHANNEL + 0));
        addOutput(createOutput<PJ301MPort>(Vec(106, 159), module, ChipGBS::OUTPUT_CHANNEL + 1));
        addOutput(createOutput<PJ301MPort>(Vec(106, 244), module, ChipGBS::OUTPUT_CHANNEL + 2));
        addOutput(createOutput<PJ301MPort>(Vec(106, 329), module, ChipGBS::OUTPUT_CHANNEL + 3));
    }
};

/// the global instance of the model
Model *modelChipGBS = createModel<ChipGBS, ChipGBSWidget>("GBS");
