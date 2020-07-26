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
#include "dsp/nintendo_gameboy.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Nintendo GBS Chip module.
struct ChipGBS : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, NintendoGBS::OSC_COUNT),
        ENUMS(PARAM_PW, 2),
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, NintendoGBS::OSC_COUNT),
        ENUMS(INPUT_FM, 3),
        INPUT_LFSR,
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, NintendoGBS::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds { LIGHT_COUNT };

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[NintendoGBS::OSC_COUNT];
    /// The GBS instance to synthesize sound with
    NintendoGBS apu;

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
        for (unsigned i = 0; i < NintendoGBS::OSC_COUNT; i++)
            apu.set_output(i, &buf[i]);
        // volume of 3 produces a roughly 5Vpp signal from all voices
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
    /// @returns the 11 bit frequency value from the panel
    /// @details
    /// parameters for pulse wave:
    /// freq_min = 8, freq_max = 1023, clock_division = 16
    /// parameters for triangle wave:
    /// freq_min = 2, freq_max = 2047, clock_division = 32
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
        freq = (buf[channel].get_clock_rate() / (clock_division * freq));
        return rack::clamp(freq, freq_min, freq_max);
    }

    /// Get the PW for the given channel
    ///
    /// @param channel the channel to return the pulse width for
    /// @returns the pulse width value coded in an 8-bit container
    ///
    inline uint8_t getPulseWidth(int channel) {
        // the minimal value for the pulse width register
        static constexpr float PW_MIN = 0;
        // the maximal value for the pulse width register
        static constexpr float PW_MAX = 3;
        // get the pulse width from the parameter knob
        auto pwParam = params[PARAM_PW + channel].getValue();
        // get the control voltage to the pulse width with 1V/step
        auto pwCV = 0;  // TODO: inputs[INPUT_PW + channel].getVoltage() / 3.f;
        // get the 8-bit pulse width clamped within legal limits
        uint8_t pw = rack::clamp(pwParam + pwCV, PW_MIN, PW_MAX);
        return pw << 6;
    }

    /// Return the period of the noise oscillator from the panel controls.
    inline uint8_t getNoisePeriod() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ_MIN = 0;
        // the maximal value for the frequency register
        static constexpr float FREQ_MAX = 7;
        // get the pitch / frequency of the oscillator
        auto sign = sgn(inputs[INPUT_VOCT + 3].getVoltage());
        auto pitch = abs(inputs[INPUT_VOCT + 3].getVoltage() / 100.f);
        // convert the pitch to frequency based on standard exponential scale
        auto freq = rack::dsp::FREQ_C4 * sign * (powf(2.0, pitch) - 1.f);
        freq += params[PARAM_FREQ + 3].getValue();
        return FREQ_MAX - rack::clamp(freq, FREQ_MIN, FREQ_MAX);
    }

    void channel_pulse(int channel) {

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
            lfsr.process(rescale(inputs[INPUT_LFSR].getVoltage(), 0.f, 2.f, 0.f, 1.f));
            // turn on the power
            apu.write(NintendoGBS::POWER_CONTROL_STATUS, 0b10000000);
            // set the global volume
            apu.write(NintendoGBS::STEREO_ENABLES, 0b11111111);
            apu.write(NintendoGBS::STEREO_VOLUME, 0b11111111);
            // ---------------------------------------------------------------
            // pulse
            // ---------------------------------------------------------------
            for (unsigned channel = 0; channel < 2; channel++) {
                // pulse width of the pulse wave (high 2 bits)
                apu.write(NintendoGBS::PULSE0_DUTY_LENGTH_LOAD + NintendoGBS::REGS_PER_VOICE * channel, getPulseWidth(channel));
                // volume of the pulse wave, envelope add mode on
                apu.write(NintendoGBS::PULSE0_START_VOLUME + NintendoGBS::REGS_PER_VOICE * channel, 0b11111000);
                // frequency
                auto freq = getFrequency(channel, 8, 2035, 16);
                auto lo =           freq & 0b0000000011111111;
                apu.write(NintendoGBS::PULSE0_FREQ_LO               + NintendoGBS::REGS_PER_VOICE * channel, lo);
                auto hi =  0x80 | ((freq & 0b0000011100000000) >> 8);
                apu.write(NintendoGBS::PULSE0_TRIG_LENGTH_ENABLE_HI + NintendoGBS::REGS_PER_VOICE * channel, hi);
            }
            // ---------------------------------------------------------------
            // wave
            // ---------------------------------------------------------------
            // turn on the DAC for the channel
            apu.write(NintendoGBS::WAVE_DAC_POWER, 0b10000000);
            // set the volume
            apu.write(NintendoGBS::WAVE_VOLUME_CODE, 0b00100000);
            // frequency
            auto freq = getFrequency(2, 8, 2035, 16);
            auto lo =           freq & 0b0000000011111111;
            apu.write(NintendoGBS::WAVE_FREQ_LO, lo);
            auto hi =  0x80 | ((freq & 0b0000011100000000) >> 8);
            apu.write(NintendoGBS::WAVE_TRIG_LENGTH_ENABLE_FREQ_HI, hi);
            // write the wave-table for the channel
            for (int i = 0; i < 32 / 2; i++) {
                uint8_t nibbleHi = sine_wave[2 * i];
                uint8_t nibbleLo = sine_wave[2 * i + 1];
                // combine the two nibbles into a byte for the RAM
                apu.write(NintendoGBS::WAVE_TABLE_VALUES + i, (nibbleHi << 4) | nibbleLo);
            }
            // ---------------------------------------------------------------
            // noise
            // ---------------------------------------------------------------
            // set the period and LFSR
            apu.write(NintendoGBS::NOISE_CLOCK_SHIFT, lfsr.isHigh() * 0b00001000 | getNoisePeriod());
            // set the volume for the channel
            apu.write(NintendoGBS::NOISE_START_VOLUME, 0b11111000);
            // enable the channel. setting trigger resets the phase of the noise,
            // so check if it's set first
            if (apu.read(NintendoGBS::NOISE_TRIG_LENGTH_ENABLE) != 0x80)
                apu.write(NintendoGBS::NOISE_TRIG_LENGTH_ENABLE, 0x80);
        }
        // process audio samples on the chip engine
        apu.end_frame(4194304 / args.sampleRate);
        for (unsigned i = 0; i < NintendoGBS::OSC_COUNT; i++)
            outputs[OUTPUT_CHANNEL + i].setVoltage(getAudioOut(i));
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override {
        // update the buffer for each channel
        for (unsigned i = 0; i < NintendoGBS::OSC_COUNT; i++)
            buf[i].set_sample_rate(APP->engine->getSampleRate(), 4194304);
    }
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
        { auto param = createParam<Rogan3PSNES>(Vec(54, 297), module, ChipGBS::PARAM_FREQ + 3);
        param->snap = true;
        addParam(param); }
        // PW
        { auto param = createParam<Rogan0PSNES>(Vec(102, 30), module, ChipGBS::PARAM_PW + 0);
        param->snap = true;
        addParam(param); }
        { auto param = createParam<Rogan0PSNES>(Vec(102, 115), module, ChipGBS::PARAM_PW + 1);
        param->snap = true;
        addParam(param); }
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
