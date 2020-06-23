# 2A03

[![Travis CI Build Status][BuildStatus]][BuildServer]

[BuildStatus]:  https://travis-ci.com/Kautenja/2A03.svg?branch=master
[BuildServer]:  https://travis-ci.com/Kautenja/2A03

2A03 is an emulation of the 2A03 sound chip from the Nintendo Entertainment
System (NES) for VCV Rack. The 2A03 chip contains two square wave generators,
a quantized triangle wave generator, and a noise generator. The original chip
featured a DMC loader for playing samples that has been omitted in this
emulation.

<p align="center">
<img alt="2A03" src="img/2A03.png" height="380px">
</p>

## Features

-   **Dual square wave generator:** Dual 8-bit square waves with four duty
    cycles: _12.5%_, _25%_, _50%_, and _75%_
-   **Quantized triangle wave generator:** Generate NES style triangle wave
    with 16 steps of quantization
-   **Noise generator:** generate pseudo-random numbers at 16 different
    frequencies
-   **Linear Feedback Shift Register (LFSR):** old-school 8-bit randomness!

See the [Manual](https://kautenja.github.io/modules/2A03/manual.pdf) for more
information about the features of this module.

## Acknowledgments

The code for the module is derived from the NES synthesis library,
[Nes_Snd_Emu](https://github.com/jamesathey/Nes_Snd_Emu).
