<p align="center">
<img alt="Potato Chips" src="manual/PotatoChips-SocialMedia.svg">
</p>

[![Travis CI Build Status][BuildStatus]][BuildServer]

[BuildStatus]:  https://travis-ci.org/Kautenja/PotatoChips.svg?branch=master
[BuildServer]:  https://travis-ci.org/Kautenja/PotatoChips

These retro sound chips are so good,
[I eat 'em like they're potato chips.][SamHyde-PotatoChips]
They're just so addicting!

[SamHyde-PotatoChips]: https://www.youtube.com/watch?v=lL5M-vXq58c

<!-- ------------------------------------------------------------ -->
<!-- MARK: 106 -->
<!-- ------------------------------------------------------------ -->

## 106

106 is an emulation of the
[Namco 106](https://wiki.nesdev.com/w/index.php?title=Namco_163_audio&redirect=no)
audio processing unit from the
[Nintendo Entertainment System (NES)](https://en.wikipedia.org/wiki/Nintendo_Entertainment_System)
for VCV Rack. The Namco 106 chip contains eight channels of wave-table
synthesis and 128 bytes of operational RAM. The wave-tables are 4-bit
and can be as long as 63 samples. This module uses a bank of five
32-sample wave-tables to act as the waveform for all eight channels.

<p align="center">
<img alt="106" src="manual/106/img/106-Module.svg">
</p>

### Features

-   **Wave-table synthesis:** 8 channels of wave-table synthesis with bit depth
    of 4 bits and table size of 32 samples
-   **Waveform morph:** 5 banks of wave-tables to morph between using 32-bit
    floating point linear interpolation (not very retro, but it sounds nice)
-   **Frequency control:** 18-bit frequency control with linear frequency modulation
-   **Amplitude modulation:** 4-bit amplifier with linear amplitude modulation
-   **Namco 106 compute limitation:** activating each additional channel (up
    to 8) reduces the amount of compute available for all channels. This causes
    all channels to drop in frequency when additional channels are activated.

See the [Manual][106] for more information about the features of this module.

[106]: https://github.com/Kautenja/PotatoChips/releases/latest/download/106.pdf

<!-- ------------------------------------------------------------ -->
<!-- MARK: 2612 -->
<!-- ------------------------------------------------------------ -->

## 2612

2612 is an emulation of the
[Yamaha YM2612](https://en.wikipedia.org/wiki/Yamaha_YM2612)
audio processing unit from the
[Sega Master System](https://en.wikipedia.org/wiki/Master_System)
and
[Sega Genesis](https://en.wikipedia.org/wiki/Sega_Genesis)
for VCV Rack. The YM2612 is a 4-operator FM synthesis chip with 6 voices of
polyphony.

<p align="center">
<img alt="2612" src="manual/2612/img/2612-Module.svg">
</p>

### Features

-   **16-bit:** 8 bits better than the previous generation of chips!
-   **6 Voice Polyphony:** 6 voices of polyphony with independent V/OCT and
    Gate inputs
-   **4-Operator FM Synthesis:** Full control over the FM-synthesis parameters
    for each of the four operators including: envelopes, multiplier rate scale,
    tuning, and amplitude modulation
-   **8 FM Algorithms:** 8 different arrangements of the four operators
-   **Stereo Outputs:** Output from the left and right channels on the chip

See the [Manual][2612] for more information about the features of this module.

[2612]: https://github.com/Kautenja/PotatoChips/releases/latest/download/2612.pdf

<!-- ------------------------------------------------------------ -->
<!-- MARK: 2A03 -->
<!-- ------------------------------------------------------------ -->

## 2A03

2A03 is an emulation of the
[Ricoh 2A03](https://wiki.nesdev.com/w/index.php/2A03)
audio processing unit from the
[Nintendo Entertainment System (NES)](https://en.wikipedia.org/wiki/Nintendo_Entertainment_System)
for VCV Rack. The 2A03 chip contains two pulse wave
generators, a quantized triangle wave generator, and a noise generator. The
original chip featured a DMC loader for playing samples that has been omitted
in this emulation.

<p align="center">
<img alt="2A03" src="manual/2A03/img/2A03-Module.svg">
</p>

### Features

-   **Dual pulse wave generator:** Dual 8-bit pulse waves with four duty
    cycles: _12.5%_, _25%_, _50%_, and _75%_
-   **Quantized triangle wave generator:** Generate NES style triangle wave
    with 16 steps of quantization
-   **Noise generator:** generate pseudo-random numbers at 16 different
    frequencies
-   **Linear Feedback Shift Register (LFSR):** old-school 8-bit randomness!

See the [Manual][2A03] for more information about the features of this module.

[2A03]: https://github.com/Kautenja/PotatoChips/releases/latest/download/2A03.pdf

<!-- ------------------------------------------------------------ -->
<!-- MARK: AY-3-8910 -->
<!-- ------------------------------------------------------------ -->

## AY-3-8910

AY-3-8910 is an emulation of the
[General Instrument AY-3-8910](http://map.grauw.nl/resources/sound/generalinstrument_ay-3-8910.pdf)
audio processing
unit. The AY-3-8910 features three pulse waveform generators and a noise
generator that is shared between the channels.

<p align="center">
<img alt="AY-3-8910" src="manual/AY-3-8910/img/AY_3_8910-Module.svg">
</p>

### Features

-   **Triple pulse wave generator:** Triple 12-bit pulse waves with duty cycle of _50%_
-   **Amplitude modulation:** Manual and CV control over the individual voice levels
-   **White noise:** Generate noise using the frequency knob for channel 3
-   **Tone/Noise control:** CV and switch to control tone and noise for each channel

See the [Manual][AY_3_8910] for more information about the features of this module.

[AY_3_8910]: https://github.com/Kautenja/PotatoChips/releases/latest/download/AY_3_8910.pdf

<!-- ------------------------------------------------------------ -->
<!-- MARK: FME7 -->
<!-- ------------------------------------------------------------ -->

## FME7

FME7 is an emulation of the
[Sunsoft FME7](https://wiki.nesdev.com/w/index.php/Sunsoft_5B_audio)
audio processing unit from the
[Nintendo Entertainment System (NES)](https://en.wikipedia.org/wiki/Nintendo_Entertainment_System)
for VCV Rack. The FME7 chip contains three
pulse wave generators, a noise generator, and an envelope generator. Only the
pulse wave generators are implemented currently.

<p align="center">
<img alt="FME7" src="manual/FME7/img/FME7-Module.svg">
</p>

### Features

-   **Triple pulse wave generator:** Triple 12-bit pulse waves with duty cycle of _50%_
-   **Amplitude modulation:** Manual and CV control over the individual voice levels

See the [Manual][FME7] for more information about the features of this module.

[FME7]: https://github.com/Kautenja/PotatoChips/releases/latest/download/FME7.pdf

<!-- ------------------------------------------------------------ -->
<!-- MARK: GBS -->
<!-- ------------------------------------------------------------ -->

## GBS

GBS is an emulation of the
[Nintendo GameBoy Sound System (GBS)](https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware)
audio processing
unit. The GBS is similar to the Ricoh 2A03, but replaces the triangle waveform
generator with a wave-table synthesizer.

<p align="center">
<img alt="GBS" src="manual/GBS/img/GBS-Module.svg">
</p>

### Features

-   **Dual pulse wave generator:** Dual 8-bit pulse waves with four duty
    cycles: _12.5%_, _25%_, _50%_, and _75%_
-   **Wave-table synthesis channel:** wave-table synthesis with bit depth
    of 4 bits and table size of 32 samples. 5 pages of wave-tables can be
    interpolated between using CV
-   **Noise generator:** generate pseudo-random numbers at 7 different
    frequencies
-   **Linear Feedback Shift Register (LFSR):** old-school 8-bit randomness!

See the [Manual][GBS] for more information about the features of this module.

[GBS]: https://github.com/Kautenja/PotatoChips/releases/latest/download/GBS.pdf

<!-- ------------------------------------------------------------ -->
<!-- MARK: POKEY -->
<!-- ------------------------------------------------------------ -->

## POKEY

POKEY is an emulation of the
[Atari POKEY](https://en.wikipedia.org/wiki/POKEY)
audio processing unit. The POKEY
produces four pulse waveforms, but contains a variety of bonus controls,
including extended frequency ranges, high-pass filters, and noise generators /
distortion effects.

<p align="center">
<img alt="POKEY" src="manual/POKEY/img/POKEY-Module.svg">
</p>

### Features

-   **Quad pulse wave generator:** Four pulse waves with 8-bit frequency value
    and _50%_ pulse width
-   **Low-frequency mode:** Change base clock of the chip from
    _64 KHz_ to _15 KHz_
-   **High-frequency mode:** Change base clock of channels 1 and 3 from
    _64 KHz_ to _1.79 MHz_
-   **High-pass filter:** High-pass filter channel 1 using channel 3 as a clock
    or high-pass channel 2 using channel 4 as a clock
-   **Linear Feedback Shift Register (LFSR):** old-school 8-bit randomness!
-   **Noise/Distortion generator:** generate per-channel pseudo-random numbers
    at 15 different frequencies as a distortion source
-   **Amplitude modulation:** 4-bit amplifier with linear amplitude modulation

See the [Manual][POKEY] for more information about the features of this module.

[POKEY]: https://github.com/Kautenja/PotatoChips/releases/latest/download/POKEY.pdf

<!-- ------------------------------------------------------------ -->
<!-- MARK: S-SMP-ADSR -->
<!-- ------------------------------------------------------------ -->

## S-SMP-ADSR

S-SMP-ADSR is an emulation of the ADSR from the Sony S-SMP audio processing
unit in the
[Super Nintendo Entertainment System (SNES)](https://en.wikipedia.org/wiki/Super_Nintendo_Entertainment_System).
The envelope generator has three stages, (1) an attack stage that ramps up
linearly to the total level, (2) a decay stage that ramps down exponentially
to a sustain level, and (3) a sustain/release stage that ramps down
exponentially from the sustain level to zero.

S-SMP(ADSR) provides the key features of the ADSR envelope generator of the
S-SMP chip, namely,
-   **_32KHz_ Sample Rate:** The S-SMP was designed to run at _32kHz_, so the
    audio inputs and outputs of the module are locked to _32kHz_.
-   **Stereo Processing:** Two envelope generators on one module for
    stereo modulation, cross modulation, or both!
-   **Total Level Control:** Control over the overall level of the envelope
    generator, including inversion for ducking effects.
-   **Stage Length Control:** Control over stage timings using sliders and
    control voltages.

<p align="center">
<img alt="S-SMP-ADSR" src="manual/S_SMP_ADSR/img/S-SMP-ADSR-Module.svg">
</p>

### Features

See the [Manual][S_SMP_ADSR] for more information about the features of this module.

[S_SMP_ADSR]: https://github.com/Kautenja/PotatoChips/releases/latest/download/S_SMP_ADSR.pdf

<!-- ------------------------------------------------------------ -->
<!-- MARK: S-SMP(Echo) -->
<!-- ------------------------------------------------------------ -->

## S-SMP(Echo)

S-SMP(Echo) is a Eurorack module that emulates the echo effect from the S-SMP
sound chip on the
[Super Nintendo Entertainment System (SNES)](https://en.wikipedia.org/wiki/Super_Nintendo_Entertainment_System).
The Echo effect of the S-SMP chip has _15_ different delay levels of _16ms_
each, a _64KB_ echo buffer, an 8-tap FIR filter for shaping the sound of the
echo, parameterized feedback, and parameterized dry / wet mix level. The echo
buffer is stereo, although the echo parameters and coefficients of the FIR
filter are the same for both channels.

S-SMP(Echo) provides the key features of the echo module of the S-SMP chip,
namely,
-   **_32KHz_ Sample Rate:** The S-SMP was designed to run at _32kHz_, so the
    audio inputs and outputs of the module are locked to _32kHz_.
-   **Stereo Processing:** Echo buffer for two independent inputs in stereo
    configuration. The parameters are the same for both inputs, but the inputs
    have their own dedicated echo buffers.
-   **Expanded Delay:** The 15 levels of delay has been upgraded to 31 levels
    that each add an additional _16ms_ of delay (up to roughly _500ms_). 31
    levels of delay is able to fit in the RAM of the original S-SMP, but the
    instruction set does not normally support the addressing of 31 levels.
-   **Feedback:** Additive and subtractive feedback following the original
    implementation
-   **Surround Effect:** Stereo mixer with the ability to invert the phase of
    either channel resulting in odd Haas effects.
-   **8-tap FIR Filter:** Fully parameterized 8-tap FIR filter for shaping the
    sound of the echo. The filter can be parameterized as low-pass, high-pass,
    band-pass, band-stop, etc. and includes presets with filter parameters from
    popular SNES games.

<p align="center">
<img alt="S-SMP(Echo)" src="manual/S_SMP_Echo/img/S-SMP-Echo-Module.svg">
</p>

### Features

See the [Manual][S_SMP_Echo] for more information about the features of this module.

[S_SMP_Echo]: https://github.com/Kautenja/PotatoChips/releases/latest/download/S_SMP_Echo.pdf

<!-- ------------------------------------------------------------ -->
<!-- MARK: S-SMP-Gauss -->
<!-- ------------------------------------------------------------ -->

## S-SMP-Gauss

S-SMP-Gauss is an emulation of the BRR filter & Gaussian filter from the Sony
S-SMP audio processing unit in the
[Super Nintendo Entertainment System (SNES)](https://en.wikipedia.org/wiki/Super_Nintendo_Entertainment_System).
The four BRR filter modes were applied to BRR sample blocks on the SNES and the
Gaussian filter was applied to output audio and removed high-frequency content
from the signal.

S-SMP(Gauss) provides the key features of the BRR filter and Gaussian filter
of the S-SMP chip, namely,
-   **_32KHz_ Sample Rate:** The S-SMP was designed to run at _32kHz_, so the
    audio inputs and outputs of the module are locked to _32kHz_.
-   **Stereo Processing:** Dual processing channels for stereo effects or other
    create multi-tracking applications.
-   **4 BRR Filter Modes:** 4 filter modes from the BRR sample playback engine
    that act as low-pass filters.
-   **Gaussian Interpolation Filter:** A filter that removes high-frequency
    content and adds subtle distortion. This filter provides the muffling
    character that fans of SNES audio will find familiar.

<p align="center">
<img alt="S-SMP-Gauss" src="manual/S_SMP_Gauss/img/S-SMP-Gauss-Module.svg">
</p>

### Features

See the [Manual][S_SMP_Gauss] for more information about the features of this module.

[S_SMP_Gauss]: https://github.com/Kautenja/PotatoChips/releases/latest/download/S_SMP_Gauss.pdf

<!-- ------------------------------------------------------------ -->
<!-- MARK: S-SMP -->
<!-- ------------------------------------------------------------ -->

<!-- ## S-SMP _(Coming Soon!)_

S-SMP is an emulation of the Sony S-SMP audio processing unit from the
[Super Nintendo Entertainment System (SNES)](https://en.wikipedia.org/wiki/Super_Nintendo_Entertainment_System).
The S-SMP is a complex module containing a DSP chip (the S-DSP), a CPU chip (
the SPC700), 64KB of total RAM storage, an amplifier, and a DAC.

<p align="center">
<img alt="S-SMP" src="manual/S_SMP/img/S_SMP-Module.svg">
</p>

### Features

See the [Manual][S_SMP] for more information about the features of this module.

[S_SMP]: https://github.com/Kautenja/PotatoChips/releases/latest/download/S_SMP.pdf -->

<!-- ------------------------------------------------------------ -->
<!-- MARK: SCC -->
<!-- ------------------------------------------------------------ -->

<!--
## SCC _(Coming Soon!)_

SCC is an emulation of the Konami SCC audio processing unit.

<p align="center">
<img alt="SCC" src="manual/SCC/img/SCC-Module.svg">
</p>

### Features

See the [Manual][SCC] for more information about the features of this module.

[SCC]: https://github.com/Kautenja/PotatoChips/releases/latest/download/SCC.pdf
-->

<!-- ------------------------------------------------------------ -->
<!-- MARK: SN76489 -->
<!-- ------------------------------------------------------------ -->
## SN76489

SN76489 is an emulation of the [Texas Instruments SN76489][TI-SN76489] audio
processing unit from the [Sega Master System][SegaMasterSystem] for VCV Rack.
The SN76489 chip contains three pulse waveform generators and a noise generator
that selects between white-noise and periodic noise (LFSR).

<p align="center">
<img alt="SN76489" src="manual/SN76489/img/SN76489-Module.svg">
</p>

### Features

-   **Triple pulse wave generator:** Triple 8-bit pulse waves with _50%_ duty
    cycle and 10-bit frequency parameter
-   **Noise generator:** Generate either white-noise or periodic noise at one
    of four shift rates: _N/512_, _N/1024_, _N/2048_, or the output of tone
    generator 3
-   **4-bit Level Control:** 4-bit level control over each channel with
    mixer sliders and CV inputs

See the [Manual][SN76489] for more information about the features of this module.

[SegaMasterSystem]: https://en.wikipedia.org/wiki/Master_System
[TI-SN76489]: https://en.wikipedia.org/wiki/Texas_Instruments_SN76489
[SN76489]: https://github.com/Kautenja/PotatoChips/releases/latest/download/SN76489.pdf

<!-- ------------------------------------------------------------ -->
<!-- MARK: TurboGrafx16 -->
<!-- ------------------------------------------------------------ -->

<!--
## TurboGrafx16 _(Coming Soon!)_

TurboGrafx16 is an emulation of the NEC TurboGrafx16 audio processing unit.

<p align="center">
<img alt="TurboGrafx16" src="manual/TurboGrafx16/img/TURBO_GRAFX_16-Module.svg">
</p>

### Features

See the [Manual][TurboGrafx16] for more information about the features of this module.

[TurboGrafx16]: https://github.com/Kautenja/PotatoChips/releases/latest/download/TurboGrafx16.pdf
-->

<!-- ------------------------------------------------------------ -->
<!-- MARK: VRC6 -->
<!-- ------------------------------------------------------------ -->

## VRC6

VRC6 is an emulation of the
[Konami VRC6](https://wiki.nesdev.com/w/index.php/VRC6_audio)
audio processing unit from the
[Nintendo Entertainment System (NES)](https://en.wikipedia.org/wiki/Nintendo_Entertainment_System)
for VCV Rack. The VRC6 chip contains two
pulse wave generators, and a quantized saw wave generator.

<p align="center">
<img alt="VRC6" src="manual/VRC6/img/VRC6-Module.svg">
</p>

### Features

-   **Dual pulse wave generator:** Dual 8-bit pulse waves with eight duty
    cycles: _6.25%_, _12.5%_, _18.75%_, _25%_, _31.25%_, _37.5%_, _43.75%_, and
    _50%_
-   **Quantized saw wave generator:** Generate NES style saw wave with variable
    quantization including the overflow bug in the VRC6
-   **Amplitude modulation:** Manual and CV control over the individual voice
    levels

See the [Manual][VRC6] for more information about the features of this module.

[VRC6]: https://github.com/Kautenja/PotatoChips/releases/latest/download/VRC6.pdf
