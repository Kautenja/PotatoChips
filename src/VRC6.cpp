// A VRC6 Chip module.
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
#include "2A03/Nes_Vrc6.h"

// /// the IO registers on the APU
// enum IORegisters {
//     SQ1_VOL =     0x4000,
//     SQ1_SWEEP =   0x4001,
//     SQ1_LO =      0x4002,
//     SQ1_HI =      0x4003,
//     SQ2_VOL =     0x4004,
//     SQ2_SWEEP =   0x4005,
//     SQ2_LO =      0x4006,
//     SQ2_HI =      0x4007,
//     TRI_LINEAR =  0x4008,
//     // APU_UNUSED1 = 0x4009,  // may be used for memory clearing loops
//     TRI_LO =      0x400A,
//     TRI_HI =      0x400B,
//     NOISE_VOL =   0x400C,
//     // APU_UNUSED2 = 0x400D,  // may be used for memory clearing loops
//     NOISE_LO =    0x400E,
//     NOISE_HI =    0x400F,
//     DMC_FREQ =    0x4010,
//     DMC_RAW =     0x4011,
//     DMC_START =   0x4012,
//     DMC_LEN =     0x4013,
//     SND_CHN =     0x4015,
//     // JOY1 =        0x4016,  // unused for APU
//     JOY2 =        0x4017,
// };

// /// The pulse width modes available
// enum class PulseWidth : uint8_t {
//     TwelveHalf  = 0b00000000,  // 12.5%
//     TwentyFive  = 0b01000000,  // 25%
//     Fifty       = 0b10000000,  // 50%
//     SeventyFive = 0b11000000   // 75%
// };

// /// Return the sum of a pulse width flag with an input value flag.
// ///
// /// @param a the pulse width value flag to add to the existing flags
// /// @param b a byte that represents flags where bits 'B' are used: 0bAABBBBBB
// /// @returns the sum of a and b, i.e., the flag: 0bAABBBBBB
// ///
// inline uint8_t operator+(const PulseWidth& a, const uint8_t& b) {
//     return static_cast<uint8_t>(a) + b;
// }

// /// Return the sum of a pulse width flag with an input value flag.
// ///
// /// @param a the pulse width value flag to add to the existing flags
// /// @param b a byte that represents flags where bits 'B' are used: 0bAABBBBBB
// /// @returns the sum of a and b, i.e., the flag: 0bAABBBBBB
// ///
// inline PulseWidth next(PulseWidth a) {
//     auto b = ((static_cast<uint8_t>(a) >> 6) + 1) % 4;
//     a = static_cast<PulseWidth>(b << 6);
//     return a;
// }

// /// TODO:
// inline uint8_t operator>>(const PulseWidth& a, const uint8_t& b) {
//     return static_cast<uint8_t>(a) >> b;
// }

// /// The channels send flag bits.
// enum class SendChannels : uint8_t {
//     Square1  = 0b00000001,
//     Square2  = 0b00000010,
//     Triangle = 0b00000100,
//     Noise    = 0b00001000,
//     All      = 0b00001111
// };

// /// Return the sum of a send channel flag with an input value flag.
// ///
// /// @param a the send channel value flag to add to the existing flags
// /// @param b a byte that represents flags where bits 'B' are used: 0bAAAABBBB
// /// @returns the sum of a and b, i.e., the flag: 0bAAAABBBB
// ///
// inline uint8_t operator+(SendChannels a, uint8_t b) {
//     return static_cast<uint8_t>(a) + b;
// }

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A VRC6 Chip module.
struct ChipVRC6 : Module {
    enum ParamIds {
        PARAM_FREQ0,
        PARAM_FREQ1,
        PARAM_FREQ2,
        PARAM_FREQ3,
        PARAM_PW1,
        PARAM_PW2,
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
        OUTPUT_MIX,
        OUTPUT_COUNT
    };
    enum LightIds {
        ENUMS(LIGHT_PW1, 4),
        ENUMS(LIGHT_PW2, 4),
        LIGHT_COUNT
    };

    /// the clock rate of the module
    static constexpr uint64_t CLOCK_RATE = 800000;

    /// The BLIP buffer to render audio samples from
    Blip_Buffer buf[3];
    /// The VRC6 instance to synthesize sound with
    Nes_Vrc6 apu;

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    /// a clock divider for updating the LEDs slower than audio rate
    dsp::ClockDivider lightDivider;

