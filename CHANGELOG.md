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

## 1.7.2 (2020-09-23)

-   fix 2612 to use less CPU resources (disable emulators of polyphonic channels
    that are not in use)

## 1.7.3 (2020-09-24)

-   fix issue where GBS and 106 would drop onto the rack with corrupted waveforms

## 1.8.0 (2020-11-16)

-   header-only DSP library
-   fix issue where some modules would generate a "pop"/"click"/"impulse" on
    start up: 2A03, GBS, VRC6, FME7, POKEY, Ay-3-8910, 106, SN76489
-   fixes and updates to 2612 (rev2 branded as "Boss Fight")
    -   code optimizations
    -   updated panel layout
    -   saturation / aliasing control
    -   individual operator retrigger input for envelope generators
    -   individual operator triggers, gates, VOCT, and frequency knobs
    -   individual operator looping AD envelope generator
    -   individual operator sensitivity to LFO-based FM and AM
    -   VU Meter to monitor levels / clipping / aliasing
    -   invert sustain level and total level controls to be more intuitive
-   updates to SN76489 (rev2 branded as "Mega Tone")
    -   **audio rate FM**
    -   code optimizations
    -   new panel design
    -   attenuverter for frequency modulation that acts as fine frequency
        control when nothing is patched
    -   normalled inputs
    -   normalled outputs (i.e., mixer, with clipping, and aliasing)
    -   VU Meter to monitor levels / clipping / aliasing
-   updates to FME7 (rev2 branded as "Pulses")
    -   **audio rate FM**
    -   **changed default amplifier level to 10 instead of 7**
    -   new panel design
    -   attenuverter for frequency modulation that acts as fine frequency
        control when nothing is patched
    -   normalled inputs
    -   normalled outputs (i.e., mixer, with clipping, and aliasing)
    -   VU Meter to monitor levels / clipping / aliasing
-   updates to VRC6 (rev2 branded as "Step Saw")
    -   **audio rate FM**
    -   **sync input for saw wave generator**
    -   new panel design
    -   attenuverter for frequency modulation that acts as fine frequency
        control when nothing is patched
    -   normalled inputs
    -   normalled outputs (i.e., mixer, with clipping, and aliasing)
    -   VU Meter to monitor levels / clipping / aliasing
-   updates to 2A03 (rev2 branded as "Infinite Stairs")
    -   **hard sync input**
    -   **amplifier for steppy triangle generator**
    -   **sync input for steppy triangle generator and noise generator**
    -   **audio rate FM**
    -   new panel design
    -   attenuverter for frequency modulation that acts as fine frequency
        control when nothing is patched
    -   normalled inputs
    -   normalled outputs (i.e., mixer, with clipping, and aliasing)
    -   VU Meter to monitor levels / clipping / aliasing
-   updates to AY-3-8910 (rev2 branded as "Jairasullator")
    -   **internal envelope / LFO (full synthesizer voice)**
    -   **discrete noise period control**
    -   **audio rate FM**
    -   **offet and scale parameters in DAC mode**
    -   new panel design
    -   attenuverter for frequency modulation that acts as fine frequency
        control when nothing is patched
    -   normalled inputs
    -   normalled outputs (i.e., mixer, with clipping, and aliasing)
    -   VU Meter to monitor levels / clipping / aliasing
-   updates to POKEY (rev2 branded as "Pot Keys")
    -   **audio rate FM**
    -   new panel design
    -   attenuverter for frequency modulation that acts as fine frequency
        control when nothing is patched
    -   normalled inputs
    -   normalled outputs (i.e., mixer, with clipping, and aliasing)
    -   VU Meter to monitor levels / clipping / aliasing
-   updates to 106 (rev2 branded as "Name Corp Octal Wave Generator")
    -   **undo/redo support for wavetable edits**
    -   new panel design
    -   attenuverter for frequency modulation that acts as fine frequency
        control when nothing is patched
    -   normalled inputs
    -   normalled outputs (i.e., mixer, with clipping, and aliasing)
    -   VU Meter to monitor levels / clipping / aliasing
