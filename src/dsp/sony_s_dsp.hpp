// Sony S-DSP emulator.
// Copyright 2020 Christian Kauten
// Copyright 2006 Shay Green
// Copyright 2002 Brad Martin
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
// derived from: Game_Music_Emu 0.5.2
// Based on Brad Martin's OpenSPC DSP emulator
//

#ifndef DSP_SONY_S_DSP_HPP_
#define DSP_SONY_S_DSP_HPP_

#include "exceptions.hpp"
#include "sony_s_dsp_common.hpp"
#include <cstring>

/// @brief Sony S-DSP chip emulator.
class Sony_S_DSP {
 public:
    enum : unsigned {
        /// the number of sampler voices on the chip
        VOICE_COUNT = 8,
        /// the number of FIR coefficients used by the chip's echo filter
        FIR_COEFFICIENT_COUNT = 8,
        /// the number of registers on the chip
        NUM_REGISTERS = 128,
        /// the size of the RAM bank in bytes
        SIZE_OF_RAM = 1 << 16
    };

    /// @brief the global registers on the S-DSP.
    enum GlobalRegister : uint8_t {
        /// The volume for the left channel of the main output
        MAIN_VOLUME_LEFT =         0x0C,
        /// Echo Feedback
        ECHO_FEEDBACK =            0x0D,
        /// The volume for the right channel of the main output
        MAIN_VOLUME_RIGHT =        0x1C,
        /// The volume for the left channel of the echo effect
        ECHO_VOLUME_LEFT =         0x2C,
        /// pitch modulation
        PITCH_MODULATION =         0x2D,
        /// The volume for the right channel of the echo effect
        ECHO_VOLUME_RIGHT =        0x3C,
        /// Noise enable
        NOISE_ENABLE =             0x3D,
        /// Key-on (1 bit for each voice)
        KEY_ON =                   0x4C,
        /// Echo enable
        ECHO_ENABLE =              0x4D,
        /// Key-off (1 bit for each voice)
        KEY_OFF =                  0x5C,
        /// Offset of source directory
        /// (`OFFSET_SOURCE_DIRECTORY * 0x100` = memory offset)
        OFFSET_SOURCE_DIRECTORY =  0x5D,
        /// DSP flags for RESET, MUTE, ECHO, NOISE PERIOD
        FLAGS =                    0x6C,
        /// Echo buffer start offset
        /// (`ECHO_BUFFER_START_OFFSET * 0x100` = memory offset)
        ECHO_BUFFER_START_OFFSET = 0x6D,
        /// ENDX - 1 bit for each voice.
        ENDX =                     0x7C,
        /// Echo delay, 4-bits, higher values require more memory.
        ECHO_DELAY =               0x7D
    };

    /// @brief the channel registers on the S-DSP. To get the register for
    /// channel `n`, perform the logical OR of the register address with `0xn0`.
    enum ChannelRegister : uint8_t {
        /// Left channel volume (8-bit signed value).
        VOLUME_LEFT      = 0x00,
        /// Right channel volume (8-bit signed value).
        VOLUME_RIGHT     = 0x01,
        /// Lower 8 bits of pitch.
        PITCH_LOW        = 0x02,
        /// Higher 8-bits of pitch.
        PITCH_HIGH       = 0x03,
        /// Source number (\f$\in [0, 255]\f$). (references the source directory)
        SOURCE_NUMBER    = 0x04,
        /// If bit-7 is set, ADSR is enabled. If cleared GAIN is used.
        ADSR_1           = 0x05,
        /// These two registers control the ADSR envelope.
        ADSR_2           = 0x06,
        /// This register provides function for software envelopes.
        GAIN             = 0x07,
        /// The DSP writes the current value of the envelope to this register.
        ENVELOPE_OUT     = 0x08,
        /// The DSP writes the current waveform value after envelope
        /// multiplication and before volume multiplication.
        WAVEFORM_OUT     = 0x09,
        /// 8-tap FIR Filter coefficients
        FIR_COEFFICIENTS = 0x0F
    };