    /// Initialize a new VRC6 Chip module.
    ChipVRC6() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ0, -30.f, 30.f, 0.f, "Square 1 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ1, -30.f, 30.f, 0.f, "Square 2 Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_FREQ2, -30.f, 30.f, 0.f, "Saw      Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(PARAM_PW1,   0.0,    1.0, 0.0, "Square 1 Pulse Width");
        configParam(PARAM_PW2,   0.0,    1.0, 0.0, "Square 2 Pulse Width");
        // set the output buffer for each individual voice
        for (int i = 0; i < 3; i++) apu.osc_output(i, &buf[i]);
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.volume(3.f);
        // set the number of samples between LED updates
        lightDivider.setDivision(32);
    }

    // /// Process square wave (channel 1).
    // void square1() {
    //     float pitch = params[PARAM_FREQ0].getValue() / 12.f;
    //     pitch += inputs[INPUT_VOCT0].getVoltage();
    //     float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
    //     freq = rack::clamp(freq, 0.0f, 20000.0f);
    //     uint16_t freq11bit = (CLOCK_RATE / (16 * freq)) - 1;
    //     freq11bit += inputs[INPUT_FM0].getVoltage();
    //     freq11bit = rack::clamp(freq11bit, 8, 1023);
    //     uint8_t sq_hi = (freq11bit & 0b0000011100000000) >> 8;
    //     uint8_t sq_lo = freq11bit & 0b11111111;
    //     apu.write_register(0, SQ1_VOL, pw1 + 0b00011111);
    //     apu.write_register(0, SQ1_LO, sq_lo);
    //     apu.write_register(0, SQ1_HI, sq_hi);
    // }

    // /// Process square wave (channel 2).
    // void square2() {
    //     float pitch = params[PARAM_FREQ1].getValue() / 12.f;
    //     pitch += inputs[INPUT_VOCT1].getVoltage();
    //     float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
    //     freq = rack::clamp(freq, 0.0f, 20000.0f);
    //     uint16_t freq11bit = (CLOCK_RATE / (16 * freq)) - 1;
    //     freq11bit += inputs[INPUT_FM1].getVoltage();
    //     freq11bit = rack::clamp(freq11bit, 8, 1023);
    //     uint8_t sq_hi = (freq11bit & 0b0000011100000000) >> 8;
    //     uint8_t sq_lo = freq11bit & 0b11111111;
    //     apu.write_register(0, SQ2_VOL, pw2 + 0b00011111);
    //     apu.write_register(0, SQ2_LO, sq_lo);
    //     apu.write_register(0, SQ2_HI, sq_hi);
    // }

    // /// Process triangle wave (channel 3).
    // void triangle() {
    //     float pitch = params[PARAM_FREQ2].getValue() / 12.f;
    //     pitch += inputs[INPUT_VOCT2].getVoltage();
    //     float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
    //     freq = rack::clamp(freq, 0.0f, 20000.0f);
    //     uint16_t freq11bit = (CLOCK_RATE / (32 * freq)) - 1;
    //     freq11bit += inputs[INPUT_FM2].getVoltage();
    //     freq11bit = rack::clamp(freq11bit, 2, 2047);
    //     uint8_t tri_hi = (freq11bit & 0b0000011100000000) >> 8;
    //     uint8_t tri_lo = freq11bit & 0b11111111;
    //     apu.write_register(0, TRI_LINEAR, 0b01111111);
    //     apu.write_register(0, TRI_LO, tri_lo);
    //     apu.write_register(0, TRI_HI, tri_hi);
    // }

    /// Return a 10V signed sample from the APU.
    ///
    /// @param channel the channel to get the audio sample for
    ///
    float getAudioOut(int channel) {
        auto samples = buf[channel].samples_avail();
        if (samples == 0) return 0.f;
        // copy the buffer to  a local vector and return the first sample
        std::vector<int16_t> output_buffer(samples);
        buf[channel].read_samples(&output_buffer[0], samples);
        return 10.f * output_buffer[0] / static_cast<float>(1 << 15);;
    }

    // /// Set the lights on the module
    // ///
    // /// @param deltaTime the amount of time between LED update samples
    // ///
    // void setLights(float deltaTime) {
    //     auto pw1index = pw1 >> 6;
    //     for (int i = 0; i < 4; i++)
    //         lights[LIGHT_PW1 + i].setSmoothBrightness(i == pw1index, deltaTime);
    //     auto pw2index = pw2 >> 6;
    //     for (int i = 0; i < 4; i++)
    //         lights[LIGHT_PW2 + i].setSmoothBrightness(i == pw2index, deltaTime);
    // }

    /// Process a sample.
    void process(const ProcessArgs &args) override {
        // calculate the number of clock cycles on the chip per audio sample
        uint32_t cycles_per_sample = CLOCK_RATE / args.sampleRate;
        // check for sample rate changes from the engine to send to the chip
        if (new_sample_rate) {
            // update the buffer for each channel
            for (int i = 0; i < 3; i++) {
                buf[i].sample_rate(args.sampleRate);
                buf[i].clock_rate(cycles_per_sample * args.sampleRate);
                buf[i].clear();
            }
            // clear the new sample rate flag
            new_sample_rate = false;
        }
        // // process the data on the chip
        // square1(); square2(); triangle();
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < 3; i++) buf[i].end_frame(cycles_per_sample);
        // // set the output from the oscillators
        outputs[OUTPUT_CHANNEL0].setVoltage(getAudioOut(0));
        outputs[OUTPUT_CHANNEL1].setVoltage(getAudioOut(1));
        outputs[OUTPUT_CHANNEL2].setVoltage(getAudioOut(2));
        // // set the lights
        // if (lightDivider.process())
        //     setLights(lightDivider.getDivision() * args.sampleTime);
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override { new_sample_rate = true; }

    // /// Respond to the user resetting the module from the front end.
    // inline void onReset() override { pw1 = pw2 = PulseWidth::Fifty; }

    // /// Respond to the randomization of the module parameters.
    // inline void onRandomize() override {
    //     pw1 = static_cast<PulseWidth>((random::u32() % 4) << 6);
    //     pw2 = static_cast<PulseWidth>((random::u32() % 4) << 6);
    // }

    // /// Convert the module's state to a JSON object
    // inline json_t* dataToJson() override {
    //     json_t* root = json_object();
    //     json_object_set_new(root, "pw1", json_integer(static_cast<uint8_t>(pw1)));
    //     json_object_set_new(root, "pw2", json_integer(static_cast<uint8_t>(pw2)));
    //     return root;
    // }

    // /// Load the module's state from a JSON object
    // inline void dataFromJson(json_t* root) override {
    //     json_t* pw1Data = json_object_get(root, "pw1");
    //     if (pw1Data) pw1 = static_cast<PulseWidth>(json_integer_value(pw1Data));
    //     json_t* pw2Data = json_object_get(root, "pw2");
    //     if (pw2Data) pw2 = static_cast<PulseWidth>(json_integer_value(pw2Data));
    // }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// string labels for the square wave PW values