-   updates to GBS (rev2 branded as "Pallet Town Waves System")
    -   **undo/redo support for wavetable edits**
    -   **fixed wave channel to track VOCT correctly**
    -   new panel design
    -   attenuverter for frequency modulation that acts as fine frequency
        control when nothing is patched
    -   normalled inputs
    -   normalled outputs (i.e., mixer, with clipping, and aliasing)
    -   VU Meter to monitor levels / clipping / aliasing

## 1.8.1 (2020-11-16)

-   remove accidental debugging modules from release

## 1.8.2 (2020-11-17)

-   replace BossFight global data structures with Windows friendly implementation

## 1.8.3 (2020-11-17)

-   fix manual link for Boss Fight

## 1.9.0 (2020-11-20)

-   new module: Super Echo
    -   echo effect from the SNES (16-bit PCM)
        -   Gaussian filter removed
        -   BRR down-sampling removed
    -   2 channels of echo effect
        -   feedback and delay parameters with CV control
        -   stereo mix control with surround effect through phasing
    -   8-tap FIR filter with coefficient parameterization and CV control
    -   extended delay control up to _512ms_
-   new module: Super VCA
    -   Gaussian interpolation filter from the SNES
    -   low-pass IIR filter with parameterized coefficients (for operational modes)
    -   designed to act as a low-pass gate / VCA without a VCF
-   new module: Super ADSR
    -   Envelope generator from the SNES
    -   Attack, Decay, Sustain, and Release rate stages
-   new blank panels:
    -   illustration of Sony S-SMP IC

## 1.10.0 (2020-11-25)

-   new module: Mini Boss
    -   single operator version of an operator from **Boss Fight**
-   fix issues with Boss Fight
    -   fix issue where extreme negative voltage to the AR input would cause the
        envelope generator to be silent. This is an intentional design of the
        YM2612, but is better to omit for this module
    -   fix polyphonic pitch calculation for the operators

## 1.10.1 (2020-12-17)

-   fix sync inputs for Infinite Stairs, StepSaw, and Jairasullator. the inputs were triggering when the input crossed 2V, they now trigger when the signal crosses zero (from negative to positive)
-   fix trigger-able inputs to not fire when opening a patch
-   update parameter descriptions for Boss Fight and Mini Boss
    -   LFO now shows the Hz measurement of the LFO
    -   multiply now shows "1/2" instead of "0" when all the way CCW
    -   FMS now measures the sensitivity in _% of halftone_
    -   AMS now measures the sensitivity in _dB_
    -   change "prevent clicks from envelope generator" to "soft reset envelope generator"
    -   add _soft reset envelope generator_ to BossFight

## 1.10.2 (2021-01-12)

-   fix sync inputs for Infinite Stairs, StepSaw, and Jairasullator. the inputs were triggering when the input crossed 0V, they now trigger when the signal crosses the threshold of 0.01V to 0.02V (approximately 0V). This allows the sync to be triggered by a DC signal, like a gate or LFO.
-   fix trigger inputs on Pallet Town Waves System, Super ADSR, Boss Fight, Infinite Stairs, Jairasullator, Mega Tone, Mini Boss, and Pot Keys. They now work with AC and DC signals.
-   fix clock dividers for CV acquisition and LED light updates. They now fire on the downbeat of the source clock

## 1.11.0 (2021-07-27)

-   Fix wavetable editor screens to allow right clicks
-   Fix name of wave table editor Undo/Redo actions
-   Mirror monophonic inputs to modules that are running poly-phonically
-   new module: Blocks
    -   inspired by Mutable Instruments Edges

## 1.12.0 (TBD)

-   new module: S-SMP(BRR)
    -   Bit-Rate Reduction (BRR) based sampler/sample player
-   new module: NES(DMC)
    -   DMC sampler from the Ricoh 2A03 audio processing chip

## 1.13.0 (TBD)

-   Yamaha YM2151

## 1.14.0 (TBD)

-   Yamaha YM2413
