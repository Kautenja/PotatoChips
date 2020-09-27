// A Sony SPC700 chip (from Nintendo SNES) emulator module.
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
#include "componentlibrary.hpp"
#include "dsp/wavetable4bit.hpp"
#include "dsp/snes_echo.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Sony S-DSP chip (from Nintendo SNES) emulator module.
struct ChipSNES_Echo : Module {
 private:
    /// the RAM for the S-DSP chip (64KB = 16-bit address space)
    uint8_t ram[Sony_S_DSP_Echo::SIZE_OF_RAM];
    /// the Sony S-DSP sound chip emulator
    Sony_S_DSP_Echo apu{ram};

    /// @brief Fill the RAM with 0's.
    inline void clearRAM() { memset(ram, 0, sizeof ram); }

    /// triggers for handling gate inputs for the voices
    rack::dsp::BooleanTrigger gateTriggers[Sony_S_DSP_Echo::VOICE_COUNT][2];

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ,          Sony_S_DSP_Echo::VOICE_COUNT),  // TODO: remove
        ENUMS(PARAM_PM_ENABLE,     Sony_S_DSP_Echo::VOICE_COUNT),  // TODO: remove
        ENUMS(PARAM_NOISE_ENABLE,  Sony_S_DSP_Echo::VOICE_COUNT),  // TODO: remove
        PARAM_NOISE_FREQ,                                          // TODO: remove
        ENUMS(PARAM_VOLUME_L,      Sony_S_DSP_Echo::VOICE_COUNT),  // TODO: remove
        ENUMS(PARAM_VOLUME_R,      Sony_S_DSP_Echo::VOICE_COUNT),  // TODO: remove
        ENUMS(PARAM_ATTACK,        Sony_S_DSP_Echo::VOICE_COUNT),  // TODO: remove
        ENUMS(PARAM_DECAY,         Sony_S_DSP_Echo::VOICE_COUNT),  // TODO: remove
        ENUMS(PARAM_SUSTAIN_LEVEL, Sony_S_DSP_Echo::VOICE_COUNT),  // TODO: remove
        ENUMS(PARAM_SUSTAIN_RATE,  Sony_S_DSP_Echo::VOICE_COUNT),  // TODO: remove
        ENUMS(PARAM_ECHO_ENABLE,   Sony_S_DSP_Echo::VOICE_COUNT),  // TODO: remove
        PARAM_ECHO_DELAY,
        PARAM_ECHO_FEEDBACK,
        ENUMS(PARAM_VOLUME_ECHO, 2),  // TODO: remove
        ENUMS(PARAM_VOLUME_MAIN, 2),  // TODO: remove
        ENUMS(PARAM_FIR_COEFFICIENT, Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT,          Sony_S_DSP_Echo::VOICE_COUNT),
        ENUMS(INPUT_FM,            Sony_S_DSP_Echo::VOICE_COUNT),
        ENUMS(INPUT_PM_ENABLE,     Sony_S_DSP_Echo::VOICE_COUNT),
        ENUMS(INPUT_NOISE_ENABLE,  Sony_S_DSP_Echo::VOICE_COUNT),
        INPUT_NOISE_FM,
        ENUMS(INPUT_GATE,          Sony_S_DSP_Echo::VOICE_COUNT),
        ENUMS(INPUT_VOLUME_L,      Sony_S_DSP_Echo::VOICE_COUNT),
        ENUMS(INPUT_VOLUME_R,      Sony_S_DSP_Echo::VOICE_COUNT),
        ENUMS(INPUT_ATTACK,        Sony_S_DSP_Echo::VOICE_COUNT),
        ENUMS(INPUT_DECAY,         Sony_S_DSP_Echo::VOICE_COUNT),
        ENUMS(INPUT_SUSTAIN_LEVEL, Sony_S_DSP_Echo::VOICE_COUNT),
        ENUMS(INPUT_SUSTAIN_RATE,  Sony_S_DSP_Echo::VOICE_COUNT),
        ENUMS(INPUT_ECHO_ENABLE,   Sony_S_DSP_Echo::VOICE_COUNT),
        INPUT_ECHO_DELAY,
        INPUT_ECHO_FEEDBACK,
        ENUMS(INPUT_VOLUME_ECHO, 2),
        ENUMS(INPUT_VOLUME_MAIN, 2),
        ENUMS(INPUT_FIR_COEFFICIENT, Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT),
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        ENUMS(OUTPUT_AUDIO, 2),
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        NUM_LIGHTS
    };

    /// @brief Initialize a new S-DSP Chip module.
    ChipSNES_Echo() {
        // setup parameters
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned coeff = 0; coeff < Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT; coeff++) {
            // the first FIR coefficient defaults to 0x7f = 127 and the other
            // coefficients are 0 by default
            configParam(PARAM_FIR_COEFFICIENT  + coeff, -128, 127, (coeff ? 0 : 127), "FIR Coefficient " + std::to_string(coeff + 1));
        }
        configParam(PARAM_ECHO_DELAY,         0,  15,   0, "Echo Delay", "ms", 0, 16);
        configParam(PARAM_ECHO_FEEDBACK,   -128, 127,   0, "Echo Feedback");
        // clear the shared RAM between the CPU and the S-DSP
        clearRAM();
        // reset the S-DSP emulator
        apu.reset();
        // set the initial state for registers and RAM
        setupSourceDirectory();
    }

 protected:
    /// Setup the register initial state on the chip.
    inline void setupSourceDirectory() {
        // Echo data start address.
        //
        // ESA
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $6D   |                  Offset value                 |
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // This register points to an area of memory to be used by the echo
        // buffer. Like DIR its value is multiplied by 0x100. This is because
        // the echo buffer is stereo and contains a tuple of L+R 16-bit
        // samples (32-bits).

        apu.write(Sony_S_DSP_Echo::ECHO_BUFFER_START_OFFSET, 128);
        // The amount of memory required is EDL * 2KBytes (MAX $7800 bytes).
        const auto ECHO_LENGTH = 15 * (2 * (1 << 10));

        // Source Directory Offset.
        //
        // DIR
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $5D   |                  Offset value                 |
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // This register points to the source(sample) directory in external
        // RAM. The pointer is calculated by Offset*0x100. This is because each
        // directory is 4-bytes (0x100).
        //
        // The source directory contains sample start and loop point offsets.
        // Its a simple array of 16-bit values.
        //
        // SAMPLE DIRECTORY
        //
        // OFFSET  SIZE    DESC
        // dir+0   16-BIT  SAMPLE-0 START
        // dir+2   16-BIT  SAMPLE-0 LOOP START
        // dir+4   16-BIT  SAMPLE-1 START
        // dir+6   16-BIT  SAMPLE-1 LOOP START
        // dir+8   16-BIT  SAMPLE-2 START
        // ...
        // This can continue for up to 256 samples. (SRCN can only reference
        // 256 samples)

        // put the first directory at the end of the echo buffer
        apu.write(Sony_S_DSP_Echo::OFFSET_SOURCE_DIRECTORY, ECHO_LENGTH / 0x100);

        for (unsigned voice = 0; voice < Sony_S_DSP_Echo::VOICE_COUNT; voice++) {
            // shift the voice index over a nibble to get the bit mask for the
            // logical OR operator
            auto mask = voice << 4;

            // Source number is a reference to the "Source Directory" (see DIR).
            // The DSP will use the sample with this index from the directory.
            // I'm not sure what happens when you change the SRCN when the
            // channel is active, but it probably doesn't have any effect
            // until KON is set.
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x4   |                 Source Number                 |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            apu.write(mask | Sony_S_DSP_Echo::SOURCE_NUMBER, 0);
        }
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    inline void process(const ProcessArgs &args) final {
        // -------------------------------------------------------------------
        // MARK: RAM (SPC700 emulation)
        // -------------------------------------------------------------------
        // TODO: design a few banks of wavetables / other ways to put data
        //       into this RAM
        // write the first directory to RAM (at the end of the echo buffer)
        auto dir = reinterpret_cast<Sony_S_DSP_Echo::SourceDirectoryEntry*>(&ram[0x7800]);
        // point to a block immediately after this directory entry
        dir->start = 0x7804;
        dir->loop = 0x7804;
        // set address 256 to a single sample ramp wave sample in BRR format
        // the header for the BRR single sample waveform
        auto block = reinterpret_cast<Sony_S_DSP_Echo::BitRateReductionBlock*>(&ram[0x7804]);
        block->flags.set_volume(Sony_S_DSP_Echo::BitRateReductionBlock::MAX_VOLUME);
        block->flags.filter = 0;
        block->flags.is_loop = 1;
        block->flags.is_end = 1;
        for (int i = 0; i < Sony_S_DSP_Echo::BitRateReductionBlock::NUM_SAMPLES; i++)
            block->samples[i] = 15 + 2 * i;
        // -------------------------------------------------------------------
        // MARK: Flags (Noise Frequency)
        // -------------------------------------------------------------------
        // uint8_t noise = params[PARAM_NOISE_FREQ].getValue();
        // apu.write(Sony_S_DSP_Echo::FLAGS, noise);
        // -------------------------------------------------------------------
        // MARK: Gate input
        // -------------------------------------------------------------------
        // // create bit-masks for the key-on and key-off state of each voice
        // uint8_t key_on = 0;
        // uint8_t key_off = 0;
        // // iterate over the voices to detect key-on and key-off events
        // for (unsigned voice = 0; voice < Sony_S_DSP_Echo::VOICE_COUNT; voice++) {
        //     // get the voltage from the gate input port
        //     const auto gate = inputs[INPUT_GATE + voice].getVoltage();
        //     // process the voltage to detect key-on events
        //     key_on = key_on | (gateTriggers[voice][0].process(rescale(gate, 0.f, 2.f, 0.f, 1.f)) << voice);
        //     // process the inverted voltage to detect key-of events
        //     key_off = key_off | (gateTriggers[voice][1].process(rescale(10.f - gate, 0.f, 2.f, 0.f, 1.f)) << voice);
        // }
        // if (key_on) {  // a key-on event occurred from the gate input
        //     // write key off to enable all voices
        //     apu.write(Sony_S_DSP_Echo::KEY_OFF, 0);
        //     // write the key-on value to the register
        //     apu.write(Sony_S_DSP_Echo::KEY_ON, key_on);
        // }
        // if (key_off)  // a key-off event occurred from the gate input
        //     apu.write(Sony_S_DSP_Echo::KEY_OFF, key_off);
        // -------------------------------------------------------------------
        // MARK: Echo Parameters
        // -------------------------------------------------------------------
        apu.write(Sony_S_DSP_Echo::ECHO_FEEDBACK, params[PARAM_ECHO_FEEDBACK].getValue());
        apu.write(Sony_S_DSP_Echo::ECHO_DELAY, params[PARAM_ECHO_DELAY].getValue());

        apu.write(Sony_S_DSP_Echo::ECHO_ENABLE, 0xff);
        apu.write(Sony_S_DSP_Echo::NOISE_ENABLE, 0);
        apu.write(Sony_S_DSP_Echo::PITCH_MODULATION, 0);

        apu.write(Sony_S_DSP_Echo::MAIN_VOLUME_LEFT,  params[PARAM_VOLUME_MAIN + 0].getValue());
        apu.write(Sony_S_DSP_Echo::MAIN_VOLUME_RIGHT, params[PARAM_VOLUME_MAIN + 1].getValue());
        apu.write(Sony_S_DSP_Echo::ECHO_VOLUME_LEFT,  params[PARAM_VOLUME_ECHO + 0].getValue());
        apu.write(Sony_S_DSP_Echo::ECHO_VOLUME_RIGHT, params[PARAM_VOLUME_ECHO + 1].getValue());
        // -------------------------------------------------------------------
        // MARK: Voice-wise Parameters
        // -------------------------------------------------------------------
        for (unsigned voice = 0; voice < Sony_S_DSP_Echo::VOICE_COUNT; voice++) {
            // shift the voice index over a nibble to get the bit mask for the
            // logical OR operator
            auto mask = voice << 4;
            // pitch
            apu.write(mask | Sony_S_DSP_Echo::PITCH_LOW,  0xff &  256     );
            apu.write(mask | Sony_S_DSP_Echo::PITCH_HIGH, 0xff & (256 >> 8));
            // adsr
            apu.write(mask | Sony_S_DSP_Echo::ADSR_1, 0);
            apu.write(mask | Sony_S_DSP_Echo::ADSR_2, 0);
            apu.write(mask | Sony_S_DSP_Echo::GAIN, 127);
            // amp volume
            apu.write(mask | Sony_S_DSP_Echo::VOLUME_LEFT,  127);
            apu.write(mask | Sony_S_DSP_Echo::VOLUME_RIGHT, 127);
        }
        // -------------------------------------------------------------------
        // MARK: FIR Coefficients
        // -------------------------------------------------------------------
        for (unsigned coeff = 0; coeff < Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT; coeff++) {
            auto param = params[PARAM_FIR_COEFFICIENT + coeff].getValue();
            apu.write((coeff << 4) | Sony_S_DSP_Echo::FIR_COEFFICIENTS, param);
        }
        // -------------------------------------------------------------------
        // MARK: Stereo output
        // -------------------------------------------------------------------
        short sample[2] = {0, 0};
        auto left = 32000 * inputs[INPUT_GATE + 0].getVoltage() / 10.f;
        auto right = 32000 * inputs[INPUT_GATE + 1].getVoltage() / 10.f;
        apu.run(left, right, sample);
        outputs[OUTPUT_AUDIO + 0].setVoltage(5.f * sample[0] / std::numeric_limits<int16_t>::max());
        outputs[OUTPUT_AUDIO + 1].setVoltage(5.f * sample[1] / std::numeric_limits<int16_t>::max());
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for SPC700.
struct ChipSNES_EchoWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit ChipSNES_EchoWidget(ChipSNES_Echo *module) {
        setModule(module);
        static constexpr auto panel = "res/S-SMP.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // Gate
        for (unsigned i = 0; i < 2; i++)
            addInput(createInput<PJ301MPort>(Vec(20, 40 + i * 41), module, ChipSNES_Echo::INPUT_GATE + i));
        // FIR Coefficients
        for (unsigned i = 0; i < Sony_S_DSP_Echo::FIR_COEFFICIENT_COUNT; i++) {
            addInput(createInput<PJ301MPort>(Vec(60, 40 + i * 41), module, ChipSNES_Echo::INPUT_FIR_COEFFICIENT + i));
            auto param = createParam<Rogan2PWhite>(Vec(90, 35 + i * 41), module, ChipSNES_Echo::PARAM_FIR_COEFFICIENT + i);
            param->snap = true;
            addParam(param);
        }
        // Echo Delay
        auto echoDelay = createParam<Rogan2PGreen>(Vec(130, 30), module, ChipSNES_Echo::PARAM_ECHO_DELAY);
        echoDelay->snap = true;
        addParam(echoDelay);
        addInput(createInput<PJ301MPort>(Vec(140, 80), module, ChipSNES_Echo::INPUT_ECHO_DELAY));
        // Echo Feedback
        auto echoFeedback = createParam<Rogan2PGreen>(Vec(180, 30), module, ChipSNES_Echo::PARAM_ECHO_FEEDBACK);
        echoFeedback->snap = true;
        addParam(echoFeedback);
        addInput(createInput<PJ301MPort>(Vec(190, 80), module, ChipSNES_Echo::INPUT_ECHO_FEEDBACK));
        // Outputs
        addOutput(createOutput<PJ301MPort>(Vec(140, 325), module, ChipSNES_Echo::OUTPUT_AUDIO + 0));
        addOutput(createOutput<PJ301MPort>(Vec(190, 325), module, ChipSNES_Echo::OUTPUT_AUDIO + 1));

    }
};

/// the global instance of the model
rack::Model *modelChipSNES_Echo = createModel<ChipSNES_Echo, ChipSNES_EchoWidget>("SNES_Echo");
