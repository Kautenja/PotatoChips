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
#include "dsp/sony_s_dsp_brr.hpp"
#include "dsp/wavetable4bit.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Sony S-DSP chip (from Nintendo SNES) emulator module.
struct ChipS_SMP_BRR : Module {
 private:
    /// the RAM for the S-DSP chip (64KB = 16-bit address space)
    uint8_t ram[Sony_S_DSP_BRR::SIZE_OF_RAM];
    /// the Sony S-DSP sound chip emulator
    Sony_S_DSP_BRR apu{ram};

    /// @brief Fill the RAM with 0's.
    inline void clearRAM() { memset(ram, 0, sizeof ram); }

    /// triggers for handling gate inputs for the voices
    rack::dsp::BooleanTrigger gateTriggers[Sony_S_DSP_BRR::VOICE_COUNT][2];

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ,          Sony_S_DSP_BRR::VOICE_COUNT),
        ENUMS(PARAM_PM_ENABLE,     Sony_S_DSP_BRR::VOICE_COUNT),
        ENUMS(PARAM_VOLUME_L,      Sony_S_DSP_BRR::VOICE_COUNT),
        ENUMS(PARAM_VOLUME_R,      Sony_S_DSP_BRR::VOICE_COUNT),
        ENUMS(PARAM_VOLUME_MAIN, 2),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT,          Sony_S_DSP_BRR::VOICE_COUNT),
        ENUMS(INPUT_FM,            Sony_S_DSP_BRR::VOICE_COUNT),
        ENUMS(INPUT_PM_ENABLE,     Sony_S_DSP_BRR::VOICE_COUNT),
        ENUMS(INPUT_GATE,          Sony_S_DSP_BRR::VOICE_COUNT),
        ENUMS(INPUT_VOLUME_L,      Sony_S_DSP_BRR::VOICE_COUNT),
        ENUMS(INPUT_VOLUME_R,      Sony_S_DSP_BRR::VOICE_COUNT),
        ENUMS(INPUT_VOLUME_MAIN, 2),
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
    ChipS_SMP_BRR() {
        // setup parameters
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned osc = 0; osc < Sony_S_DSP_BRR::VOICE_COUNT; osc++) {
            auto osc_name = "Voice " + std::to_string(osc + 1);
            configParam(PARAM_FREQ     + osc, -6.f, 6.f, 2.f, osc_name + " Frequency", " Hz", 2, dsp::FREQ_C4);
            configParam(PARAM_VOLUME_L + osc, -128, 127, 127, osc_name + " Volume (Left)");
            configParam(PARAM_VOLUME_R + osc, -128, 127, 127, osc_name + " Volume (Right)");
            osc_name = "Voice " + std::to_string(osc) + " -> " + osc_name;
            configParam(PARAM_PM_ENABLE + osc, 0, 1, 0, osc_name + " Phase Modulation Enable");
        }
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

        apu.write(Sony_S_DSP_BRR::ECHO_BUFFER_START_OFFSET, 128);
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
        apu.write(Sony_S_DSP_BRR::OFFSET_SOURCE_DIRECTORY, ECHO_LENGTH / 0x100);

        for (unsigned voice = 0; voice < Sony_S_DSP_BRR::VOICE_COUNT; voice++) {
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
            apu.write(mask | Sony_S_DSP_BRR::SOURCE_NUMBER, 0);
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
        // write the first directory to RAM (at the end of the echo buffer)
        auto dir = reinterpret_cast<Sony_S_DSP_BRR::SourceDirectoryEntry*>(&ram[0x7800]);
        // point to a block immediately after this directory entry
        dir->start = 0x7804;
        dir->loop = 0x7804;
        // set address 256 to a single sample ramp wave sample in BRR format
        // the header for the BRR single sample waveform
        auto block = reinterpret_cast<Sony_S_DSP_BRR::BitRateReductionBlock*>(&ram[0x7804]);
        block->flags.set_volume(Sony_S_DSP_BRR::BitRateReductionBlock::MAX_VOLUME);
        block->flags.filter = 0;
        block->flags.is_loop = 1;
        block->flags.is_end = 1;
        static const uint8_t samples[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
        for (unsigned i = 0; i < Sony_S_DSP_BRR::BitRateReductionBlock::NUM_SAMPLES; i++)
            block->samples[i] = samples[i];
        // -------------------------------------------------------------------
        // MARK: Gate input
        // -------------------------------------------------------------------
        // create bit-masks for the key-on and key-off state of each voice
        uint8_t key_on = 0;
        uint8_t key_off = 0;
        // iterate over the voices to detect key-on and key-off events
        for (unsigned voice = 0; voice < Sony_S_DSP_BRR::VOICE_COUNT; voice++) {
            // get the voltage from the gate input port
            const auto gate = inputs[INPUT_GATE + voice].getVoltage();
            // process the voltage to detect key-on events
            key_on = key_on | (gateTriggers[voice][0].process(rescale(gate, 0.f, 2.f, 0.f, 1.f)) << voice);
            // process the inverted voltage to detect key-of events
            key_off = key_off | (gateTriggers[voice][1].process(rescale(10.f - gate, 0.f, 2.f, 0.f, 1.f)) << voice);
        }
        if (key_on) {  // a key-on event occurred from the gate input
            // write key off to enable all voices
            apu.write(Sony_S_DSP_BRR::KEY_OFF, 0);
            // write the key-on value to the register
            apu.write(Sony_S_DSP_BRR::KEY_ON, key_on);
        }
        if (key_off)  // a key-off event occurred from the gate input
            apu.write(Sony_S_DSP_BRR::KEY_OFF, key_off);

        // -------------------------------------------------------------------
        // MARK: Voice-wise Parameters
        // -------------------------------------------------------------------
        // for (unsigned voice = 0; voice < Sony_S_DSP_BRR::VOICE_COUNT; voice++) {
        //
        // }
            unsigned voice = 0;
            // ---------------------------------------------------------------
            // MARK: Frequency
            // ---------------------------------------------------------------
            // calculate the frequency using standard exponential scale
            float pitch = params[PARAM_FREQ + voice].getValue();
            pitch += inputs[INPUT_VOCT + voice].getVoltage();
            pitch += inputs[INPUT_FM + voice].getVoltage() / 5.f;
            float frequency = rack::dsp::FREQ_C4 * powf(2.0, pitch);
            frequency = rack::clamp(frequency, 0.0f, 20000.0f);
            apu.setFrequency(frequency);
            // ---------------------------------------------------------------
            // MARK: Amplifier Volume
            // ---------------------------------------------------------------
            apu.setVolumeLeft(params[PARAM_VOLUME_L + voice].getValue());
            apu.setVolumeRight(params[PARAM_VOLUME_R + voice].getValue());
        // -------------------------------------------------------------------
        // MARK: Stereo output
        // -------------------------------------------------------------------
        short sample[2] = {0, 0};
        apu.run(sample);
        outputs[OUTPUT_AUDIO + 0].setVoltage(5.f * sample[0] / std::numeric_limits<int16_t>::max());
        outputs[OUTPUT_AUDIO + 1].setVoltage(5.f * sample[1] / std::numeric_limits<int16_t>::max());
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for SPC700.
struct ChipS_SMP_BRRWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit ChipS_SMP_BRRWidget(ChipS_SMP_BRR *module) {
        setModule(module);
        static constexpr auto panel = "res/S-SMP-BRR.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // individual oscillator controls
        for (unsigned i = 0; i < Sony_S_DSP_BRR::VOICE_COUNT; i++) {
            // Frequency
            addInput(createInput<PJ301MPort>(Vec(15, 40 + i * 41), module, ChipS_SMP_BRR::INPUT_VOCT + i));
            addInput(createInput<PJ301MPort>(Vec(45, 40 + i * 41), module, ChipS_SMP_BRR::INPUT_FM + i));
            addParam(createParam<Rogan2PSNES>(Vec(75, 35 + i * 41), module, ChipS_SMP_BRR::PARAM_FREQ + i));
            // Gate
            addInput(createInput<PJ301MPort>(Vec(120, 40 + i * 41), module, ChipS_SMP_BRR::INPUT_GATE + i));
            // Volume - Left
            addInput(createInput<PJ301MPort>(Vec(155, 40 + i * 41), module, ChipS_SMP_BRR::INPUT_VOLUME_L + i));
            auto left = createParam<Rogan2PWhite>(Vec(190, 35 + i * 41), module, ChipS_SMP_BRR::PARAM_VOLUME_L + i);
            left->snap = true;
            addParam(left);
            // Volume - Right
            addInput(createInput<PJ301MPort>(Vec(240, 40 + i * 41), module, ChipS_SMP_BRR::INPUT_VOLUME_R + i));
            auto right = createParam<Rogan2PRed>(Vec(275, 35 + i * 41), module, ChipS_SMP_BRR::PARAM_VOLUME_R + i);
            right->snap = true;
            addParam(right);
            // Phase Modulation
            if (i > 0) {  // phase modulation is not defined for the first voice
                addParam(createParam<CKSS>(Vec(330, 40  + i * 41), module, ChipS_SMP_BRR::PARAM_PM_ENABLE + i));
                addInput(createInput<PJ301MPort>(Vec(350, 40 + i * 41), module, ChipS_SMP_BRR::INPUT_PM_ENABLE + i));
            }
        }
        // Output
        addOutput(createOutput<PJ301MPort>(Vec(320, 40), module, ChipS_SMP_BRR::OUTPUT_AUDIO + 0));
        addOutput(createOutput<PJ301MPort>(Vec(355, 40), module, ChipS_SMP_BRR::OUTPUT_AUDIO + 1));
    }
};

/// the global instance of the model
rack::Model *modelChipS_SMP_BRR = createModel<ChipS_SMP_BRR, ChipS_SMP_BRRWidget>("S_SMP_BRR");
