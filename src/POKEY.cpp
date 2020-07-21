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
        ENUMS(PARAM_PW, 2),
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, AtariPOKEY::OSC_COUNT),
        ENUMS(INPUT_FM, 3),
        INPUT_LFSR,
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, AtariPOKEY::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds { LIGHT_COUNT };

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[AtariPOKEY::OSC_COUNT];
    /// The POKEY instance to synthesize sound with
    AtariPOKEY apu;

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

    /// Initialize a new POKEY Chip module.
    ChipPOKEY() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ + 0, -30.f, 30.f, 0.f, "Pulse 1 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -30.f, 30.f, 0.f, "Pulse 2 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2, -30.f, 30.f, 0.f, "Triangle Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 3,   0,   15,   7,   "Noise Period", "", 0, 1, -15);
        configParam(PARAM_PW + 0,     0,    3,   2,   "Pulse 1 Duty Cycle");
        configParam(PARAM_PW + 1,     0,    3,   2,   "Pulse 2 Duty Cycle");
        cvDivider.setDivision(16);
        // set the output buffer for each individual voice
        for (int i = 0; i < AtariPOKEY::OSC_COUNT; i++) apu.set_output(i, &buf[i]);
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.set_volume(3.f);
    }

    /// Process pulse wave for given channel.
    ///
    /// @param channel the channel to process the pulse wave for
    ///
    void channel(int channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ8BIT_MIN = 0;
        // the maximal value for the frequency register
        static constexpr float FREQ8BIT_MAX = 0xFF;
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
        // convert the frequency to an 8-bit value
        freq = (buf[channel].get_clock_rate() / (CLOCK_DIVISION * freq)) - 1;
        uint8_t freq8bit = rack::clamp(freq, FREQ8BIT_MIN, FREQ8BIT_MAX);
        // - there are 2 registers per channel, multiply first channel by 2 to
        //   produce an offset between registers based on channel index
        apu.write(AtariPOKEY::AUDF1 + 2 * channel, freq8bit);

        // auto pw = static_cast<uint8_t>(params[PARAM_PW + channel].getValue()) << 6;
        apu.write(AtariPOKEY::AUDC1 + 2 * channel, 0b11101111);
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
            for (int i = 0; i < AtariPOKEY::OSC_COUNT; i++)
                buf[i].set_sample_rate(args.sampleRate, CLOCK_RATE);
            // clear the new sample rate flag
            new_sample_rate = false;
        }
        if (cvDivider.process()) {  // process the CV inputs to the chip
            for (int i = 0; i < AtariPOKEY::OSC_COUNT; i++) channel(i);
            apu.write(AtariPOKEY::AUDCTL, 0b00001111);
        }
        // process audio samples on the chip engine
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < AtariPOKEY::OSC_COUNT; i++)
            outputs[OUTPUT_CHANNEL + i].setVoltage(getAudioOut(i));
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override { new_sample_rate = true; }
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
        // V/OCT inputs
        addInput(createInput<PJ301MPort>(Vec(20, 74), module, ChipPOKEY::INPUT_VOCT + 0));
        addInput(createInput<PJ301MPort>(Vec(20, 159), module, ChipPOKEY::INPUT_VOCT + 1));
        addInput(createInput<PJ301MPort>(Vec(20, 244), module, ChipPOKEY::INPUT_VOCT + 2));
        addInput(createInput<PJ301MPort>(Vec(20, 329), module, ChipPOKEY::INPUT_VOCT + 3));
        // FM inputs
        addInput(createInput<PJ301MPort>(Vec(25, 32), module, ChipPOKEY::INPUT_FM + 0));
        addInput(createInput<PJ301MPort>(Vec(25, 118), module, ChipPOKEY::INPUT_FM + 1));
        addInput(createInput<PJ301MPort>(Vec(25, 203), module, ChipPOKEY::INPUT_FM + 2));
        // Frequency parameters
        addParam(createParam<Rogan3PSNES>(Vec(54, 42), module, ChipPOKEY::PARAM_FREQ + 0));
        addParam(createParam<Rogan3PSNES>(Vec(54, 126), module, ChipPOKEY::PARAM_FREQ + 1));
        addParam(createParam<Rogan3PSNES>(Vec(54, 211), module, ChipPOKEY::PARAM_FREQ + 2));
        addParam(createParam<Rogan3PSNES_Snap>(Vec(54, 297), module, ChipPOKEY::PARAM_FREQ + 3));
        // PW
        addParam(createParam<Rogan0PSNES_Snap>(Vec(102, 30), module, ChipPOKEY::PARAM_PW + 0));
        addParam(createParam<Rogan0PSNES_Snap>(Vec(102, 115), module, ChipPOKEY::PARAM_PW + 1));
        // LFSR switch
        addInput(createInput<PJ301MPort>(Vec(24, 284), module, ChipPOKEY::INPUT_LFSR));
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(106, 74),  module, ChipPOKEY::OUTPUT_CHANNEL + 0));
        addOutput(createOutput<PJ301MPort>(Vec(106, 159), module, ChipPOKEY::OUTPUT_CHANNEL + 1));
        addOutput(createOutput<PJ301MPort>(Vec(106, 244), module, ChipPOKEY::OUTPUT_CHANNEL + 2));
        addOutput(createOutput<PJ301MPort>(Vec(106, 329), module, ChipPOKEY::OUTPUT_CHANNEL + 3));
    }
};

/// the global instance of the model
Model *modelChipPOKEY = createModel<ChipPOKEY, ChipPOKEYWidget>("POKEY");
