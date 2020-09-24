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
        ENUMS(PARAM_FREQ,       Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_NOISE_FREQ, Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_VOLUME_L,   Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_VOLUME_R,   Sony_S_DSP::VOICE_COUNT),
        ENUMS(PARAM_VOLUME_MAIN, 2),
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        ENUMS(INPUT_VOCT,     Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_NOISE_FM, Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_FM,       Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_GATE,     Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_VOLUME_L, Sony_S_DSP::VOICE_COUNT),
        ENUMS(INPUT_VOLUME_R, Sony_S_DSP::VOICE_COUNT),
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
            auto osc_name = std::to_string(osc + 1);
            configParam(PARAM_FREQ + osc, -4.f, 4.f, 0.f, "Voice " + osc_name + " Frequency", " Hz", 2, dsp::FREQ_C4);
            configParam(PARAM_NOISE_FREQ + osc, 0, 32, 0, "Voice " + osc_name + " Noise Frequency");
            configParam(PARAM_VOLUME_L + osc, -128, 127, 0, "Voice " + osc_name + " Volume (Left)");
            configParam(PARAM_VOLUME_R + osc, -128, 127, 0, "Voice " + osc_name + " Volume (Right)");
        }
        configParam(PARAM_VOLUME_MAIN + 0, -128, 127, 0, "Main Volume (Left)");
        configParam(PARAM_VOLUME_MAIN + 1, -128, 127, 0, "Main Volume (Right)");

        // clear the shared RAM between the CPU and the S-DSP
        clearRAM();
        // reset the S-DSP emulator
        apu.reset();

        // apu.run(1, NULL);
        processCV();
    }

 protected:
    inline void processCV() {
        // Main/Echo volume! 8-bit signed values. Regular sound is scaled by
        // the main volume. Echoed sound is scaled by the echo volume.
        //
        // I also had a problem with writing to these registers, sometimes my
        // writes would get ignored, or zeroed?
        //
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $0C   | sign|         Left Output Main Volume         |
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        //
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $1C   | sign|        Right Output Main Volume         |
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        //
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $2C   | sign|         Left Output Echo Volume         |
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        //
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $3C   | sign|        Right Output Echo Volume         |
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        apu.write(Sony_S_DSP::MAIN_VOLUME_LEFT,  127);
        apu.write(Sony_S_DSP::MAIN_VOLUME_RIGHT, 127);
        apu.write(Sony_S_DSP::ECHO_VOLUME_LEFT,  127);
        apu.write(Sony_S_DSP::ECHO_VOLUME_RIGHT, 127);
        // Writing bits to KON will start / restart the voice specified.
        // Writing bits to KOF will cause the voice to fade out. The fade is
        // done with subtraction of 1/256 values and takes about 8msec.
        //
        // It is said that you should not write to KON/KOF in succession, you
        // have to wait a little while (a few NOPs).
        //
        // Key-On
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $4C   |VOIC7|VOIC6|VOIC5|VOIC4|VOIC3|VOIC2|VOIC1|VOIC0|
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // Key-Off
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $5C   |VOIC7|VOIC6|VOIC5|VOIC4|VOIC3|VOIC2|VOIC1|VOIC0|
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // apu.write(Sony_S_DSP::KEY_ON, 0xff);
        // apu.write(Sony_S_DSP::KEY_OFF, 0);

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
        apu.write(Sony_S_DSP::PITCH_MODULATION,  0);
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
        apu.write(Sony_S_DSP::NOISE_ENABLE, 0xff);
        // Echo enable.
        //
        // EON
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $4D   |VOIC7|VOIC6|VOIC5|VOIC4|VOIC3|VOIC2|VOIC1|VOIC0|
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // This register enables echo effects for the specified channel(s).
        apu.write(Sony_S_DSP::ECHO_ENABLE, 0);
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
        apu.write(Sony_S_DSP::OFFSET_SOURCE_DIRECTORY, 0);
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
            // Volume, 8-bit signed value.
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x0   | sign|              Volume Left                |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x1   | sign|              Volume Right               |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            apu.write(mask | Sony_S_DSP::VOLUME_LEFT,  127);
            apu.write(mask | Sony_S_DSP::VOLUME_RIGHT, 127);
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x2   |               Lower 8-bits of Pitch           |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x3   |  -  |  -  |   Higher 6-bits of Pitch          |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // Pitch is a 14-bit value.
            apu.write(mask | Sony_S_DSP::PITCH_LOW,  0xff & Sony_S_DSP::convert_pitch(262)     );
            apu.write(mask | Sony_S_DSP::PITCH_HIGH, 0xff & Sony_S_DSP::convert_pitch(262) >> 8);
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
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x5   |ENABL|        DR       |          AR           |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x6   |     SL          |          SR                 |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // The ENABL bit determines which envelope mode to use. If this bit
            // is set then ADSR is used, otherwise GAIN is operative.
            //
            // My knowledge about DSP stuff is a bit low. Some enlightenment on
            // how the ADSR works would be greatly appreciated!
            // Table 2.2 ADSR Parameters
            //
            // AR  TIME FROM 0->1    DR  TIME FROM 1->SL    SL  RATIO    SR  TIME FROM 0->1
            // 00  4.1 sec           00  1.2 sec            00  1/8      00  Infinite
            // 01  2.5 sec           01  740 msec           01  2/8      01   38 sec
            // 02  1.5 sec           02  440 msec           02  3/8      02   28 sec
            // 03  1.0 sec           03  290 msec           03  4/8      03   24 sec
            // 04  640 msec          04  180 msec           04  5/8      04   19 sec
            // 05  380 msec          05  110 msec           05  6/8      05   14 sec
            // 06  260 msec          06   74 msec           06  7/8      06   12 sec
            // 07  160 msec          07   37 msec           07  8/8      07  9.4 sec
            // 08   96 msec                                              08  7.1 sec
            // 09   64 msec                                              09  5.9 sec
            // 0A   40 msec                                              0A  4.7 sec
            // 0B   24 msec                                              0B  3.5 sec
            // 0C   16 msec                                              0C  2.9 sec
            // 0D   10 msec                                              0D  2.4 sec
            // 0E    6 msec                                              0E  1.8 sec
            // 0F    0 msec                                              0F  1.5 sec
            //                                                           10  1.2 sec
            //                                                           11  880 msec
            //                                                           12  740 msec
            //      |                                                    13  590 msec
            //      |                                                    14  440 msec
            //    1 |--------                                            15  370 msec
            //      |       /\                                           16  290 msec
            //      |      /| \                                          17  220 msec
            //      |     / |  \                                         18  180 msec
            //      |    /  |   \                                        19  150 msec
            //    SL|---/---|-----\__                                    1A  110 msec
            //      |  /    |    |   \___                                1B   92 msec
            //      | /     |    |       \_________________              1C   74 msec
            //      |/AR    | DR |           SR            \   t         1D   55 msec
            //      |-------------------------------------------         1E   37 msec
            //      0                                      |             1F   18 msec
            //
            //      key on                                Key off
            // (cool ascii picture taken from "APU MANUAL IN TEXT BY LEDI")
            apu.write(mask | Sony_S_DSP::ADSR_1, 0b10100100);
            apu.write(mask | Sony_S_DSP::ADSR_2, 0b01000100);
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
            // OUTX is written to by the DSP. It contains the present wave
            // height multiplied by the ADSR/GAIN envelope value. It isn't
            // multiplied by the voice volume though.
            // OUTX
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $x9   | sign|                 VALUE                   |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            //
            // 8-bit signed value
            // apu.write(mask | Sony_S_DSP::WAVEFORM_OUT, 0);
            // Echo FIR filter coefficients.
            //
            // COEF
            //          7     6     5     4     3     2     1     0
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // $xF   | sign|         Filter Coefficient X            |
            //       +-----+-----+-----+-----+-----+-----+-----+-----+
            // The 8-bytes at $0F,$1F,$2F,$3F,$4F,$5F,$6F,$7F are used by the
            // 8-tap FIR filter. I really have no idea how FIR filters work...
            // but the filter is applied to the echo output.
            // apu.write(mask | Sony_S_DSP::FIR_COEFFICIENTS, 0);
        }
    }

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    inline void process(const ProcessArgs &args) final {
        // Flags
        //          7     6     5     4     3     2     1     0
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // $6C   |RESET|MUTE |~ECEN|         NOISE CLOCK         |
        //       +-----+-----+-----+-----+-----+-----+-----+-----+
        // RESET: Soft reset. Writing a '1' here will set all voices in a
        //        state of "Key-On suspension" (???). MUTE is also set. A
        //        soft-reset gets triggered upon power-on.
        //
        // MUTE: Mutes all channel output.
        //
        // ECEN: ~Echo enable. A '0' here enables echo data to be written into
        //       external memory (the memory your program/data is in!). Be
        //       careful when enabling it, it's quite easy to crash your
        //       program with the echo hardware!
        //
        // NOISE CLOCK: Designates the frequency for the white noise.
        // Table: Noise clock value to frequency conversion
        // Value   Frequency
        // 00  0 Hz
        // 01  16 Hz
        // 02  21 Hz
        // 03  25 Hz
        // 04  31 Hz
        // 05  42 Hz
        // 06  50 Hz
        // 07  63 Hz
        // 08  83 Hz
        // 09  100 Hz
        // 0A  125 Hz
        // 0B  167 Hz
        // 0C  200 Hz
        // 0D  250 Hz
        // 0E  333 Hz
        // 0F  400 Hz
        // 10  500 Hz
        // 11  667 Hz
        // 12  800 Hz
        // 13  1.0 KHz
        // 14  1.3 KHz
        // 15  1.6 KHz
        // 16  2.0 KHz
        // 17  2.7 KHz
        // 18  3.2 KHz
        // 19  4.0 KHz
        // 1A  5.3 KHz
        // 1B  6.4 KHz
        // 1C  8.0 KHz
        // 1D  10.7 KHz
        // 1E  16 KHz
        // 1F  32 KHz
        auto noise = params[PARAM_NOISE_FREQ].getValue();
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
            key_on = key_on   | (gateTriggers[voice][0].process(rescale(gate,        0.f, 2.f, 0.f, 1.f)) << voice);
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
        // MARK: Frequency
        // -------------------------------------------------------------------
        // for (unsigned voice = 0; voice < Sony_S_DSP::VOICE_COUNT; voice++) {
        //     // shift the voice index over a nibble to get the bit mask for the
        //     // logical OR operator
        //     auto mask = voice << 4;

        // }
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
        static constexpr auto panel = "res/SPC700.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // individual oscillator controls
        for (unsigned i = 0; i < Sony_S_DSP::VOICE_COUNT; i++) {
            // frequency
            addInput(createInput<PJ301MPort>(  Vec(15, 40 + i * 41),  module, ChipSPC700::INPUT_VOCT + i      ));
            addInput(createInput<PJ301MPort>(  Vec(45, 40 + i * 41),  module, ChipSPC700::INPUT_FM + i        ));
            addParam(createParam<Rogan2PSNES>( Vec(75, 35 + i * 41),  module, ChipSPC700::PARAM_FREQ + i      ));
            // noise frequency
            addInput(createInput<PJ301MPort>(  Vec(115, 40 + i * 41), module, ChipSPC700::INPUT_NOISE_FM + i  ));
            addParam(createParam<Rogan2PSNES>( Vec(145, 35 + i * 41), module, ChipSPC700::PARAM_NOISE_FREQ + i));
            // gate
            addInput(createInput<PJ301MPort>(  Vec(185, 40 + i * 41), module, ChipSPC700::INPUT_GATE + i      ));
            // volume
            addParam(createParam<Rogan2PWhite>(Vec(220, 35 + i * 41), module, ChipSPC700::PARAM_VOLUME_L + i  ));
            addInput(createInput<PJ301MPort>(  Vec(260, 40 + i * 41), module, ChipSPC700::INPUT_VOLUME_L + i  ));
            addParam(createParam<Rogan2PRed>(  Vec(300, 35 + i * 41), module, ChipSPC700::PARAM_VOLUME_R + i  ));
            addInput(createInput<PJ301MPort>(  Vec(340, 40 + i * 41), module, ChipSPC700::INPUT_VOLUME_R + i  ));
        }
        // left channel output
        addParam(createParam<Rogan2PWhite>(Vec(390, 230), module, ChipSPC700::PARAM_VOLUME_MAIN + 0));
        addInput(createInput<PJ301MPort>(  Vec(400, 280), module, ChipSPC700::INPUT_VOLUME_MAIN + 0));
        addOutput(createOutput<PJ301MPort>(Vec(400, 325), module, ChipSPC700::OUTPUT_AUDIO + 0   ));
        // right channel output
        addParam(createParam<Rogan2PRed>(  Vec(440, 230), module, ChipSPC700::PARAM_VOLUME_MAIN + 1));
        addInput(createInput<PJ301MPort>(  Vec(430, 280), module, ChipSPC700::INPUT_VOLUME_MAIN + 1));
        addOutput(createOutput<PJ301MPort>(Vec(430, 325), module, ChipSPC700::OUTPUT_AUDIO + 1   ));
    }
};

/// the global instance of the model
rack::Model *modelChipSPC700 = createModel<ChipSPC700, ChipSPC700Widget>("SPC700");