    /// @brief Bit-masks for extracting values from the flags registers.
    enum FlagMasks : uint8_t {
        /// a mask for the flag register to extract the noise period parameter
        FLAG_MASK_NOISE_PERIOD = 0x1F,
        /// a mask for the flag register to extract the echo write enabled bit
        FLAG_MASK_ECHO_WRITE = 0x20,
        /// a mask for the flag register to extract the mute voices bit
        FLAG_MASK_MUTE = 0x40,
        /// a mask for the flag register to extract the reset chip bit
        FLAG_MASK_RESET = 0x80
    };

    /// A structure mapping the register space to a single voice's data fields.
    struct RawVoice {
        /// the volume of the left channel
        int8_t left_vol;
        /// the volume of the right channel
        int8_t right_vol;
        /// the rate of the oscillator
        uint8_t rate[2];
        /// the oscillator's waveform sample
        uint8_t waveform;
        /// envelope rates for attack, decay, and sustain
        uint8_t adsr[2];
        /// envelope gain (if not using ADSR)
        uint8_t gain;
        /// current envelope level
        int8_t envx;
        /// current sample
        int8_t outx;
        /// filler bytes to align to 16-bytes
        int8_t unused[6];
    };

    /// A structure mapping the register space to symbolic global data fields.
    struct GlobalData {
        /// padding
        int8_t unused1[12];
        /// 0C Main Volume Left (8-bit signed value)
        int8_t left_volume;
        /// 0D   Echo Feedback (8-bit signed value)
        int8_t echo_feedback;
        /// padding
        int8_t unused2[14];
        /// 1C   Main Volume Right (8-bit signed value)
        int8_t right_volume;
        /// padding
        int8_t unused3[15];
        /// 2C   Echo Volume Left (8-bit signed value)
        int8_t left_echo_volume;
        /// 2D   Pitch Modulation on/off for each voice (bit-mask)
        uint8_t pitch_mods;
        /// padding
        int8_t unused4[14];
        /// 3C   Echo Volume Right (8-bit signed value)
        int8_t right_echo_volume;
        /// 3D   Noise output on/off for each voice (bit-mask)
        uint8_t noise_enables;
        /// padding
        int8_t unused5[14];
        /// 4C   Key On for each voice (bit-mask)
        uint8_t key_ons;
        /// 4D   Echo on/off for each voice (bit-mask)
        uint8_t echo_ons;
        /// padding
        int8_t unused6[14];
        /// 5C   key off for each voice (instantiates release mode) (bit-mask)
        uint8_t key_offs;
        /// 5D   source directory (wave table offsets)
        uint8_t wave_page;
        /// padding
        int8_t unused7[14];
        /// 6C   flags and noise freq (coded 8-bit value)
        uint8_t flags;
        /// 6D   the page of RAM to use for the echo buffer
        uint8_t echo_page;
        /// padding
        int8_t unused8[14];
        /// 7C   whether the sample has ended for each voice (bit-mask)
        uint8_t wave_ended;
        /// 7D   ms >> 4
        uint8_t echo_delay;
        /// padding
        char unused9[2];
    };

 private:
    /// Combine the raw voice, registers, and global data structures into a
    /// single piece of memory to allow easy symbolic access to register data
    union {
        /// the register bank on the chip
        uint8_t registers[NUM_REGISTERS];
        /// the mapping of register data to the voices on the chip
        RawVoice voices[VOICE_COUNT];
        /// the mapping of register data to the global data on the chip
        GlobalData global;
    };

    /// The values of the FIR filter coefficients from the register bank. This
    /// allows the FIR coefficients to be stored as 16-bit
    short fir_coeff[FIR_COEFFICIENT_COUNT];

    /// @brief A pointer to the shared 64KB RAM bank between the S-DSP and
    /// the SPC700.
    /// @details
    /// this must be maintained by the caller in order to provide data to the
    /// S-DSP. This includes input sample data, and the allocated space for the
    /// echo buffer according to the global ECHO_BUFFER_START_OFFSET register
    uint8_t* const ram;

