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
#include "engine/chip_module.hpp"
#include "dsp/sony_s_dsp.hpp"
#include "dsp/wavetable4bit.hpp"

#include <iostream>
// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Sony S-DSP chip (from Nintendo SNES) emulator module.
struct ChipS_SMP : Module {
 private:
    /// the RAM for the S-DSP chip (64KB = 16-bit address space)
    uint8_t ram[1 << 16];
    /// the Sony S-DSP sound chip emulator
    Sony_S_DSP apu{ram};

    /// @brief Fill the RAM with 0's.
    inline void clearRAM() { memset(ram, 0, sizeof ram); }

    /// triggers for handling gate inputs for the voices
    rack::dsp::BooleanTrigger gateTriggers[Sony_S_DSP::VOICE_COUNT][2];

 public:
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        ENUMS(PARAM_FREQ,          Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_PM_ENABLE,     Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_NOISE_ENABLE,  Sony_S_DSP::VOICE_COUNT),
        PARAM_NOISE_FREQ,
        ENUMS(PARAM_VOLUME_L,      Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_VOLUME_R,      Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_ATTACK,        Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_DECAY,         Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_SUSTAIN_LEVEL, Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_SUSTAIN_RATE,  Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_ECHO_ENABLE,   Sony_S_DSP::VOICE_COUNT),
        PARAM_ECHO_DELAY,
        PARAM_ECHO_FEEDBACK,
        ENUMS(PARAM_VOLUME_ECHO, 2),
        ENUMS(PARAM_VOLUME_MAIN, 2),
        ENUMS(PARAM_FIR_COEFFICIENT, Sony_S_DSP::FIR_COEFFICIENT_COUNT),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT,          Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_FM,            Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_PM_ENABLE,     Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_NOISE_ENABLE,  Sony_S_DSP::VOICE_COUNT),
        INPUT_NOISE_FM,
        ENUMS(INPUT_GATE,          Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_VOLUME_L,      Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_VOLUME_R,      Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_ATTACK,        Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_DECAY,         Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_SUSTAIN_LEVEL, Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_SUSTAIN_RATE,  Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_ECHO_ENABLE,   Sony_S_DSP::VOICE_COUNT),
        INPUT_ECHO_DELAY,
        INPUT_ECHO_FEEDBACK,
        ENUMS(INPUT_VOLUME_ECHO, 2),
        ENUMS(INPUT_VOLUME_MAIN, 2),
        ENUMS(INPUT_FIR_COEFFICIENT, Sony_S_DSP::FIR_COEFFICIENT_COUNT),
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
    ChipS_SMP() {
        // setup parameters
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned osc = 0; osc < Sony_S_DSP::VOICE_COUNT; osc++) {
            auto osc_name = "Voice " + std::to_string(osc + 1);
            configParam(PARAM_FREQ          + osc, -4.f, 4.f, 2.f, osc_name + " Frequency", " Hz", 2, dsp::FREQ_C4);
            configParam(PARAM_VOLUME_L      + osc, -128, 127, 127, osc_name + " Volume (Left)");
            configParam(PARAM_VOLUME_R      + osc, -128, 127, 127, osc_name + " Volume (Right)");
            configParam(PARAM_ATTACK        + osc,    0,  15,   0, osc_name + " Envelope Attack");
            configParam(PARAM_DECAY         + osc,    0,   7,   0, osc_name + " Envelope Decay");
            configParam(PARAM_SUSTAIN_LEVEL + osc,    0,   7,   0, osc_name + " Envelope Sustain Level");
            configParam(PARAM_SUSTAIN_RATE  + osc,    0,  31,   0, osc_name + " Envelope Sustain Rate");
            configParam(PARAM_NOISE_ENABLE  + osc,    0,   1,   0, osc_name + " Noise Enable");
            configParam(PARAM_ECHO_ENABLE   + osc,    0,   1,   1, osc_name + " Echo Enable");
            if (osc > 0) {  // voice 0 does not have phase modulation
                osc_name = "Voice " + std::to_string(osc) + " -> " + osc_name;
                configParam(PARAM_PM_ENABLE + osc, 0, 1, 0, osc_name + " Phase Modulation Enable");
            }
        }
        for (unsigned coeff = 0; coeff < Sony_S_DSP::FIR_COEFFICIENT_COUNT; coeff++) {
            // the first FIR coefficient defaults to 0x7f = 127 and the other
            // coefficients are 0 by default
            configParam(PARAM_FIR_COEFFICIENT  + coeff, -128, 127, (coeff ? 0 : 127), "FIR Coefficient " + std::to_string(coeff + 1));
        }
        configParam(PARAM_NOISE_FREQ,         0,  31,  16, "Noise Frequency");
        configParam(PARAM_ECHO_DELAY,         0,  15,   0, "Echo Delay", "ms", 0, 16);
        configParam(PARAM_ECHO_FEEDBACK,   -128, 127,   0, "Echo Feedback");
        configParam(PARAM_VOLUME_ECHO + 0, -128, 127, 127, "Echo Volume (Left)");
        configParam(PARAM_VOLUME_ECHO + 1, -128, 127, 127, "Echo Volume (Right)");
        configParam(PARAM_VOLUME_MAIN + 0, -128, 127, 127, "Main Volume (Left)");
        configParam(PARAM_VOLUME_MAIN + 1, -128, 127, 127, "Main Volume (Right)");
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

        apu.write(Sony_S_DSP::ECHO_BUFFER_START_OFFSET, 128);
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
        apu.write(Sony_S_DSP::OFFSET_SOURCE_DIRECTORY, ECHO_LENGTH / 0x100);

        for (unsigned voice = 0; voice < Sony_S_DSP::VOICE_COUNT; voice++) {
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
            apu.write(mask | Sony_S_DSP::SOURCE_NUMBER, 0);
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
        auto dir = reinterpret_cast<Sony_S_DSP::SourceDirectoryEntry*>(&ram[0x7800]);
        // point to a block immediately after this directory entry
        dir->start = 0x7804;
        dir->loop = 0x7804;
        // set address 256 to a single sample ramp wave sample in BRR format
        // the header for the BRR single sample waveform
        auto block = reinterpret_cast<Sony_S_DSP::BitRateReductionBlock*>(&ram[0x7804]);
        block->flags.set_volume(Sony_S_DSP::BitRateReductionBlock::MAX_VOLUME);
        block->flags.filter = 0;
        block->flags.is_loop = 1;
        block->flags.is_end = 1;
        for (int i = 0; i < Sony_S_DSP::BitRateReductionBlock::NUM_SAMPLES; i++)
            block->samples[i] = 15 + 2 * i;
        // -------------------------------------------------------------------
        // MARK: Flags (Noise Frequency)
        // -------------------------------------------------------------------
        uint8_t noise = params[PARAM_NOISE_FREQ].getValue();
        apu.write(Sony_S_DSP::FLAGS, noise);
        // -------------------------------------------------------------------
        // MARK: Gate input
        // -------------------------------------------------------------------
        // create bit-masks for the key-on and key-off state of each voice
        uint8_t key_on = 0;
        uint8_t key_off = 0;
        // iterate over the voices to detect key-on and key-off events
        for (unsigned voice = 0; voice < Sony_S_DSP::VOICE_COUNT; voice++) {
            // get the voltage from the gate input port
            const auto gate = inputs[INPUT_GATE + voice].getVoltage();
            // process the voltage to detect key-on events
            key_on = key_on | (gateTriggers[voice][0].process(rescale(gate, 0.f, 2.f, 0.f, 1.f)) << voice);
            // process the inverted voltage to detect key-of events
            key_off = key_off | (gateTriggers[voice][1].process(rescale(10.f - gate, 0.f, 2.f, 0.f, 1.f)) << voice);
        }
        if (key_on) {  // a key-on event occurred from the gate input
            // write key off to enable all voices
            apu.write(Sony_S_DSP::KEY_OFF, 0);
            // write the key-on value to the register
            apu.write(Sony_S_DSP::KEY_ON, key_on);
        }
        if (key_off)  // a key-off event occurred from the gate input
            apu.write(Sony_S_DSP::KEY_OFF, key_off);
        // -------------------------------------------------------------------
        // MARK: Echo Parameters
        // -------------------------------------------------------------------
        apu.write(Sony_S_DSP::ECHO_FEEDBACK, params[PARAM_ECHO_FEEDBACK].getValue());
        apu.write(Sony_S_DSP::ECHO_DELAY, params[PARAM_ECHO_DELAY].getValue());
        // echo enable
        uint8_t echo_enable = 0;
        for (unsigned voice = 0; voice < Sony_S_DSP::VOICE_COUNT; voice++)
            echo_enable |= static_cast<uint8_t>(params[PARAM_ECHO_ENABLE + voice].getValue()) << voice;
        apu.write(Sony_S_DSP::ECHO_ENABLE, echo_enable);
        // -------------------------------------------------------------------
        // MARK: Noise Enable
        // -------------------------------------------------------------------
        uint8_t noise_enable = 0;
        for (unsigned voice = 0; voice < Sony_S_DSP::VOICE_COUNT; voice++)
            noise_enable |= static_cast<uint8_t>(params[PARAM_NOISE_ENABLE + voice].getValue()) << voice;
        apu.write(Sony_S_DSP::NOISE_ENABLE, noise_enable);
        // -------------------------------------------------------------------
        // MARK: Pitch Modulation
        // -------------------------------------------------------------------
        uint8_t pitch_modulation = 0;
        // start from 1 because there is no pitch modulation for the first channel
        for (unsigned voice = 1; voice < Sony_S_DSP::VOICE_COUNT; voice++)
            pitch_modulation |= static_cast<uint8_t>(params[PARAM_PM_ENABLE + voice].getValue()) << voice;
        apu.write(Sony_S_DSP::PITCH_MODULATION, pitch_modulation);
        // -------------------------------------------------------------------
        // MARK: Main Volume & Echo Volume
        // -------------------------------------------------------------------
        apu.write(Sony_S_DSP::MAIN_VOLUME_LEFT,  params[PARAM_VOLUME_MAIN + 0].getValue());
        apu.write(Sony_S_DSP::MAIN_VOLUME_RIGHT, params[PARAM_VOLUME_MAIN + 1].getValue());
        apu.write(Sony_S_DSP::ECHO_VOLUME_LEFT,  params[PARAM_VOLUME_ECHO + 0].getValue());
        apu.write(Sony_S_DSP::ECHO_VOLUME_RIGHT, params[PARAM_VOLUME_ECHO + 1].getValue());
        // -------------------------------------------------------------------
        // MARK: Voice-wise Parameters
        // -------------------------------------------------------------------
        for (unsigned voice = 0; voice < Sony_S_DSP::VOICE_COUNT; voice++) {
            // shift the voice index over a nibble to get the bit mask for the
            // logical OR operator
            auto mask = voice << 4;
            // ---------------------------------------------------------------
            // MARK: Frequency
            // ---------------------------------------------------------------
            // calculate the frequency using standard exponential scale
            float pitch = params[PARAM_FREQ + voice].getValue();
            pitch += inputs[INPUT_VOCT + voice].getVoltage();
            pitch += inputs[INPUT_FM + voice].getVoltage() / 5.f;
            float frequency = rack::dsp::FREQ_C4 * powf(2.0, pitch);
            frequency = rack::clamp(frequency, 0.0f, 20000.0f);
            // convert the floating point frequency to a 14-bit pitch value
            auto pitch16bit = Sony_S_DSP::convert_pitch(frequency);
            // set the 14-bit pitch value to the cascade of two RAM slots
            apu.write(mask | Sony_S_DSP::PITCH_LOW,  0xff &  pitch16bit     );
            apu.write(mask | Sony_S_DSP::PITCH_HIGH, 0xff & (pitch16bit >> 8));
            // ---------------------------------------------------------------
            // MARK: Gain (Custom ADSR override)
            // ---------------------------------------------------------------
            // TODO: GAIN can be used to implement custom envelopes in your
            // program. There are 5 modes GAIN uses.
            // DIRECT
            //
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x7   |  0  |               PARAMETER                 |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // INCREASE (LINEAR)
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x7   |  1  |  1  |  0  |          PARAMETER          |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // INCREASE (BENT LINE)
            //
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x7   |  1  |  1  |  1  |          PARAMETER          |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // DECREASE (LINEAR)
            //
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x7   |  1  |  0  |  0  |          PARAMETER          |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // DECREASE (EXPONENTIAL)
            //
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x7   |  1  |  0  |  1  |          PARAMETER          |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // Direct: The value of GAIN is set to PARAMETER.
            //
            // Increase (Linear):
            //     GAIN slides to 1 with additions of 1/64.
            //
            // Increase (Bent Line):
            //     GAIN slides up with additions of 1/64 until it reaches 3/4,
            //     then it slides up to 1 with additions of 1/256.
            //
            // Decrease (Linear):
            //     GAIN slides down to 0 with subtractions of 1/64.
            //
            // Decrease (Exponential):
            //     GAIN slides down exponentially by getting multiplied by
            //     255/256.
            //
            // Table 2.3 Gain Parameters (Increate 0 -> 1 / Decrease 1 -> 0):
            // Parameter Value Increase Linear Increase Bentline   Decrease Linear Decrease Exponential
            // 00  INFINITE    INFINITE    INFINITE    INFINITE
            // 01  4.1s    7.2s    4.1s    38s
            // 02  3.1s    5.4s    3.1s    28s
            // 03  2.6s    4.6s    2.6s    24s
            // 04  2.0s    3.5s    2.0s    19s
            // 05  1.5s    2.6s    1.5s    14s
            // 06  1.3s    2.3s    1.3s    12s
            // 07  1.0s    1.8s    1.0s    9.4s
            // 08  770ms   1.3s    770ms   7.1s
            // 09  640ms   1.1s    640ms   5.9s
            // 0A  510ms   900ms   510ms   4.7s
            // 0B  380ms   670ms   380ms   3.5s
            // 0C  320ms   560ms   320ms   2.9s
            // 0D  260ms   450ms   260ms   2.4s
            // 0E  190ms   340ms   190ms   1.8s
            // 0F  160ms   280ms   160ms   1.5s
            // 10  130ms   220ms   130ms   1.2s
            // 11  96ms    170ms   96ms    880ms
            // 12  80ms    140ms   80ms    740ms
            // 13  64ms    110ms   64ms    590ms
            // 14  48ms    84ms    48ms    440ms
            // 15  40ms    70ms    40ms    370ms
            // 16  32ms    56ms    32ms    290ms
            // 17  24ms    42ms    24ms    220ms
            // 18  20ms    35ms    20ms    180ms
            // 19  16ms    28ms    16ms    150ms
            // 1A  12ms    21ms    12ms    110ms
            // 1B  10ms    18ms    10ms    92ms
            // 1C  8ms 14ms    8ms 74ms
            // 1D  6ms 11ms    6ms 55ms
            // 1E  4ms 7ms 4ms 37ms
            // 1F  2ms 3.5ms   2ms 18ms
            //
            // apu.write(mask | Sony_S_DSP::GAIN, 64);
            // ---------------------------------------------------------------
            // MARK: ADSR
            // ---------------------------------------------------------------
            // the ADSR1 register is set from the attack and decay values
            auto attack = (uint8_t) params[PARAM_ATTACK + voice].getValue();
            auto decay = (uint8_t) params[PARAM_DECAY + voice].getValue();
            // the high bit of the ADSR1 register is set to enable the ADSR
            auto adsr1 = 0b10000000 | (decay << 4) | attack;
            apu.write(mask | Sony_S_DSP::ADSR_1, adsr1);
            // the ADSR2 register is set from the sustain level and rate
            auto sustainLevel = (uint8_t) params[PARAM_SUSTAIN_LEVEL + voice].getValue();
            auto sustainRate = (uint8_t) params[PARAM_SUSTAIN_RATE + voice].getValue();
            auto adsr2 = (sustainLevel << 5) | sustainRate;
            apu.write(mask | Sony_S_DSP::ADSR_2, adsr2);
            // ---------------------------------------------------------------
            // MARK: ADSR Output
            // ---------------------------------------------------------------
            // TODO: ENVX gets written to by the DSP. It contains the present
            // ADSR/GAIN envelope value.
            // ENVX
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x8   |  0  |                 VALUE                   |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // 7-bit unsigned value
            // apu.read(mask | Sony_S_DSP::ENVELOPE_OUT, 0);
            // ---------------------------------------------------------------
            // MARK: Waveform Output
            // ---------------------------------------------------------------
            // OUTX is written to by the DSP. It contains the present wave height multiplied by the ADSR/GAIN envelope value. It isn't multiplied by the voice volume though.
            //
            // OUTX
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x9   | sign|                 VALUE                   |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // 8-bit signed value
            // apu.read(mask | Sony_S_DSP::WAVEFORM_OUT, 0);
            // ---------------------------------------------------------------
            // MARK: Amplifier Volume
            // ---------------------------------------------------------------
            apu.write(mask | Sony_S_DSP::VOLUME_LEFT,  params[PARAM_VOLUME_L + voice].getValue());
            apu.write(mask | Sony_S_DSP::VOLUME_RIGHT, params[PARAM_VOLUME_R + voice].getValue());
        }
        // -------------------------------------------------------------------
        // MARK: FIR Coefficients
        // -------------------------------------------------------------------
        for (unsigned coeff = 0; coeff < Sony_S_DSP::FIR_COEFFICIENT_COUNT; coeff++) {
            auto param = params[PARAM_FIR_COEFFICIENT + coeff].getValue();
            apu.write((coeff << 4) | Sony_S_DSP::FIR_COEFFICIENTS, param);
        }
        // -------------------------------------------------------------------
        // MARK: Voice Activity Output
        // -------------------------------------------------------------------
        // TODO: This register is written to during DSP activity.
        //
        // Each voice gets 1 bit. If the bit is set then it means the BRR
        // decoder has reached the last compressed block in the sample.
        //
        // ENDX
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $7C   |VOIC7|VOIC6|VOIC5|VOIC4|VOIC3|VOIC2|VOIC1|VOIC0|
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // apu.read(Sony_S_DSP::ENDX, 0);
        // -------------------------------------------------------------------
        // MARK: Stereo output
        // -------------------------------------------------------------------
        short sample[2] = {0, 0};
        apu.run(1, sample);
        outputs[OUTPUT_AUDIO + 0].setVoltage(5.f * sample[0] / std::numeric_limits<int16_t>::max());
        outputs[OUTPUT_AUDIO + 1].setVoltage(5.f * sample[1] / std::numeric_limits<int16_t>::max());
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// The panel widget for SPC700.
struct ChipS_SMPWidget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit ChipS_SMPWidget(ChipS_SMP *module) {
        setModule(module);
        static constexpr auto panel = "res/S-SMP.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // individual oscillator controls
        for (unsigned i = 0; i < Sony_S_DSP::VOICE_COUNT; i++) {
            // Frequency
            addInput(createInput<PJ301MPort>(Vec(15, 40 + i * 41), module, ChipS_SMP::INPUT_VOCT + i));
            addInput(createInput<PJ301MPort>(Vec(45, 40 + i * 41), module, ChipS_SMP::INPUT_FM + i));
            addParam(createParam<Rogan2PSNES>(Vec(75, 35 + i * 41), module, ChipS_SMP::PARAM_FREQ + i));
            // Gate
            addInput(createInput<PJ301MPort>(Vec(185, 40 + i * 41), module, ChipS_SMP::INPUT_GATE + i));
            // Volume - Left
            addInput(createInput<PJ301MPort>(Vec(220, 40 + i * 41), module, ChipS_SMP::INPUT_VOLUME_L + i));
            auto left = createParam<Rogan2PWhite>(Vec(250, 35 + i * 41), module, ChipS_SMP::PARAM_VOLUME_L + i);
            left->snap = true;
            addParam(left);
            // Volume - Right
            addInput(createInput<PJ301MPort>(Vec(300, 40 + i * 41), module, ChipS_SMP::INPUT_VOLUME_R + i));
            auto right = createParam<Rogan2PRed>(Vec(330, 35 + i * 41), module, ChipS_SMP::PARAM_VOLUME_R + i);
            right->snap = true;
            addParam(right);
            // ADSR - Attack
            addInput(createInput<PJ301MPort>(Vec(390, 40 + i * 41), module, ChipS_SMP::INPUT_ATTACK + i));
            auto attack = createParam<Rogan2PGreen>(Vec(420, 35 + i * 41), module, ChipS_SMP::PARAM_ATTACK + i);
            attack->snap = true;
            addParam(attack);
            // ADSR - Decay
            addInput(createInput<PJ301MPort>(Vec(460, 40 + i * 41), module, ChipS_SMP::INPUT_DECAY + i));
            auto decay = createParam<Rogan2PBlue>(Vec(490, 35 + i * 41), module, ChipS_SMP::PARAM_DECAY + i);
            decay->snap = true;
            addParam(decay);
            // ADSR - Sustain Level
            addInput(createInput<PJ301MPort>(Vec(530, 40 + i * 41), module, ChipS_SMP::INPUT_SUSTAIN_LEVEL + i));
            auto sustainLevel = createParam<Rogan2PRed>(Vec(560, 35 + i * 41), module, ChipS_SMP::PARAM_SUSTAIN_LEVEL + i);
            sustainLevel->snap = true;
            addParam(sustainLevel);
            // ADSR - Sustain Rate
            addInput(createInput<PJ301MPort>(Vec(600, 40 + i * 41), module, ChipS_SMP::INPUT_SUSTAIN_RATE + i));
            auto sustainRate = createParam<Rogan2PWhite>(Vec(630, 35 + i * 41), module, ChipS_SMP::PARAM_SUSTAIN_RATE + i);
            sustainRate->snap = true;
            addParam(sustainRate);
            // Phase Modulation
            if (i > 0) {  // phase modulation is not defined for the first voice
                addParam(createParam<CKSS>(Vec(880, 40  + i * 41), module, ChipS_SMP::PARAM_PM_ENABLE + i));
                addInput(createInput<PJ301MPort>(Vec(900, 40 + i * 41), module, ChipS_SMP::INPUT_PM_ENABLE + i));
            }
            // Echo Enable
            addParam(createParam<CKSS>(Vec(940, 40  + i * 41), module, ChipS_SMP::PARAM_ECHO_ENABLE + i));
            addInput(createInput<PJ301MPort>(Vec(960, 40 + i * 41), module, ChipS_SMP::INPUT_ECHO_ENABLE + i));
            // Noise Enable
            addParam(createParam<CKSS>(Vec(1000, 40  + i * 41), module, ChipS_SMP::PARAM_NOISE_ENABLE + i));
            addInput(createInput<PJ301MPort>(Vec(1020, 40 + i * 41), module, ChipS_SMP::INPUT_NOISE_ENABLE + i));
        }

        // Noise Frequency
        addInput(createInput<PJ301MPort>(Vec(115, 40), module, ChipS_SMP::INPUT_NOISE_FM));
        auto noise = createParam<Rogan2PSNES>(Vec(145, 35), module, ChipS_SMP::PARAM_NOISE_FREQ);
        noise->snap = true;
        addParam(noise);

        // Echo Delay
        auto echoDelay = createParam<Rogan2PGreen>(Vec(690, 30), module, ChipS_SMP::PARAM_ECHO_DELAY);
        echoDelay->snap = true;
        addParam(echoDelay);
        addInput(createInput<PJ301MPort>(Vec(700, 80), module, ChipS_SMP::INPUT_ECHO_DELAY));
        // Echo Feedback
        auto echoFeedback = createParam<Rogan2PGreen>(Vec(740, 30), module, ChipS_SMP::PARAM_ECHO_FEEDBACK);
        echoFeedback->snap = true;
        addParam(echoFeedback);
        addInput(createInput<PJ301MPort>(Vec(750, 80), module, ChipS_SMP::INPUT_ECHO_FEEDBACK));

        // Echo Volume - Left channel
        auto echoLeft = createParam<Rogan2PWhite>(Vec(690, 130), module, ChipS_SMP::PARAM_VOLUME_ECHO + 0);
        echoLeft->snap = true;
        addParam(echoLeft);
        addInput(createInput<PJ301MPort>(Vec(700, 180), module, ChipS_SMP::INPUT_VOLUME_ECHO + 0));
        // Echo Volume - Right channel
        auto echoRight = createParam<Rogan2PRed>(Vec(740, 130), module, ChipS_SMP::PARAM_VOLUME_ECHO + 1);
        echoRight->snap = true;
        addParam(echoRight);
        addInput(createInput<PJ301MPort>(Vec(750, 180), module, ChipS_SMP::INPUT_VOLUME_ECHO + 1));

        // Mixer & Output - Left Channel
        auto volumeLeft = createParam<Rogan2PWhite>(Vec(690, 230), module, ChipS_SMP::PARAM_VOLUME_MAIN + 0);
        volumeLeft->snap = true;
        addParam(volumeLeft);
        addInput(createInput<PJ301MPort>(Vec(700, 280), module, ChipS_SMP::INPUT_VOLUME_MAIN + 0));
        addOutput(createOutput<PJ301MPort>(Vec(700, 325), module, ChipS_SMP::OUTPUT_AUDIO + 0));
        // Mixer & Output - Right Channel
        auto volumeRight = createParam<Rogan2PRed>(Vec(740, 230), module, ChipS_SMP::PARAM_VOLUME_MAIN + 1);
        volumeRight->snap = true;
        addParam(volumeRight);
        addInput(createInput<PJ301MPort>(Vec(750, 280), module, ChipS_SMP::INPUT_VOLUME_MAIN + 1));
        addOutput(createOutput<PJ301MPort>(Vec(750, 325), module, ChipS_SMP::OUTPUT_AUDIO + 1));

        // FIR Coefficients
        for (unsigned i = 0; i < Sony_S_DSP::FIR_COEFFICIENT_COUNT; i++) {
            addInput(createInput<PJ301MPort>(Vec(800, 40 + i * 41), module, ChipS_SMP::INPUT_FIR_COEFFICIENT + i));
            auto param = createParam<Rogan2PWhite>(Vec(830, 35 + i * 41), module, ChipS_SMP::PARAM_FIR_COEFFICIENT + i);
            param->snap = true;
            addParam(param);
        }
    }
};

/// the global instance of the model
rack::Model *modelChipS_SMP = createModel<ChipS_SMP, ChipS_SMPWidget>("S_SMP");