static const char* PWLabels[4] = {"12.5%", "25%", "50%", "75%"};

/// The widget structure that lays out the panel of the module and the UI menus.
struct ChipVRC6Widget : ModuleWidget {
    ChipVRC6Widget(ChipVRC6 *module) {
        setModule(module);
        static const auto panel = "res/VRC6.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // V/OCT inputs
        addInput(createInput<PJ301MPort>(Vec(28, 74), module, ChipVRC6::INPUT_VOCT0));
        addInput(createInput<PJ301MPort>(Vec(28, 159), module, ChipVRC6::INPUT_VOCT1));
        addInput(createInput<PJ301MPort>(Vec(28, 244), module, ChipVRC6::INPUT_VOCT2));
        addInput(createInput<PJ301MPort>(Vec(28, 329), module, ChipVRC6::INPUT_VOCT3));
        // FM inputs
        addInput(createInput<PJ301MPort>(Vec(33, 32), module, ChipVRC6::INPUT_FM0));
        addInput(createInput<PJ301MPort>(Vec(33, 118), module, ChipVRC6::INPUT_FM1));
        addInput(createInput<PJ301MPort>(Vec(33, 203), module, ChipVRC6::INPUT_FM2));
        // Frequency parameters
        addParam(createParam<Rogan3PSNES>(Vec(62, 42), module, ChipVRC6::PARAM_FREQ0));
        addParam(createParam<Rogan3PSNES>(Vec(62, 126), module, ChipVRC6::PARAM_FREQ1));
        addParam(createParam<Rogan3PSNES>(Vec(62, 211), module, ChipVRC6::PARAM_FREQ2));
        addParam(createParam<Rogan3PSNES_Snap>(Vec(62, 297), module, ChipVRC6::PARAM_FREQ3));
        // square 1 PW
        addChild(createLight<SmallLight<GreenLight>>(Vec(114, 49), module, ChipVRC6::LIGHT_PW1 + 0));
        addChild(createLight<SmallLight<GreenLight>>(Vec(121, 49), module, ChipVRC6::LIGHT_PW1 + 1));
        addChild(createLight<SmallLight<GreenLight>>(Vec(128, 49), module, ChipVRC6::LIGHT_PW1 + 2));
        addChild(createLight<SmallLight<GreenLight>>(Vec(135, 49), module, ChipVRC6::LIGHT_PW1 + 3));
        addParam(createParam<TL1105>(Vec(115, 30), module, ChipVRC6::PARAM_PW1));
        // square 2 PW
        addChild(createLight<SmallLight<GreenLight>>(Vec(114, 133), module, ChipVRC6::LIGHT_PW2 + 0));
        addChild(createLight<SmallLight<GreenLight>>(Vec(121, 133), module, ChipVRC6::LIGHT_PW2 + 1));
        addChild(createLight<SmallLight<GreenLight>>(Vec(128, 133), module, ChipVRC6::LIGHT_PW2 + 2));
        addChild(createLight<SmallLight<GreenLight>>(Vec(135, 133), module, ChipVRC6::LIGHT_PW2 + 3));
        addParam(createParam<TL1105>(Vec(115, 115), module, ChipVRC6::PARAM_PW2));
        // LFSR switch
        addInput(createInput<PJ301MPort>(Vec(32, 284), module, ChipVRC6::INPUT_LFSR));
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(114, 74), module, ChipVRC6::OUTPUT_CHANNEL0));
        addOutput(createOutput<PJ301MPort>(Vec(114, 159), module, ChipVRC6::OUTPUT_CHANNEL1));
        addOutput(createOutput<PJ301MPort>(Vec(114, 244), module, ChipVRC6::OUTPUT_CHANNEL2));
        addOutput(createOutput<PJ301MPort>(Vec(114, 329), module, ChipVRC6::OUTPUT_CHANNEL3));
    }

    // /// A menu item for controlling the oscillator shape.
    // struct PWItem : MenuItem {
    //     /// the pulse width on the module to set when actions occur on this item
    //     PulseWidth* pw;
    //     /// the pulse width on this item
    //     PulseWidth menuPW = PulseWidth::Fifty;
    //     /// Respond to an action on the menu item.
    //     inline void onAction(const event::Action &e) override {
    //         (*pw) = menuPW;
    //     }
    // };

    // /// Create a waveform menu for the waveform selection buttons.
    // ///
    // /// @param menu the menu to add the item to
    // /// @param labels the labels of the model selections
    // /// @param pw the pulse width value to set
    // ///
    // void create_waveform_menu(Menu *menu, const char* labels[4], PulseWidth* pw) {
    //     // iterate over the 4 pulse width options for the waveform
    //     for (int i = 0; i < 4; i++) {
    //         auto modelItem = createMenuItem<PWItem>(
    //             labels[i], CHECKMARK(static_cast<uint8_t>(*pw) == i << 6)
    //         );
    //         modelItem->pw = pw;
    //         modelItem->menuPW = static_cast<PulseWidth>(i << 6);
    //         menu->addChild(modelItem);
    //     }
    // }

    // /// Setup the context menus for the module
    // void appendContextMenu(Menu *menu) override {
    //     ChipVRC6 *module = dynamic_cast<ChipVRC6*>(this->module);
    //     assert(module);
    //     // the PW for oscillator 1
    //     menu->addChild(new MenuSeparator);
    //     menu->addChild(createMenuLabel("Square 1 Pulse Width"));
    //     create_waveform_menu(menu, PWLabels, &module->pw1);
    //     // the PW for oscillator 2
    //     menu->addChild(new MenuSeparator);
    //     menu->addChild(createMenuLabel("Square 2 Pulse Width"));
    //     create_waveform_menu(menu, PWLabels, &module->pw2);
    // }
};

/// the global instance of the model
Model *modelChipVRC6 = createModel<ChipVRC6, ChipVRC6Widget>("VRC6");