    /// A bit-mask representation of the active voice gates
    int keys;

    /// The number of samples until the LFSR will sample a new value
    int noise_count;
    /// The discrete sampled LFSR register noise value
    int noise;
    /// The amplified LFSR register noise sample
    int noise_amp;

    /// A pointer to the head of the echo buffer in RAM
    int echo_ptr;

    /// fir_buf[i + 8] == fir_buf[i], to avoid wrap checking in FIR code
    short fir_buf[16][2];
    /// (0 to 7)
    int fir_offset;

    /// The stages of the ADSR envelope generator.
    enum class EnvelopeStage : short { Attack, Decay, Sustain, Release };

    /// The state of a synthesizer voice (channel) on the chip.
    struct VoiceState {
        /// the volume level of the voice
        short volume[2];
        /// 12-bit fractional position
        short fraction;
        /// padding (placement here keeps interp's in a 64-bit line)
        short unused0;
        /// most recent four decoded samples for the Gaussian filter
        int16_t interp[4];
        /// number of nibbles remaining in current block
        short block_remain;
        /// the current address of the sample being played by the voice
        unsigned short addr;
        /// header byte from current block
        short block_header;
        /// padding (placement here keeps envelope data in a 64-bit line)
        short unused1;
        /// the envelope generator sample counter
        short envcnt;
        /// the output value from the envelope generator
        short envx;
        /// the number of samples delay until the voice turns on (after key-on)
        short on_cnt;
        /// the current stage of the envelope generator
        EnvelopeStage envelope_stage;
    } voice_states[VOICE_COUNT];

