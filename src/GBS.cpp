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

// TODO: remove (use global wavetable header)

/// the default values for the wave-table
const uint8_t sine_wave[32] = {
    0xA,0x8,0xD,0xC,0xE,0xE,0xF,0xF,0xF,0xF,0xE,0xF,0xD,0xE,0xA,0xC,
    0x5,0x8,0x2,0x3,0x1,0x1,0x0,0x0,0x0,0x0,0x1,0x0,0x2,0x1,0x5,0x3
};

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Nintendo GBS Chip module.
struct ChipGBS : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, 3),
        PARAM_NOISE_PERIOD,
        ENUMS(PARAM_PW, 2),
        PARAM_WAVETABLE,
        PARAM_LFSR,
        ENUMS(PARAM_LEVEL, NintendoGBS::OSC_COUNT),
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, 3),
        INPUT_NOISE_PERIOD,
        ENUMS(INPUT_FM, 3),
        ENUMS(INPUT_PW, 2),
        INPUT_WAVETABLE,
        INPUT_LFSR,
        ENUMS(INPUT_LEVEL, NintendoGBS::OSC_COUNT),
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, NintendoGBS::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds {
        ENUMS(LIGHTS_LEVEL, NintendoGBS::OSC_COUNT),
        LIGHT_COUNT
    };

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[NintendoGBS::OSC_COUNT];
    /// The GBS instance to synthesize sound with
    NintendoGBS apu;

    /// a Schmitt Trigger for handling inputs to the LFSR port
    dsp::SchmittTrigger lfsr;

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;

    /// a VU meter for keeping track of the channel levels
    dsp::VuMeter2 chMeters[NintendoGBS::OSC_COUNT];
    /// a clock divider for updating the mixer LEDs
    dsp::ClockDivider lightDivider;

    /// Initialize a new GBS Chip module.
    ChipGBS() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ + 0, -30.f, 30.f, 0.f, "Pulse 1 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -30.f, 30.f, 0.f, "Pulse 2 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2, -30.f, 30.f, 0.f, "Wave Frequency",    " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_NOISE_PERIOD, 0, 7, 4, "Noise Period", "", 0, 1, -7);
        configParam(PARAM_PW + 0, 0, 3, 2, "Pulse 1 Duty Cycle");
        configParam(PARAM_PW + 1, 0, 3, 2, "Pulse 2 Duty Cycle");
        configParam(PARAM_WAVETABLE, 0, 5, 0, "Wavetable morph");
        configParam(PARAM_LFSR, 0, 1, 0, "Linear Feedback Shift Register");
        configParam(PARAM_LEVEL + 0, 0.f, 1.f, 0.5f, "Pulse 1 Volume", "%", 0, 100);
        configParam(PARAM_LEVEL + 1, 0.f, 1.f, 0.5f, "Pulse 2 Volume", "%", 0, 100);
        configParam(PARAM_LEVEL + 2, 0.f, 1.f, 0.5f, "Wave Volume", "%", 0, 100);
        configParam(PARAM_LEVEL + 3, 0.f, 1.f, 0.5f, "Noise Volume", "%", 0, 100);
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
    inline uint16_t getFrequency(int channel) {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ_MIN = 8;
        // the maximal value for the frequency register
        static constexpr float FREQ_MAX = 2035;
        // the constant modulation factor
        static constexpr auto MOD_FACTOR = 10.f;
        // get the pitch from the parameter and control voltage
        float pitch = params[PARAM_FREQ + channel].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT + channel].getVoltage();
        // convert the pitch to frequency based on standard exponential scale
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        // TODO: why is the wavetable clocked at half rate? this is not
        // documented anywhere that I can find; however, it makes sense that
        // the wave channel would be an octave lower since the original
        // triangle channel was intended for bass
        if (channel == NintendoGBS::WAVETABLE) freq *= 2;
        freq += MOD_FACTOR * inputs[INPUT_FM + channel].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to an 11-bit value
        freq = 2048.f - (static_cast<uint32_t>(buf[channel].get_clock_rate() / freq) >> 5);
        return rack::clamp(freq, FREQ_MIN, FREQ_MAX);
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

    /// Return the wave-table parameter.
    ///
    /// @returns the floating index of the wave-table table in [0, 4]
    ///
    inline float getWavetable() {
        auto param = params[PARAM_WAVETABLE].getValue();
        // auto att = params[PARAM_WAVETABLE_ATT].getValue();
        // get the CV as 1V per wave-table
        auto cv = inputs[INPUT_WAVETABLE].getVoltage() / 2.f;
        // wave-tables are indexed maths style on panel, subtract 1 for CS style
        return rack::math::clamp(param + /*att * */ cv, 1.f, 5.f) - 1;
    }

    /// Return the period of the noise oscillator from the panel controls.
    inline uint8_t getNoisePeriod() {
        // the minimal value for the frequency register to produce sound
        static constexpr float FREQ_MIN = 0;
        // the maximal value for the frequency register
        static constexpr float FREQ_MAX = 7;
        // get the pitch / frequency of the oscillator
        auto sign = sgn(inputs[INPUT_NOISE_PERIOD].getVoltage());
        auto pitch = abs(inputs[INPUT_NOISE_PERIOD].getVoltage() / 100.f);
        // convert the pitch to frequency based on standard exponential scale
        auto freq = rack::dsp::FREQ_C4 * sign * (powf(2.0, pitch) - 1.f);
        freq += params[PARAM_NOISE_PERIOD].getValue();
        return FREQ_MAX - rack::clamp(freq, FREQ_MIN, FREQ_MAX);
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
            auto lfsrV = math::clamp(inputs[INPUT_LFSR].getVoltage(), 0.f, 10.f);
            lfsr.process(rescale(lfsrV, 0.f, 2.f, 0.f, 1.f));
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
                auto freq = getFrequency(channel);
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
            auto freq = getFrequency(2);
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
        apu.end_frame(CLOCK_RATE / args.sampleRate);
        for (unsigned i = 0; i < NintendoGBS::OSC_COUNT; i++)
            outputs[OUTPUT_CHANNEL + i].setVoltage(getAudioOut(i));
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override {
        // update the buffer for each channel
        for (unsigned i = 0; i < NintendoGBS::OSC_COUNT; i++)
            buf[i].set_sample_rate(APP->engine->getSampleRate(), CLOCK_RATE);
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
        for (unsigned i = 0; i < NintendoGBS::OSC_COUNT; i++) {
            if (i < NintendoGBS::NOISE) {
                addInput(createInput<PJ301MPort>(             Vec(169, 75 + 85 * i), module, ChipGBS::INPUT_VOCT     + i));
                addInput(createInput<PJ301MPort>(             Vec(169, 26 + 85 * i), module, ChipGBS::INPUT_FM       + i));
                addParam(createParam<BefacoBigKnob>(          Vec(202, 25 + 85 * i), module, ChipGBS::PARAM_FREQ     + i));
                auto param = createParam<RoundSmallBlackKnob>(Vec(289, 38 + 85 * i), module, ChipGBS::PARAM_PW       + i);
                if (i < NintendoGBS::WAVETABLE) param->snap = true;
                addParam(param);
                addInput(createInput<PJ301MPort>(             Vec(288, 73 + 85 * i), module, ChipGBS::INPUT_PW       + i));
            }
            addParam(createLightParam<LEDLightSlider<GreenLight>>(Vec(316, 24 + 85 * i),  module, ChipGBS::PARAM_LEVEL + i, ChipGBS::LIGHTS_LEVEL + i));
            addInput(createInput<PJ301MPort>(                 Vec(346, 26 + 85 * i), module, ChipGBS::INPUT_LEVEL + i));
            addOutput(createOutput<PJ301MPort>(               Vec(346, 74 + 85 * i), module, ChipGBS::OUTPUT_CHANNEL + i));
        }
        // noise period
        auto param = createParam<Rogan3PWhite>(Vec(202, 298), module, ChipGBS::PARAM_NOISE_PERIOD);
        param->snap = true;
        addParam(param);
        addInput(createInput<PJ301MPort>(Vec(169, 329), module, ChipGBS::INPUT_NOISE_PERIOD));
        // LFSR switch
        addParam(createParam<CKSS>(Vec(280, 281), module, ChipGBS::PARAM_LFSR));
        addInput(createInput<PJ301MPort>(Vec(258, 329), module, ChipGBS::INPUT_LFSR));
    }
};

/// the global instance of the model
Model *modelChipGBS = createModel<ChipGBS, ChipGBSWidget>("GBS");
