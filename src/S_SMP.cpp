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
#include "dsp/sony_s_dsp.hpp"
#include "dsp/wavetable4bit.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Sony S-DSP chip (from Nintendo SNES) emulator module.
struct ChipS_SMP : Module {
 private:
    /// the RAM for the S-DSP chip (64KB = 16-bit address space)
    uint8_t ram[Sony_S_DSP::SIZE_OF_RAM];
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

static const unsigned LENGTH = 946;
static const uint16_t synth_voice[LENGTH] = {
0xc201, 0x0200, 0x0000, 0x0000, 0x0000, 0x009a, 0x220c, 0xabff,
0x0103, 0x5341, 0x9a0f, 0xb9de, 0xf213, 0x3532, 0x1f9a, 0xd9be,
0xe022, 0x4543, 0x00eb, 0xaace, 0xf021, 0x2122, 0x01ff, 0xcdaa,
0xe002, 0x2222, 0x100f, 0xdcdf, 0xaa12, 0x2212, 0x110f, 0xfdbe,
0x02aa, 0x3211, 0x1111, 0xfdad, 0x0123, 0xaa30, 0x1111, 0xfecb,
0xe133, 0x30aa, 0x0111, 0x1fdb, 0xb033, 0x32f0, 0xaa20, 0x20ec,
0xad24, 0x330f, 0x11aa, 0x11fd, 0xbcf3, 0x342e, 0x0211, 0xaafe,
0xdcc1, 0x4330, 0x0011, 0x1faa, 0xdccf, 0x3322, 0x0010, 0x11ec,
0xaadd, 0x0333, 0x0010, 0x02fd, 0xddaa, 0xe232, 0x2001, 0x010f,
0xdecf, 0xaa33, 0x2110, 0xf012, 0xeced, 0x03aa, 0x321f, 0xf121,
0xfded, 0xe142, 0xaa11, 0xff21, 0x1fdd, 0xdf24, 0x21aa, 0xff12,
0x10de, 0xedf4, 0x311f, 0xaaf2, 0x21fd, 0xdde2, 0x4111, 0xf19a,
0x400e, 0xaabf, 0x6523, 0xef54, 0x9a0d, 0xcacd, 0xf742, 0x1f34,
0x1e9a, 0xdacf, 0xd233, 0x3124, 0x2ebd, 0x9ade, 0xef11, 0x5343,
0x0edd, 0xdf9a, 0xe0ef, 0x4544, 0x1fcc, 0xc00f, 0x9a1d, 0xf555,
0x30ac, 0xde2f, 0x1f9a, 0xd255, 0x32db, 0xecf1, 0x11ee, 0x9a35,
0x62fb, 0xced0, 0x210f, 0x039a, 0x621e, 0xbdde, 0x2300, 0x0f54,
0x9a20, 0xbcde, 0xf330, 0x1f05, 0x329a, 0xebbd, 0xe231, 0x22f2,
0x32fd, 0x9acd, 0xcf33, 0x2102, 0x311d, 0xdc8a, 0xdcf3, 0x5505,
0x71fb, 0xdcef, 0x9aee, 0x0411, 0x61ed, 0xd110, 0x0caa, 0xe120,
0x31fe, 0xe120, 0x00de, 0xa6ee, 0x0332, 0xfdf2, 0x231c, 0xcda6,
0xf332, 0x0ee1, 0x333e, 0xcde1, 0xa622, 0x0edf, 0x2452, 0xdcdf,
0x22aa, 0xfdf0, 0x332f, 0xcdf3, 0x21fe, 0xaae0, 0x2330, 0xdde0,
0x320e, 0xd09a, 0x1576, 0xcaab, 0x66ff, 0xac25, 0xaa34, 0xfdfc,
0xf42f, 0xed02, 0x24aa, 0x3dee, 0xd220, 0xfde2, 0x3240, 0xaadf,
0xdf30, 0xfee0, 0x3233, 0xeeaa, 0xed12, 0x0eee, 0x1434, 0xfefe,
0xaae1, 0x1fde, 0x0343, 0x2fee, 0xe09a, 0x11ac, 0xf366, 0x60fc,
0xdfd2, 0xaafd, 0xe034, 0x41ff, 0xe0ef, 0x1daa, 0xe012, 0x53f0,
0xef1d, 0x0fde, 0xaa12, 0x4400, 0x0e0f, 0xefee, 0xf2aa, 0x253f,
0x0ff1, 0xfdde, 0xf123, 0xaa50, 0x00f1, 0x0ddd, 0xd034, 0x41aa,
0xf001, 0x2fcb, 0xcf24, 0x43ff, 0xaa02, 0x30db, 0xbc14, 0x4310,
0xf1aa, 0x31fc, 0xbce1, 0x4520, 0x0012, 0xaa0e, 0xccdf, 0x2441,
0x0011, 0x0faa, 0xfcce, 0x0342, 0x0200, 0x1ffe, 0xaace, 0x0024,
0x1110, 0x10ef, 0xdd9a, 0x0006, 0x5140, 0xf2fc, 0xdcd1, 0x9af2,
0x5114, 0x011c, 0xddc1, 0x209a, 0x32f1, 0x113f, 0xcdc0, 0x4123,
0xaaef, 0x1012, 0xfeee, 0x2312, 0xecaa, 0x0103, 0x1eee, 0x0321,
0x2dc0, 0xaaf2, 0x31ed, 0xe232, 0x3fbe, 0xf0aa, 0x42fe, 0xee33,
0x32cb, 0x0013, 0xaa1e, 0xde14, 0x330b, 0xbf14, 0x3faa, 0xcef2,
0x442d, 0xbdf2, 0x42dd, 0xaaf0, 0x3430, 0xcbe1, 0x43fc, 0xefaa,
0x1551, 0xebcf, 0x332e, 0xce03, 0xaa54, 0x0dbc, 0x0430, 0xddf1,
0x46aa, 0x2dcc, 0xe241, 0xfce0, 0x3450, 0xaadc, 0xcf43, 0x0dcf,
0x2453, 0xedaa, 0xbc34, 0x1fcd, 0x1345, 0x1dda, 0xaaf4, 0x3fec,
0xf334, 0x5edd, 0xb1aa, 0x40ed, 0xe144, 0x51ce, 0xde11, 0xaaee,
0xf034, 0x34ee, 0xfdd0, 0xffaa, 0xf025, 0x231e, 0xffdd, 0xef00,
0xaa14, 0x333f, 0xeffb, 0xcf01, 0x13aa, 0x3231, 0x1edd, 0xcd00,
0x1331, 0xaa42, 0x10dd, 0xcbf1, 0x1231, 0x33aa, 0x03ed, 0xdbc0,
0x1141, 0x2412, 0xaa1c, 0xecae, 0x2123, 0x0431, 0x2eaa, 0xddbb,
0x1213, 0x1241, 0x21dd, 0xaabb, 0xe213, 0x3032, 0x22fc, 0xdbaa,
0xc022, 0x3121, 0x241d, 0xbdbd, 0xaa14, 0x3013, 0x231f, 0xdccb,
0x02aa, 0x3221, 0x133f, 0xfccc, 0xe032, 0xaa13, 0x1140, 0xfecd,
0xcf14, 0x129a, 0x3253, 0xeeaa, 0xbce5, 0x4361, 0xaa11, 0xf0fe,
0xefd0, 0x2142, 0x00aa, 0xff0f, 0x00dd, 0x2124, 0x1fee, 0xaa10,
0x10fc, 0x0105, 0x3fec, 0xf2a6, 0x122f, 0xcedf, 0x4530, 0xddf1,
0xaa2f, 0xc00e, 0x54fe, 0xce23, 0x11a6, 0x0ded, 0xf453, 0xfdcf,
0x022f, 0x9a00, 0xf66d, 0xbbd2, 0x415c, 0xd29a, 0xf271, 0x9bd0,
0x4130, 0xc212, 0xaa21, 0xedfe, 0x1121, 0xd022, 0x21aa, 0xfcdf,
0x1201, 0xe021, 0x32fe, 0xaabe, 0x0212, 0xfd11, 0x45fe, 0xbbaa,
0x1211, 0x1c02, 0x261e, 0xdbd1, 0xaa22, 0x2dd2, 0x154f, 0xecbf,
0x31aa, 0x20c0, 0x0363, 0xeccd, 0x0132, 0xaade, 0x0243, 0x2ebd,
0x0f13, 0xe0aa, 0xff44, 0x20dc, 0xfff4, 0xfe1e, 0xaa34, 0x11fd,
0xf0c1, 0x3e1e, 0x049a, 0x431c, 0xa0dc, 0x6ef2, 0xd453, 0x9a10,
0xcd0a, 0x12d3, 0xf144, 0x2f9a, 0xfc0e, 0xd1d2, 0x1f43, 0x4fe0,
0x9aef, 0xc0f0, 0x1f41, 0x41d1, 0x0f9a, 0xdfee, 0x2f23, 0x24ed,
0x11ef, 0x9a0a, 0x0205, 0x022c, 0xf31d, 0x1ba6, 0xcedf, 0x1122,
0x0131, 0x11dd, 0x9a00, 0x6210, 0xce52, 0xd3b9, 0x21a6, 0x0211,
0x1ff3, 0x222d, 0xcde1, 0x9643, 0x2fc2, 0x5361, 0x99bf, 0x3396,
0x20cc, 0x4655, 0xfa9c, 0x0440, 0x9ace, 0x4711, 0xcad3, 0x420d,
0xce9a, 0x1741, 0xfcb0, 0x52fd, 0xbe15, 0xaa31, 0xfff0, 0x12fd,
0xdf12, 0x41aa, 0xfe01, 0x22fc, 0xce03, 0x42ff, 0xaae1, 0x330d,
0xbcf2, 0x440f, 0xefaa, 0x242f, 0xbae1, 0x352e, 0xfe04, 0xaa40,
0xeabf, 0x245f, 0xeff2, 0x52aa, 0xecae, 0x1153, 0xfeef, 0x550d,
0xaabb, 0xf136, 0x0eef, 0x153f, 0xdbaa, 0xb026, 0x3edf, 0x0451,
0xdbbe, 0xaa13, 0x61de, 0xf144, 0x1cac, 0x01aa, 0x54ed, 0xf124,
0x3ebb, 0xf025, 0xaa1e, 0xef13, 0x51cb, 0xdf04, 0x4faa, 0xee12,
0x33fc, 0xcef2, 0x41fe, 0xaaf2, 0x330d, 0xcf00, 0x21ff, 0xf29a,
0x631e, 0xab11, 0x12ee, 0xf255, 0x9a0e, 0xcc02, 0x11ed, 0xf055,
0x3d9a, 0xcef2, 0x11ec, 0xf026, 0x4fcd, 0x9af2, 0x22fa, 0xe005,
0x62cb, 0xe29a, 0x321d, 0xaf02, 0x65ec, 0xb042, 0xaa11, 0xce00,
0x330e, 0xef12, 0x12aa, 0xecf0, 0x1320, 0xedf3, 0x221c, 0xaac0,
0x0232, 0xede1, 0x331f, 0xbeaa, 0xf232, 0x0edf, 0x2322, 0xdcef,
0xaa24, 0x2ede, 0x0431, 0x0cdf, 0x03aa, 0x30de, 0xf143, 0x1ece,
0xe242, 0xaaed, 0xef35, 0x3fdd, 0xdf34, 0x0eaa, 0xde15, 0x42dd,
0xde13, 0x2eee, 0xaaf3, 0x54fd, 0xedf2, 0x3fdd, 0xf3aa, 0x451e,
0xdfe1, 0x10dd, 0xe245, 0xaa30, 0xedf0, 0x10dc, 0xd234, 0x52aa,
0xeeff, 0x10eb, 0xc034, 0x54fe, 0xaaff, 0x010c, 0x9d34, 0x462d,
0xefaa, 0x110e, 0xaa04, 0x4450, 0xeef2, 0xaa1f, 0xc9c2, 0x5462,
0xeee1, 0x20aa, 0xeaaf, 0x4365, 0xfee0, 0x21fc, 0xbace, 0x1323,
0x0f0f, 0x011f, 0xdcaa, 0xe447, 0x4ffe, 0x011f, 0xd9a2, 0xba32,
0x30ff, 0x0110, 0xfdcf, 0x31ba, 0x320f, 0x0f11, 0x0ecd, 0x1233,
0xba0f, 0x0f11, 0x0fdc, 0xf322, 0x20aa, 0xee04, 0x2dca, 0x9255,
0x70df, 0xbaf1, 0x21ed, 0xce32, 0x42fe, 0xf0ba, 0x22fe, 0xcd13,
0x230f, 0xff12, 0xba1e, 0xdcf2, 0x332e, 0xff02, 0x2fba, 0xecd1,
0x3230, 0xfef1, 0x31fd, 0xbabe, 0x3332, 0xefe1, 0x22fe, 0xcdba,
0x1323, 0x0fef, 0x220f, 0xece2, 0xaa57, 0x4cde, 0x143f, 0xc9a2,
0x54aa, 0x60de, 0xe241, 0xec9d, 0x4364, 0xaade, 0xff23, 0x0dab,
0x2436, 0x0caa, 0xff13, 0x1ebb, 0xf435, 0x3ce0, 0xaaf3, 0x1fdc,
0xd233, 0x5fd0, 0xe1aa, 0x21ec, 0xb142, 0x42df, 0xf021, 0xaaed,
0xbf43, 0x34ed, 0xf021, 0xfdaa, 0xcd34, 0x240d, 0xff12, 0x0ecc,
0xaaf5, 0x352c, 0xef03, 0x1ecb, 0xe4aa, 0x444f, 0xdef2, 0x1fec,
0xb154, 0xaa42, 0xeee0, 0x21dc, 0xce35, 0x52aa, 0x1eef, 0x02ec,
0xce14, 0x4330, 0xaaef, 0xe10c, 0xdef2, 0x4332, 0xf0aa, 0xef1c,
0xcff0, 0x3423, 0x010c, 0xaa1e, 0xbe00, 0x1323, 0x203d, 0xd0aa,
0xccf1, 0x1312, 0x3030, 0xc0ea, 0xaad1, 0x2221, 0x3023, 0xedfb,
0xc0aa, 0x2121, 0x3114, 0xfdfd, 0xae23, 0xaa11, 0x2104, 0x3edd,
0xcb03, 0x40aa, 0x12f3, 0x5fdd, 0xdbd2, 0x52f3, 0xaaf0, 0x53fc,
0xbddf, 0x3402, 0x1eaa, 0x530e, 0xbcce, 0x3510, 0x3d15, 0xaa20,
0xcbcd, 0xf64f, 0x2ff4, 0x32aa, 0xe9cd, 0xf351, 0x1ff3, 0x310c,
0xaacd, 0xd142, 0x20e2, 0x222d, 0xcc9a, 0xb071, 0x53b4, 0x423e,
0xaaad, 0x9a61, 0x26c1, 0x6040, 0xbbca, 0x349a, 0xe7fe, 0x5052,
0xcbdb, 0xe503, 0x9a2c, 0x5214, 0xfbdc, 0xc50f, 0x5d9a, 0x25f4,
0x1cce, 0xb13e, 0x5fe6, 0x9a12, 0x1ecf, 0xcd5e, 0x23c4, 0x309a,
0x3fde, 0xdc30, 0x04d1, 0x503f, 0x9ade, 0xfc03, 0xd40d, 0x6112,
0xdd9a, 0x0dd5, 0xef4d, 0x2412, 0xede0, 0x9ac2, 0x2d4f, 0xe512,
0x1cd0, 0xef9a, 0x5d02, 0xc531, 0x1dcf, 0x1d41, 0x9ae1, 0xd250,
0x2fcd, 0x0f15, 0xd09a, 0xfe53, 0x2fcb, 0x01f4, 0x1f2c, 0x9a04,
0x22fc, 0xae03, 0x71ff, 0xc1aa, 0x220e, 0xecf1, 0x44f0, 0xfe03,
0xaa10, 0xebff, 0x0424, 0x2fee, 0xef9a, 0x32aa, 0xf466, 0x61ec,
0xc0d2, 0xaaed, 0xf133, 0x310f, 0xd0f0, 0x0dab, 0xd023, 0x42f1,
0xef0e, 0xffdf};
static const uint8_t* synth_voice_bytes = reinterpret_cast<const uint8_t*>(synth_voice);

for (unsigned block_index = 0; block_index < LENGTH / 8; block_index++) {
    auto addr = &ram[0x7804 + block_index * sizeof(Sony_S_DSP::BitRateReductionBlock)];
    auto block = reinterpret_cast<Sony_S_DSP::BitRateReductionBlock*>(addr);
    block->flags.set_volume(Sony_S_DSP::BitRateReductionBlock::MAX_VOLUME);
    block->flags.filter = 0;
    auto is_end = block_index == (LENGTH / 8) - 1;
    block->flags.is_loop = is_end;
    block->flags.is_end = is_end;
    for (unsigned sample = 0; sample < 8; sample++) {
        block->samples[sample] = synth_voice[block_index * 8 + sample] >> 8;
    }
}





        // set address 256 to a single sample ramp wave sample in BRR format
        // the header for the BRR single sample waveform
        // auto block = reinterpret_cast<Sony_S_DSP::BitRateReductionBlock*>(&ram[0x7804]);
        // block->flags.set_volume(Sony_S_DSP::BitRateReductionBlock::MAX_VOLUME);
        // block->flags.filter = 0;
        // block->flags.is_loop = 1;
        // block->flags.is_end = 1;
        // for (unsigned i = 0; i < Sony_S_DSP::BitRateReductionBlock::NUM_SAMPLES; i++)
        //     block->samples[i] = 15 + 2 * i;
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