    /// @brief Process the envelope for the voice with given index.
    ///
    /// @param voice_index the index of the voice to clock the envelope of
    /// returns the envelope counter value for given index in the table
    ///
    inline int clock_envelope(unsigned voice_idx) {
        // cache the voice data structures
        RawVoice& raw_voice = this->voices[voice_idx];
        VoiceState& voice = voice_states[voice_idx];
        // cache the current envelope value
        int envx = voice.envx;

        // process the release stage
        if (voice.envelope_stage == EnvelopeStage::Release) {
            // Docs: "When in the state of "key off". the "click" sound is
            // prevented by the addition of the fixed value 1/256" WTF???
            // Alright, I'm going to choose to interpret that this way:
            // When a note is keyed off, start the RELEASE state, which
            // subtracts 1/256th each sample period (32kHz).  Note there's
            // no need for a count because it always happens every update.
            envx -= ENVELOPE_RANGE / 256;
            if (envx <= 0) {
                voice.envx = 0;
                keys &= ~(1 << voice_idx);
                return -1;
            }
            voice.envx = envx;
            raw_voice.envx = envx >> 8;
            return envx;
        }

        int cnt = voice.envcnt;
        int adsr1 = raw_voice.adsr[0];
        if (adsr1 & 0x80) {
            switch (voice.envelope_stage) {
            case EnvelopeStage::Attack: {
                // increase envelope by 1/64 each step
                int t = adsr1 & 15;
                if (t == 15) {
                    envx += ENVELOPE_RANGE / 2;
                } else {
                    cnt -= get_envelope_rate(t * 2 + 1);
                    if (cnt > 0) break;
                    envx += ENVELOPE_RANGE / 64;
                    cnt = ENVELOPE_RATE_INITIAL;
                }
                if (envx >= ENVELOPE_RANGE) {
                    envx = ENVELOPE_RANGE - 1;
                    voice.envelope_stage = EnvelopeStage::Decay;
                }
                voice.envx = envx;
                break;
            }
            case EnvelopeStage::Decay: {
                // Docs: "DR...[is multiplied] by the fixed value
                // 1-1/256." Well, at least that makes some sense.
                // Multiplying ENVX by 255/256 every time DECAY is
                // updated.
                cnt -= get_envelope_rate(((adsr1 >> 3) & 0xE) + 0x10);
                if (cnt <= 0) {
                    cnt = ENVELOPE_RATE_INITIAL;
                    envx -= ((envx - 1) >> 8) + 1;
                    voice.envx = envx;
                }
                int sustain_level = raw_voice.adsr[1] >> 5;

                if (envx <= (sustain_level + 1) * 0x100)
                    voice.envelope_stage = EnvelopeStage::Sustain;
                break;
            }
            case EnvelopeStage::Sustain:
                // Docs: "SR[is multiplied] by the fixed value 1-1/256."
                // Multiplying ENVX by 255/256 every time SUSTAIN is
                // updated.
                cnt -= get_envelope_rate(raw_voice.adsr[1] & 0x1F);
                if (cnt <= 0) {
                    cnt = ENVELOPE_RATE_INITIAL;
                    envx -= ((envx - 1) >> 8) + 1;
                    voice.envx = envx;
                }
                break;
            case EnvelopeStage::Release:
                // handled above
                break;
            }
        } else {  // GAIN mode is set
            // TODO: if the game switches between ADSR and GAIN modes
            // partway through, should the count be reset, or should it
            // continue from where it was? Does the DSP actually watch for
            // that bit to change, or does it just go along with whatever
            // it sees when it performs the update? I'm going to assume
            // the latter and not update the count, unless I see a game
            // that obviously wants the other behavior.  The effect would
            // be pretty subtle, in any case.
            int t = raw_voice.gain;
            if (t < 0x80)  // direct GAIN mode
                envx = voice.envx = t << 4;
            else switch (t >> 5) {
            case 4:  // Decrease (linear)
                // Subtraction of the fixed value 1 / 64.
                cnt -= get_envelope_rate(t & 0x1F);
                if (cnt > 0)
                    break;
                cnt = ENVELOPE_RATE_INITIAL;
                envx -= ENVELOPE_RANGE / 64;
                if (envx < 0) {
                    envx = 0;
                    if (voice.envelope_stage == EnvelopeStage::Attack)
                        voice.envelope_stage = EnvelopeStage::Decay;
                }
                voice.envx = envx;
                break;
            case 5:  // Decrease (exponential)
                // Multiplication by the fixed value 1 - 1/256.
                cnt -= get_envelope_rate(t & 0x1F);
                if (cnt > 0)
                    break;
                cnt = ENVELOPE_RATE_INITIAL;
                envx -= ((envx - 1) >> 8) + 1;
                if (envx < 0) {
                    envx = 0;
                    if (voice.envelope_stage == EnvelopeStage::Attack)
                        voice.envelope_stage = EnvelopeStage::Decay;
                }
                voice.envx = envx;
                break;
            case 6:  // Increase (linear)
                // Addition of the fixed value 1/64.
                cnt -= get_envelope_rate(t & 0x1F);
                if (cnt > 0)
                    break;
                cnt = ENVELOPE_RATE_INITIAL;
                envx += ENVELOPE_RANGE / 64;
                if (envx >= ENVELOPE_RANGE)
                    envx = ENVELOPE_RANGE - 1;
                voice.envx = envx;
                break;
            case 7:  // Increase (bent line)
                // Addition of the constant 1/64 up to .75 of the constant 1/256
                // from .75 to 1.
                cnt -= get_envelope_rate(t & 0x1F);
                if (cnt > 0)
                    break;
                cnt = ENVELOPE_RATE_INITIAL;
                if (envx < ENVELOPE_RANGE * 3 / 4)
                    envx += ENVELOPE_RANGE / 64;
                else
                    envx += ENVELOPE_RANGE / 256;
                if (envx >= ENVELOPE_RANGE)
                    envx = ENVELOPE_RANGE - 1;
                voice.envx = envx;
                break;
            }
        }
        // update the envelope counter and envelope output for the voice
        voice.envcnt = cnt;
        raw_voice.envx = envx >> 4;

        return envx;
    }

