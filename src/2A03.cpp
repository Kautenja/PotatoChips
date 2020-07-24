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
#include "dsp/ricoh_2a03.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Ricoh 2A03 Chip module.
struct Chip2A03 : Module {
    enum ParamIds {
        ENUMS(PARAM_FREQ, Ricoh2A03::OSC_COUNT),
        ENUMS(PARAM_PW, 2),
        ENUMS(PARAM_VOLUME, 3),
        PARAM_COUNT
    };
    enum InputIds {
        ENUMS(INPUT_VOCT, Ricoh2A03::OSC_COUNT),
        ENUMS(INPUT_FM, 3),
        ENUMS(INPUT_VOLUME, 3),
        ENUMS(INPUT_PW, 2),
        INPUT_LFSR,
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, Ricoh2A03::OSC_COUNT),
        OUTPUT_COUNT
    };
    enum LightIds {
        ENUMS(LIGHTS_VOLUME, 3),
        LIGHT_COUNT
    };

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[Ricoh2A03::OSC_COUNT];
    /// The 2A03 instance to synthesize sound with
    Ricoh2A03 apu;

    /// a Schmitt Trigger for handling inputs to the LFSR port
    dsp::SchmittTrigger lfsr;

    // a clock divider for running CV acquisition slower than audio rate
    dsp::ClockDivider cvDivider;
    /// a VU meter for keeping track of the channel levels
    dsp::VuMeter2 chMeters[Ricoh2A03::OSC_COUNT];
    /// a clock divider for updating the mixer LEDs
    dsp::ClockDivider lightDivider;

    /// Initialize a new 2A03 Chip module.
    Chip2A03() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ + 0, -30.f, 30.f, 0.f,   "Pulse 1 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 1, -30.f, 30.f, 0.f,   "Pulse 2 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 2, -30.f, 30.f, 0.f,   "Triangle Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ + 3,   0,   15,   7,     "Noise Period", "", 0, 1, -15);
        configParam(PARAM_PW + 0,     0,    3,   2,     "Pulse 1 Duty Cycle");
        configParam(PARAM_PW + 1,     0,    3,   2,     "Pulse 2 Duty Cycle");
        configParam(PARAM_VOLUME + 0,  0.f,  1.f, 0.9f, "Pulse 1 Volume", "%", 0.f, 100.f);
        configParam(PARAM_VOLUME + 1,  0.f,  1.f, 0.9f, "Pulse 2 Volume", "%", 0.f, 100.f);
        configParam(PARAM_VOLUME + 2,  0.f,  1.f, 0.9f, "Noise Volume",  "%", 0.f, 100.f);
        cvDivider.setDivision(16);
        lightDivider.setDivision(512);
        // set the output buffer for each individual voice
        for (unsigned i = 0; i < Ricoh2A03::OSC_COUNT; i++)
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
        freq = (buf[channel].get_clock_rate() / (clock_division * freq)) - 1;
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
        auto pwCV = inputs[INPUT_PW + channel].getVoltage() / 3.f;
        // get the 8-bit pulse width clamped within legal limits
        uint8_t pw = rack::clamp(pwParam + pwCV, PW_MIN, PW_MAX);
        return pw << 6;
    }

    /// Return the period of the noise oscillator from the panel controls.
    inline uint8_t getNoisePeriod() {
        // the minimal value for the frequency register to produce sound
        static constexpr float PERIOD_MIN = 0;
        // the maximal value for the frequency register
        static constexpr float PERIOD_MAX = 15;
        // get the pitch / frequency of the oscillator
        auto sign = sgn(inputs[INPUT_VOCT + 3].getVoltage());
        auto pitch = abs(inputs[INPUT_VOCT + 3].getVoltage() / 100.f);
        // convert the pitch to frequency based on standard exponential scale
        auto freq = rack::dsp::FREQ_C4 * sign * (powf(2.0, pitch) - 1.f);
        freq += params[PARAM_FREQ + 3].getValue();
        return PERIOD_MAX - rack::clamp(freq, PERIOD_MIN, PERIOD_MAX);
    }

    /// Return the volume level from the panel controls.
    ///
    /// @param channel the channel to return the volume level of
    /// @returns the volume level of the given channel
    /// @details
    /// channel can be one of 0, 1, or 3. the triangle channel (2) has no
    /// volume control.
    ///
    inline uint8_t getVolume(int channel) {
        // the minimal value for the volume width register
        static constexpr float VOLUME_MIN = 0;
        // the maximal value for the volume width register
        static constexpr float VOLUME_MAX = 15;
        // decrement the noise channel
        if (channel == 3) channel -= 1;
        // get the attenuation from the parameter knob
        auto param = params[PARAM_VOLUME + channel].getValue();
        // apply the control voltage to the attenuation
        if (inputs[INPUT_VOLUME + channel].isConnected()) {
            auto cv = inputs[INPUT_VOLUME + channel].getVoltage() / 10.f;
            cv = rack::clamp(cv, 0.f, 1.f);
            cv = roundf(100.f * cv) / 100.f;
            param *= 2 * cv;
        }
        // get the 8-bit volume clamped within legal limits
        return rack::clamp(VOLUME_MAX * param, VOLUME_MIN, VOLUME_MAX);
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
            lfsr.process(rescale(inputs[INPUT_LFSR].getVoltage(), 0.f, 2.f, 0.f, 1.f));
            // ---------------------------------------------------------------
            // pulse channels (2)
            // ---------------------------------------------------------------
            for (int i = 0; i < 2; i++) {
                // set the pulse width of the pulse wave (high 3 bits) and set
                // the volume (low 4 bits). the 5th bit controls the envelope,
                // high sets constant volume.
                auto volume = getPulseWidth(i) | 0b00010000 | getVolume(i);
                apu.write(Ricoh2A03::PULSE0_VOL + 4 * i, volume);
                // write the frequency to the low and high registers
                // - there are 4 registers per pulse channel, multiply channel by 4 to
                //   produce an offset between registers based on channel index
                uint16_t freq = getFrequency(i, 8, 1023, 16);
                auto lo =  freq & 0b0000000011111111;
                apu.write(Ricoh2A03::PULSE0_LO + 4 * i, lo);
                auto hi = (freq & 0b0000011100000000) >> 8;
                apu.write(Ricoh2A03::PULSE0_HI + 4 * i, hi);
            }
            // ---------------------------------------------------------------
            // triangle channel
            // ---------------------------------------------------------------
            // write the frequency to the low and high registers
            uint16_t freq = getFrequency(2, 2, 2047, 32);
            apu.write(Ricoh2A03::TRIANGLE_LO,  freq & 0b0000000011111111);
            apu.write(Ricoh2A03::TRIANGLE_HI, (freq & 0b0000011100000000) >> 8);
            // write the linear register to enable the oscillator
            apu.write(Ricoh2A03::TRIANGLE_LINEAR, 0b01111111);
            // ---------------------------------------------------------------
            // noise channel
            // ---------------------------------------------------------------
            apu.write(Ricoh2A03::NOISE_LO, (lfsr.isHigh() << 7) | getNoisePeriod());
            apu.write(Ricoh2A03::NOISE_HI, 0);
            apu.write(Ricoh2A03::NOISE_VOL, 0b00010000 | getVolume(3));
            // enable all four channels
            apu.write(Ricoh2A03::SND_CHN, 0b00001111);
        }
        // process audio samples on the chip engine
        apu.end_frame(CLOCK_RATE / args.sampleRate);
        for (unsigned i = 0; i < Ricoh2A03::OSC_COUNT; i++) {
            auto channelOutput = getAudioOut(i);
            chMeters[i].process(args.sampleTime, channelOutput / 5.f);
            outputs[OUTPUT_CHANNEL + i].setVoltage(channelOutput);
        }
        if (lightDivider.process()) {  // update the mixer lights
            for (unsigned i = 0; i < Ricoh2A03::OSC_COUNT; i++) {
                float b = chMeters[i].getBrightness(-24.f, 0.f);
                lights[LIGHTS_VOLUME + i].setBrightness(b);
            }
        }
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override {
        // update the buffer for each channel
        for (unsigned i = 0; i < Ricoh2A03::OSC_COUNT; i++)
            buf[i].set_sample_rate(APP->engine->getSampleRate(), CLOCK_RATE);
    }
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
        for (unsigned i = 0; i < Ricoh2A03::OSC_COUNT; i++) {
            addInput(createInput<PJ301MPort>(Vec(19, 75 + i * 85),  module, Chip2A03::INPUT_VOCT + i));
            addOutput(createOutput<PJ301MPort>(Vec(166, 74 + i * 85),  module, Chip2A03::OUTPUT_CHANNEL + i));
            if (i < 3) {  // pulse0, pulse1, triangle
                addInput(createInput<PJ301MPort>(Vec(19, 26 + i * 85),  module, Chip2A03::INPUT_FM + i));
                addParam(createParam<BefacoBigKnob>(Vec(52, 25 + i * 85),  module, Chip2A03::PARAM_FREQ + i));
                auto y = i == 2 ? 3 : i;
                addParam(createLightParam<LEDLightSlider<GreenLight>>(Vec(136, 23 + y * 85),  module, Chip2A03::PARAM_VOLUME + i, Chip2A03::LIGHTS_VOLUME + i));
                addInput(createInput<PJ301MPort>(Vec(166, 26 + y * 85),  module, Chip2A03::INPUT_VOLUME + i));
            } else {  // noise
                addParam(createParam<Rogan2PWhite>( Vec(53, 298), module, Chip2A03::PARAM_FREQ + i));
            }
        }
        // PW 0
        auto pw0 = createParam<RoundSmallBlackKnob>(Vec(167, 205), module, Chip2A03::PARAM_PW + 0);
        pw0->snap = true;
        addParam(pw0);
        addInput(createInput<PJ301MPort>(Vec(134, 206),  module, Chip2A03::INPUT_PW + 0));
        // PW 1
        auto pw1 = createParam<RoundSmallBlackKnob>(Vec(107, 293), module, Chip2A03::PARAM_PW + 1);
        pw1->snap = true;
        addParam(pw1);
        addInput(createInput<PJ301MPort>(Vec(106, 328),  module, Chip2A03::INPUT_PW + 1));
        // LFSR switch
        addInput(createInput<PJ301MPort>(Vec(24, 284), module, Chip2A03::INPUT_LFSR));
    }
};

/// the global instance of the model
Model *modelChip2A03 = createModel<Chip2A03, Chip2A03Widget>("2A03");
