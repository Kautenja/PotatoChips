# Change Log

## 1.0.0 (2020-06-22)

-   2A03 implementation

## 1.1.0 (2020-06-23)

-   VRC6 implementation

## 1.1.1 (2020-06-23)

-   minor improvements and optimizations

## 1.1.2 (2020-06-30)

-   minor improvements and optimizations

## 1.2.0 (2020-07-09)

-   FME7 implementation

## 1.2.1 (2020-07-09)

-   fix issues with FME7 panel

## 1.3.0 (2020-07-14)

-   106 Module
    -   new panel design
    -   wave-table editor
    -   waveform morph with 5 tables
    -   all 8 channels active

## 1.3.1 (2020-07-14)

-   performance improvements
    -   CV acquisition at _1/16x_ sample rate
    -   LED update at _1/128x_ sample rate

## 1.3.2 (2020-07-14)

-   fix issue where oscillator increased in pitch when increasing sampling rate
-   fix issue where 106 was not tuned to C4

## 1.3.3 (2020-07-15)

-   minor optimizations to 106

## 1.3.4 (2020-07-17)

-   update Blip_Buffer to 0.4.1 and refactor
-   fix "click" from sample rates: 44.1kHz, 88.2kHz, etc.
-   fix issue where 2A03, VRC6, FME7 were detuned at sample rates:
    44.1kHz, 88.2kHz, etc.

## 1.3.5 (2020-07-17)

-   refactor BLIPBuffer to be more C++17-ish and CPU friendly

## 1.4.0 (2020-07-18)

-   Texas Instruments SN76489 from Sega Master System

## 1.4.1 (2020-07-25)

-   fix issue where CV modulated SN76489 VCA wouldn't return to 0
-   fix issue where SN76489 at high-frequency would produce no output
-   fix issue where amplifier for FME7 wasn't allowed full gain
-   4-bit volume control with CV & 2-bit PW control CV for 2A03
-   updated panel for 2A03, FME7, VRC6
-   fix issue where SN76489 trigger input wasn't responding to bipolar inputs
-   updated default wave-tables for 106, sine, ramps, pulse, triangle

## 1.4.2 (2020-07-31)

-   fix build break for MacOS platform

## 1.5.0 (2020-08-04)

-   strong FM for all modules by a factor of 2
-   new modules!
    -   AY-3-8910
    -   POKEY
    -   GBS

## 1.5.1 (2020-08-04)

-   improve the CV input for noise period on:
    -   2A03
    -   GBS

## 1.5.2 (2020-08-08)

-   fix CV volume control for:
    -   VRC6
    -   FME7
    -   AY-3-8910
    -   106

## 1.6.0 (2020-08-10)

-   polyphonic support for:
    -   2A03
    -   VRC6
    -   GBS
    -   AY-3-8910
    -   POKEY
    -   FME7
    -   SN76489
    -   106

## 1.6.1 (2020-08-10)

-   fix to check all input ports for polyphonic cables

## 1.6.2 (2020-08-10)

-   fix frequency range of 106 to resolve hang & crash

## 1.6.3 (2020-09-14)

-   fix ordering of pairs of samples (nibbles) for wavetable editor on 106
-   update triangle waveforms for wave-table editors to be subjectively better
-   fix compilation issue for ARM builds!
-   change internal representation of Frequency parameters to be more efficient
-   fix emulator initialization to address and issue where oscillators would
    be initialized perfectly out of phase (and thus mix together to produce
    silence)

## 1.7.0 (2020-09-17)

-   new module: 2616
    -    6 voices of FM synthesis
    -    CV control of all parameters
    -    polyphony support

## 1.7.1 (2020-09-18)

-   fix 2612 to sound the same at all sample rates
-   fix 2612 to be tuned to C4
-   fix 2612 LFO to produce the correct frequencies
-   fix 2612 Envelope generator to have correct timings
-   fix 2612 TL parameter to reduce the dead range of the parameter
-   remove invalid tags from plugin.json