 public:
    /// @brief Initialize a new Sony_S_DSP.
    ///
    /// @param ram_ a pointer to the 64KB shared RAM
    ///
    explicit Sony_S_DSP(uint8_t* ram_) : ram(ram_) { }

    /// @brief Clear state and silence everything.
    void reset() {
        keys = echo_ptr = noise_count = fir_offset = 0;
        noise = 1;
        // reset, mute, echo off
        global.flags = FLAG_MASK_RESET | FLAG_MASK_MUTE | FLAG_MASK_ECHO_WRITE;
        global.key_ons = 0;
        // reset voices
        for (unsigned i = 0; i < VOICE_COUNT; i++) {
            VoiceState& v = voice_states[i];
            v.on_cnt = v.volume[0] = v.volume[1] = 0;
            v.envelope_stage = EnvelopeStage::Release;
        }
        // clear the echo buffer
        memset(fir_buf, 0, sizeof fir_buf);
    }

    /// @brief Read data from the register at the given address.
    ///
    /// @param address the address of the register to read data from
    ///
    inline uint8_t read(uint8_t address) {
        if (address >= NUM_REGISTERS)  // make sure the given address is valid
            throw AddressSpaceException<uint8_t>(address, 0, NUM_REGISTERS);
        return registers[address];
    }

    /// @brief Write data to the registers at the given address.
    ///
    /// @param address the address of the register to write data to
    /// @param data the data to write to the register
    ///
    void write(uint8_t address, uint8_t data) {
        if (address >= NUM_REGISTERS)  // make sure the given address is valid
            throw AddressSpaceException<uint8_t>(address, 0, NUM_REGISTERS);
        // store the data in the register with given address
        registers[address] = data;
        // get the high 4 bits for indexing the voice / FIR coefficients
        int index = address >> 4;
        // update volume / FIR coefficients
        switch (address & FIR_COEFFICIENTS) {
            // voice volume
            case 0:    // left channel, fall through to next block
            case 1: {  // right channel, process both left and right channels
                short* volume = voice_states[index].volume;
                int left  = (int8_t) registers[address & ~1];
                int right = (int8_t) registers[address |  1];
                volume[0] = left;
                volume[1] = right;
                break;
            }
            case FIR_COEFFICIENTS:  // update FIR coefficients
                // sign-extend
                fir_coeff[index] = (int8_t) data;
                break;
        }
    }

