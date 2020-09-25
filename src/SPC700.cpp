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

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Sony S-DSP chip (from Nintendo SNES) emulator module.
struct ChipSPC700 : Module {
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
        ENUMS(PARAM_NOISE_FREQ,    Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_VOLUME_L,      Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_VOLUME_R,      Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_ATTACK,        Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_DECAY,         Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_SUSTAIN_LEVEL, Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_SUSTAIN_RATE,  Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_VOLUME_MAIN, 2),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT,          Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_NOISE_FM,      Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_FM,            Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_GATE,          Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_VOLUME_L,      Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_VOLUME_R,      Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_ATTACK,        Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_DECAY,         Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_SUSTAIN_LEVEL, Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_SUSTAIN_RATE,  Sony_S_DSP::VOICE_COUNT),
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
    ChipSPC700() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (unsigned osc = 0; osc < Sony_S_DSP::VOICE_COUNT; osc++) {
            auto osc_name = "Voice " + std::to_string(osc + 1);
            configParam(PARAM_FREQ          + osc, -4.f, 4.f, 0.f, osc_name + " Frequency", " Hz", 2, dsp::FREQ_C4);
            configParam(PARAM_NOISE_FREQ    + osc,    0,  31,  16, osc_name + " Noise Frequency");
            configParam(PARAM_VOLUME_L      + osc, -128, 127, 127, osc_name + " Volume (Left)");
            configParam(PARAM_VOLUME_R      + osc, -128, 127, 127, osc_name + " Volume (Right)");
            configParam(PARAM_ATTACK        + osc,    0,  15,   0, osc_name + " Envelope Attack");
            configParam(PARAM_DECAY         + osc,    0,   7,   0, osc_name + " Envelope Decay");
            configParam(PARAM_SUSTAIN_LEVEL + osc,    0,   7,   0, osc_name + " Envelope Sustain Level");
            configParam(PARAM_SUSTAIN_RATE  + osc,    0,  31,   0, osc_name + " Envelope Sustain Rate");
        }
        configParam(PARAM_VOLUME_MAIN + 0, -128, 127, 127, "Main Volume (Left)");
        configParam(PARAM_VOLUME_MAIN + 1, -128, 127, 127, "Main Volume (Right)");

        // clear the shared RAM between the CPU and the S-DSP
        clearRAM();
        // reset the S-DSP emulator
        apu.reset();

        // apu.run(1, NULL);
        processCV();
    }

 protected:
    inline void processCV() {
        // Source Directory Offset.
        //
        // DIR
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $5D   |                  Offset value                 |
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // This register points to the source(sample) directory in external
        // RAM. The pointer is calculated by Offset*100h.
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

        // set the source directory location to the first 256 bytes
        apu.write(Sony_S_DSP::OFFSET_SOURCE_DIRECTORY, 0);

        // This register is written to during DSP activity.
        //
        // Each voice gets 1 bit. If the bit is set then it means the BRR
        // decoder has reached the last compressed block in the sample.
        //
        // ENDX
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $7C   |VOIC7|VOIC6|VOIC5|VOIC4|VOIC3|VOIC2|VOIC1|VOIC0|
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // apu.write(Sony_S_DSP::ENDX, 0);

        // PMON
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $2D   |VOIC7|VOIC6|VOIC5|VOIC4|VOIC3|VOIC2|VOIC1|  -  |
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // Pitch modulation multiplies the current pitch of the channel by
        // OUTX of the previous channel. (P (modulated) = P[X] * (1 + OUTX[X-1])
        //
        // So a sine wave in the previous channel would cause some vibrato on
        // the modulated channel. Note that OUTX is before volume
        // multiplication, so you can have a silent channel for modulating.
        // apu.write(Sony_S_DSP::PITCH_MODULATION,  0);

        // Noise enable.
        //
        // NON
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $3D   |VOIC7|VOIC6|VOIC5|VOIC4|VOIC3|VOIC2|VOIC1|VOIC0|
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // When the noise enable bit is specified for a certain channel, white
        // noise is issued instead of sample data. The frequency of the white
        // noise is set in the FLG register. The white noise still requires a
        // (dummy) sample to determine the length of sound (or unlimited sound
        // if the sample loops).
        apu.write(Sony_S_DSP::NOISE_ENABLE, 0xf0);

        // Echo enable.
        //
        // EON
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $4D   |VOIC7|VOIC6|VOIC5|VOIC4|VOIC3|VOIC2|VOIC1|VOIC0|
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // This register enables echo effects for the specified channel(s).
        apu.write(Sony_S_DSP::ECHO_ENABLE, 0);

        // Writing to this register sets the Echo Feedback. It's an 8-bit
        // signed value. Some more information on how the feedback works
        // would be nice.
        //
        // EFB
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $0D   | sign|             Echo Feedback               |
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        apu.write(Sony_S_DSP::ECHO_FEEDBACK, 0);

        // Echo data start address.
        //
        // ESA
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $6D   |                  Offset value                 |
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // This register points to an area of memory to be used by the echo
        // buffer. Like DIR its value is multiplied by 100h.
        apu.write(Sony_S_DSP::ECHO_BUFFER_START_OFFSET, 0);

        // Echo delay size.
        //
        // EDL
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $7D   |  -  |  -  |  -  |  -  |      Echo Delay       |
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // EDL specifies the delay between the main sound and the echoed sound.
        // The delay is calculated as EDL * 16ms.
        //
        // Increased amounts of delay require more memory. The amount of memory
        // required is EDL * 2KBytes (MAX $7800 bytes). The memory region used
        // will be [ESA*100h] -> [ESA*100h + EDL*800h -1]. If EDL is zero, 4
        // bytes of memory at [ESA*100h] -> [ESA*100h + 3] will still be used.
        apu.write(Sony_S_DSP::ECHO_DELAY, 0);

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
            // GAIN can be used to implement custom envelopes in your program.
            // There are 5 modes GAIN uses.
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
            // ENVX gets written to by the DSP. It contains the present ADSR/GAIN envelope value.
            // ENVX
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x8   |  0  |                 VALUE                   |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // 7-bit unsigned value
            // apu.write(mask | Sony_S_DSP::ENVELOPE_OUT, 0);
        }
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    inline void process(const ProcessArgs &args) final {
        // write the first directory to the ram (points to address 256)
        ram[1] = 1;
        ram[3] = 1;
        // set address 256 to a single sample ramp wave
        ram[256] = 0b11000011;
        for (int i = 1; i < 9; i++) ram[256 + i] = 15 + 2 * (i - 1);
        // ram[256 + 1 * 9] = 0b11000000;
        // for (int i = 1; i < 9; i++) ram[256 + 1 * 9 + i] = 8 + i - 1;
        // ram[256 + 2 * 9] = 0b11000000;
        // for (int i = 1; i < 9; i++) ram[256 + 2 * 9 + i] = 16 + i - 1;
        // ram[256 + 3 * 9] = 0b11000011;
        // for (int i = 1; i < 9; i++) ram[256 + 3 * 9 + i] = 24 + i - 1;

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
        // MARK: Main Volume
        // -------------------------------------------------------------------
        apu.write(Sony_S_DSP::MAIN_VOLUME_LEFT,  params[PARAM_VOLUME_MAIN + 0].getValue());
        apu.write(Sony_S_DSP::MAIN_VOLUME_RIGHT, params[PARAM_VOLUME_MAIN + 1].getValue());
        // apu.write(Sony_S_DSP::ECHO_VOLUME_LEFT,  params[PARAM_VOLUME_ECHO + 0].getValue());
        // apu.write(Sony_S_DSP::ECHO_VOLUME_RIGHT, params[PARAM_VOLUME_ECHO + 1].getValue());
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

            // auto frequency = dsp::FREQ_C4 * pow(2, params[PARAM_FREQ].getValue());
            // convert the floating point frequency to a 14-bit pitch value
            auto pitch16bit = Sony_S_DSP::convert_pitch(frequency);
            // set the 14-bit pitch value to the cascade of two RAM slots
            apu.write(mask | Sony_S_DSP::PITCH_LOW,  0xff &  pitch16bit     );
            apu.write(mask | Sony_S_DSP::PITCH_HIGH, 0xff & (pitch16bit >> 8));
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
            // MARK: Amplifier Volume
            // ---------------------------------------------------------------
            apu.write(mask | Sony_S_DSP::VOLUME_LEFT,  params[PARAM_VOLUME_L + voice].getValue());
            apu.write(mask | Sony_S_DSP::VOLUME_RIGHT, params[PARAM_VOLUME_R + voice].getValue());
        }
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
struct ChipSPC700Widget : ModuleWidget {
    /// @brief Initialize a new widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit ChipSPC700Widget(ChipSPC700 *module) {
        setModule(module);
        static constexpr auto panel = "res/S-DSP.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // individual oscillator controls
        for (unsigned i = 0; i < Sony_S_DSP::VOICE_COUNT; i++) {
            // Frequency
            addInput(createInput<PJ301MPort>(Vec(15, 40 + i * 41), module, ChipSPC700::INPUT_VOCT + i));
            addInput(createInput<PJ301MPort>(Vec(45, 40 + i * 41), module, ChipSPC700::INPUT_FM + i));
            addParam(createParam<Rogan2PSNES>(Vec(75, 35 + i * 41), module, ChipSPC700::PARAM_FREQ + i));
            // Noise Frequency
            addInput(createInput<PJ301MPort>(Vec(115, 40 + i * 41), module, ChipSPC700::INPUT_NOISE_FM + i));
            auto noise = createParam<Rogan2PSNES>(Vec(145, 35 + i * 41), module, ChipSPC700::PARAM_NOISE_FREQ + i);
            noise->snap = true;
            addParam(noise);
            // Gate
            addInput(createInput<PJ301MPort>(Vec(185, 40 + i * 41), module, ChipSPC700::INPUT_GATE + i));
            // Volume - Left
            addInput(createInput<PJ301MPort>(Vec(220, 40 + i * 41), module, ChipSPC700::INPUT_VOLUME_L + i));
            auto left = createParam<Rogan2PWhite>(Vec(250, 35 + i * 41), module, ChipSPC700::PARAM_VOLUME_L + i);
            left->snap = true;
            addParam(left);
            // Volume - Right
            addInput(createInput<PJ301MPort>(Vec(300, 40 + i * 41), module, ChipSPC700::INPUT_VOLUME_R + i));
            auto right = createParam<Rogan2PRed>(Vec(330, 35 + i * 41), module, ChipSPC700::PARAM_VOLUME_R + i);
            right->snap = true;
            addParam(right);
            // ADSR - Attack
            addInput(createInput<PJ301MPort>(Vec(390, 40 + i * 41), module, ChipSPC700::INPUT_ATTACK + i));
            auto attack = createParam<Rogan2PGreen>(Vec(420, 35 + i * 41), module, ChipSPC700::PARAM_ATTACK + i);
            attack->snap = true;
            addParam(attack);
            // ADSR - Decay
            addInput(createInput<PJ301MPort>(Vec(460, 40 + i * 41), module, ChipSPC700::INPUT_DECAY + i));
            auto decay = createParam<Rogan2PBlue>(Vec(490, 35 + i * 41), module, ChipSPC700::PARAM_DECAY + i);
            decay->snap = true;
            addParam(decay);
            // ADSR - Sustain Level
            addInput(createInput<PJ301MPort>(Vec(530, 40 + i * 41), module, ChipSPC700::INPUT_SUSTAIN_LEVEL + i));
            auto sustainLevel = createParam<Rogan2PRed>(Vec(560, 35 + i * 41), module, ChipSPC700::PARAM_SUSTAIN_LEVEL + i);
            sustainLevel->snap = true;
            addParam(sustainLevel);
            // ADSR - Sustain Rate
            addInput(createInput<PJ301MPort>(Vec(600, 40 + i * 41), module, ChipSPC700::INPUT_SUSTAIN_RATE + i));
            auto sustainRate = createParam<Rogan2PWhite>(Vec(630, 35 + i * 41), module, ChipSPC700::PARAM_SUSTAIN_RATE + i);
            sustainRate->snap = true;
            addParam(sustainRate);
        }
        // Mixer & Output - Left Channel
        auto left = createParam<Rogan2PWhite>(Vec(690, 230), module, ChipSPC700::PARAM_VOLUME_MAIN + 0);
        left->snap = true;
        addParam(left);
        addInput(createInput<PJ301MPort>(Vec(700, 280), module, ChipSPC700::INPUT_VOLUME_MAIN + 0));
        addOutput(createOutput<PJ301MPort>(Vec(700, 325), module, ChipSPC700::OUTPUT_AUDIO + 0));
        // Mixer & Output - Right Channel
        auto right = createParam<Rogan2PRed>(Vec(740, 230), module, ChipSPC700::PARAM_VOLUME_MAIN + 1);
        right->snap = true;
        addParam(right);
        addInput(createInput<PJ301MPort>(Vec(730, 280), module, ChipSPC700::INPUT_VOLUME_MAIN + 1));
        addOutput(createOutput<PJ301MPort>(Vec(730, 325), module, ChipSPC700::OUTPUT_AUDIO + 1));
    }
};

/// the global instance of the model
rack::Model *modelChipSPC700 = createModel<ChipSPC700, ChipSPC700Widget>("SPC700");