    /// @brief Run DSP for some samples and write them to the given buffer.
    ///
    /// @param output_buffer the output buffer to write samples to (optional)
    ///
    /// @details
    /// the sample rate of the system is locked to 32kHz just like the SNES
    ///
    void run(int16_t* output_buffer = nullptr) {
        // TODO: Should we just fill the buffer with silence? Flags won't be
        // cleared during this run so it seems it should keep resetting every
        // sample.
        if (global.flags & FLAG_MASK_RESET) reset();
        // use the global wave page address to lookup a pointer to the first entry
        // in the source directory. the wave page is multiplied by 0x100 to produce
        // the RAM address of the source directory.
        const SourceDirectoryEntry* const source_directory =
            reinterpret_cast<SourceDirectoryEntry*>(&ram[global.wave_page * 0x100]);
        // get the volume of the left channel from the global registers
        int left_volume  = global.left_volume;
        int right_volume = global.right_volume;
        // -------------------------------------------------------------------
        // MARK: Key Off / Key On
        // -------------------------------------------------------------------
        // Here we check for keys on / off. Docs say that successive writes
        // to KON / KOF must be separated by at least 2 T_s periods or risk
        // being neglected.  Therefore, DSP only looks at these during an
        // update, and not at the time of the write. Only need to do this
        // once however, since the regs haven't changed over the whole
        // period we need to catch up with.
        // -------------------------------------------------------------------
        // Keying on a voice resets that bit in ENDX.
        global.wave_ended &= ~global.key_ons;
        // -------------------------------------------------------------------
        // MARK: Noise
        // -------------------------------------------------------------------
        // the `noise_enables` register is a length 8 bit-mask for enabling /
        // disabling noise for each individual voice.
        if (global.noise_enables) {  // noise enabled for at least one voice
            // update the noise period based on the index of the rate in the
            // global flags register
            noise_count -= get_envelope_rate(global.flags & FLAG_MASK_NOISE_PERIOD);
            if (noise_count <= 0) {  // rising edge of noise generator
                // reset the noise period to the initial value
                noise_count = ENVELOPE_RATE_INITIAL;
                // the LFSR is 15-bit, shift left 1 to get the 16-bit sample
                noise_amp = static_cast<int16_t>(noise << 1);
                // update the linear feedback shift register from taps 0, 1.
                noise = (((noise << 13) ^ (noise << 14)) & 0x4000) | (noise >> 1);
                // the Galois equivalent was implemented as below, but yielded
                // poor CPU performance relative to the Fibonacci method above
                // and produced a frequency response that seemed incorrect,
                // i.e., high frequency noise had a much higher low frequency
                // response. As such, the Fibonacci implementation above is
                // the preferred route for this LFSR implementation.
                //     uint16_t noise = this->noise;
                //     noise = (noise >> 1) ^ (0x6000 & -(noise & 1));
                //     this->noise = noise;
            }
        }
        // -------------------------------------------------------------------
        // MARK: Voice Processing
        // -------------------------------------------------------------------
        // store output of the the last monophonic voice for phase modulation.
        // the output for phase modulation on voice 0 is always 0. The switch can
        // be set, but has no effect. Games like Jurassic Park set the flag, but
        // it is not known why.
        int prev_outx = 0;
        // buffer the outputs of the left and right echo and main channels
        int echol = 0;
        int echor = 0;
        int left = 0;
        int right = 0;
        // iterate over the voices on the chip
        for (unsigned voice_idx = 0; voice_idx < VOICE_COUNT; voice_idx++) {
            // get the voice's bit-mask shift value
            const int voice_bit = 1 << voice_idx;
            // cache the voice and data structures
            RawVoice& raw_voice = voices[voice_idx];
            VoiceState& voice = voice_states[voice_idx];
            // ---------------------------------------------------------------
            // MARK: Gate Processing
            // ---------------------------------------------------------------
            if (voice.on_cnt && !--voice.on_cnt) {
                // key on
                keys |= voice_bit;
                voice.addr = source_directory[raw_voice.waveform].start;
                voice.block_remain = 1;
                voice.envx = 0;
                voice.block_header = 0;
                // decode three samples immediately
                voice.fraction = 0x3FFF;
                // BRR decoder filter uses previous two samples
                voice.interp[0] = 0;
                voice.interp[1] = 0;
                // NOTE: Real SNES does *not* appear to initialize the
                // envelope counter to anything in particular. The first
                // cycle always seems to come at a random time sooner than
                // expected; as yet, I have been unable to find any
                // pattern.  I doubt it will matter though, so we'll go
                // ahead and do the full time for now.
                voice.envcnt = ENVELOPE_RATE_INITIAL;
                voice.envelope_stage = EnvelopeStage::Attack;
            }
            // key-on = !key-off = true
            if (global.key_ons & voice_bit & ~global.key_offs) {
                global.key_ons &= ~voice_bit;
                voice.on_cnt = 8;
            }
            // key-off = true
            if (keys & global.key_offs & voice_bit) {
                voice.envelope_stage = EnvelopeStage::Release;
                voice.on_cnt = 0;
            }

            int envx;
            if (!(keys & voice_bit) || (envx = clock_envelope(voice_idx)) < 0) {
                raw_voice.envx = 0;
                raw_voice.outx = 0;
                prev_outx = 0;
                continue;
            }

            // Decode samples when fraction >= 1.0 (0x1000)
            for (int n = voice.fraction >> 12; --n >= 0;) {
                if (!--voice.block_remain) {
                    if (voice.block_header & 1) {
                        global.wave_ended |= voice_bit;
                        if (voice.block_header & 2) {
                            // verified (played endless looping sample and ENDX was set)
                            voice.addr = source_directory[raw_voice.waveform].loop;
                        } else {  // first block was end block; don't play anything
                            goto sample_ended;
                        }
                    }
                    voice.block_header = ram[voice.addr++];
                    voice.block_remain = 16;  // nibbles
                }

                if (
                    voice.block_remain == 9 &&
                    (ram[voice.addr + 5] & 3) == 1 &&
                    (voice.block_header & 3) != 3
                ) {  // next block has end flag set, this block ends early
            sample_ended:
                    global.wave_ended |= voice_bit;
                    keys &= ~voice_bit;
                    raw_voice.envx = 0;
                    voice.envx = 0;
                    // add silence samples to interpolation buffer
                    do {
                        voice.interp[3] = voice.interp[2];
                        voice.interp[2] = voice.interp[1];
                        voice.interp[1] = voice.interp[0];
                        voice.interp[0] = 0;
                    } while (--n >= 0);
                    break;
                }

                int delta = ram[voice.addr];
                if (voice.block_remain & 1) {
                    delta <<= 4;  // use lower nibble
                    voice.addr++;
                }

                // Use sign-extended upper nibble
                delta = int8_t(delta) >> 4;

                // For invalid ranges (D,E,F): if the nibble is negative,
                // the result is F000.  If positive, 0000. Nothing else
                // like previous range, etc seems to have any effect.  If
                // range is valid, do the shift normally.  Note these are
                // both shifted right once to do the filters properly, but
                // the output will be shifted back again at the end.
                int shift = voice.block_header >> 4;
                delta = (delta << shift) >> 1;
                if (shift > 0x0C) delta = (delta >> 14) & ~0x7FF;

                // One, two and three point IIR filters
                int smp1 = voice.interp[0];
                int smp2 = voice.interp[1];
                if (voice.block_header & 8) {
                    delta += smp1;
                    delta -= smp2 >> 1;
                    if (!(voice.block_header & 4)) {
                        delta += (-smp1 - (smp1 >> 1)) >> 5;
                        delta += smp2 >> 5;
                    } else {
                        delta += (-smp1 * 13) >> 7;
                        delta += (smp2 + (smp2 >> 1)) >> 4;
                    }
                } else if (voice.block_header & 4) {
                    delta += smp1 >> 1;
                    delta += (-smp1) >> 5;
                }

                voice.interp[3] = voice.interp[2];
                voice.interp[2] = smp2;
                voice.interp[1] = smp1;
                voice.interp[0] = 2 * clamp_16(delta);
            }

            // get the 14-bit frequency value
            int rate = 0x3FFF & ((raw_voice.rate[1] << 8) | raw_voice.rate[0]);
            if (global.pitch_mods & voice_bit)
                rate = (rate * (prev_outx + 32768)) >> 15;

            // Gaussian interpolation using most recent 4 samples
            int index = voice.fraction >> 2 & 0x3FC;
            voice.fraction = (voice.fraction & 0x0FFF) + rate;
            auto table1 = get_gaussian(index);
            auto table2 = get_gaussian(255 * 4 - index);
            int sample = ((table1[0] * voice.interp[3]) >> 12) +
                         ((table1[1] * voice.interp[2]) >> 12) +
                         ((table2[1] * voice.interp[1]) >> 12);
            sample = static_cast<int16_t>(2 * sample);
            sample +=     (table2[0] * voice.interp[0]) >> 11 & ~1;

            // if noise is enabled for this voice use the amplified noise as
            // the output, otherwise use the clamped sampled value
            int output = (global.noise_enables & voice_bit) ?
                noise_amp : clamp_16(sample);
            // scale output and set outx values
            output = (output * envx) >> 11 & ~1;
            int l = (voice.volume[0] * output) >> 7;
            int r = (voice.volume[1] * output) >> 7;

            prev_outx = output;
            raw_voice.outx = output >> 8;
            if (global.echo_ons & voice_bit) {
                echol += l;
                echor += r;
            }
            left  += l;
            right += r;
        }
        // end of channel loop

        // main volume control
        left  = (left  * left_volume) >> 7;
        right = (right * right_volume) >> 7;

        // -------------------------------------------------------------------
        // MARK: Echo FIR filter
        // -------------------------------------------------------------------

        // get the current feedback sample in the echo buffer
        StereoSample* const echo_sample =
            reinterpret_cast<StereoSample*>(&ram[(global.echo_page * 0x100 + echo_ptr) & 0xFFFF]);
        // increment the echo pointer by the size of the echo buffer sample (4)
        echo_ptr += sizeof(StereoSample);
        // check if for the end of the ring buffer and wrap the pointer around
        // the echo delay is clamped in [0, 15] and each delay index requires
        // 2KB of RAM (0x800)
        if (echo_ptr >= (global.echo_delay & 15) * 0x800) echo_ptr = 0;
        // cache the feedback value (sign-extended to 32-bit)
        int fb_left = echo_sample->samples[StereoSample::LEFT];
        int fb_right = echo_sample->samples[StereoSample::RIGHT];

        // put samples in history ring buffer
        const int fir_offset = this->fir_offset;
        short (*fir_pos)[2] = &fir_buf[fir_offset];
        this->fir_offset = (fir_offset + 7) & 7;  // move backwards one step
        fir_pos[0][0] = (short) fb_left;
        fir_pos[0][1] = (short) fb_right;
        // duplicate at +8 eliminates wrap checking below
        fir_pos[8][0] = (short) fb_left;
        fir_pos[8][1] = (short) fb_right;

        // FIR
        fb_left =     fb_left * fir_coeff[7] +
                fir_pos[1][0] * fir_coeff[6] +
                fir_pos[2][0] * fir_coeff[5] +
                fir_pos[3][0] * fir_coeff[4] +
                fir_pos[4][0] * fir_coeff[3] +
                fir_pos[5][0] * fir_coeff[2] +
                fir_pos[6][0] * fir_coeff[1] +
                fir_pos[7][0] * fir_coeff[0];

        fb_right =   fb_right * fir_coeff[7] +
                fir_pos[1][1] * fir_coeff[6] +
                fir_pos[2][1] * fir_coeff[5] +
                fir_pos[3][1] * fir_coeff[4] +
                fir_pos[4][1] * fir_coeff[3] +
                fir_pos[5][1] * fir_coeff[2] +
                fir_pos[6][1] * fir_coeff[1] +
                fir_pos[7][1] * fir_coeff[0];
        // add the echo to the samples for the left and right channel
        left  += (fb_left  * global.left_echo_volume) >> 14;
        right += (fb_right * global.right_echo_volume) >> 14;

        if (!(global.flags & FLAG_MASK_ECHO_WRITE)) {  // echo buffer feedback
            // add feedback to the echo samples
            echol += (fb_left  * global.echo_feedback) >> 14;
            echor += (fb_right * global.echo_feedback) >> 14;
            // put the echo samples into the buffer
            echo_sample->samples[StereoSample::LEFT] = clamp_16(echol);
            echo_sample->samples[StereoSample::RIGHT] = clamp_16(echor);
        }

        // -------------------------------------------------------------------
        // MARK: Output
        // -------------------------------------------------------------------
        if (output_buffer) {  // write final samples
            // clamp the left and right samples and place them into the buffer
            output_buffer[0] = left  = clamp_16(left);
            output_buffer[1] = right = clamp_16(right);
            // increment the buffer to the position of the next stereo sample
            if (global.flags & FLAG_MASK_MUTE)  // muting
                output_buffer[0] = output_buffer[1] = 0;
        }
    }
};

#endif  // DSP_SONY_S_DSP_HPP_
