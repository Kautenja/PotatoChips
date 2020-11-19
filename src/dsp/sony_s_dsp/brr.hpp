// An emulation of the BRR sample playback engine from the Sony S-DSP.
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

#ifndef DSP_SONY_S_DSP_BRR_HPP_
#define DSP_SONY_S_DSP_BRR_HPP_

#include "common.hpp"

/// @brief An emulation of the BRR sample playback engine from the Sony S-DSP.
class __attribute__((packed, aligned(32))) Sony_S_DSP_BRR {
 public:
    enum : unsigned {
        /// the size of the RAM bank in bytes
        SIZE_OF_RAM = 1 << 16
    };

 private:
    // -----------------------------------------------------------------------
    // MARK: Word 1,2
    // -----------------------------------------------------------------------
    /// A pointer to the shared 64KB RAM bank between the S-DSP and the SPC700.
    /// This must be maintained by the caller in order to provide sample data
    uint8_t* ram = nullptr;
    // -----------------------------------------------------------------------
    // MARK: Word 3
    // -----------------------------------------------------------------------
    /// source directory (wave table offsets)
    uint8_t wave_page = 0;
    /// the index of the starting sample of the waveform
    uint8_t wave_index = 0;
    /// the current address of the sample being played by the voice
    uint16_t addr = 0;
    // -----------------------------------------------------------------------
    // MARK: Word 4
    // -----------------------------------------------------------------------
    /// the output value from the envelope generator
    int16_t envelope_value = 0;
    /// the current stage of the envelope generator
    enum EnvelopeStage {
        Off = 0,
        On,
        Release
    };
    uint8_t envelope_stage: 3;
    /// number of nibbles remaining in current block
    uint8_t block_remain: 5;
    /// header byte from current block
    BitRateReductionBlock::Header block_header;
    // -----------------------------------------------------------------------
    // MARK: Word 5
    // -----------------------------------------------------------------------
    /// the 14-bit frequency value
    uint16_t rate = 0;
    /// 12-bit fractional position
    uint16_t fraction = 0;
    // -----------------------------------------------------------------------
    // MARK: Word 6,7
    // -----------------------------------------------------------------------
    /// the previous four samples for Gaussian interpolation
    int16_t samples[4] = {0, 0, 0, 0};
    // -----------------------------------------------------------------------
    // MARK: Word 8
    // -----------------------------------------------------------------------
    /// the monophonic output from the voice
    int16_t output = 0;
    /// the volume for the left channel output
    int8_t volumeLeft = 0;
    /// the volume for the right channel output
    int8_t volumeRight = 0;

    /// @brief Process the envelope for the voice with given index.
    ///
    /// @returns the envelope counter value for given index in the table
    ///
    inline int clock_envelope() {
        // the initial value of the envelope
        static constexpr uint16_t ENVELOPE_INITIAL = 0x0800;
        // process the release stage
        if (envelope_stage == EnvelopeStage::Release) {
            // Docs: "When in the state of "key off". the "click" sound is
            // prevented by the addition of the fixed value 1/256" WTF???
            // Alright, I'm going to choose to interpret that this way:
            // When a note is keyed off, start the RELEASE state, which
            // subtracts 1/256th each sample period (32kHz).  Note there's
            // no need for a count because it always happens every update.
            envelope_value -= ENVELOPE_INITIAL / 256;
            if (envelope_value <= 0) {
                envelope_stage = EnvelopeStage::Off;
                envelope_value = 0;
                output = 0;
                return -1;
            }
            return envelope_value;
        }
        // process the on stage
        envelope_value = ENVELOPE_INITIAL;
        return envelope_value;
    }

 public:
    /// @brief Initialize a new Sony_S_DSP_BRR.
    Sony_S_DSP_BRR() : envelope_stage(EnvelopeStage::Off), block_remain(0) { }

    /// @brief Set the RAM pointer to a new value.
    ///
    /// @param ram_ a pointer to the new 8-bit RAM to use
    ///
    inline void set_ram(uint8_t* ram_) { ram = ram_; }

    /// @brief Set the page of samples in RAM to read samples from.
    ///
    /// @param address the address in RAM to start the wave page from
    ///
    /// @details
    /// Source Directory Offset.
    ///
    /// DIR
    ///          7     6     5     4     3     2     1     0
    ///       +-----+-----+-----+-----+-----+-----+-----+-----+
    /// $5D   |                  Offset value                 |
    ///       +-----+-----+-----+-----+-----+-----+-----+-----+
    /// This register points to the source(sample) directory in external
    /// RAM. The pointer is calculated by Offset*0x100. This is because each
    /// directory is 4-bytes (0x100).
    ///
    /// The source directory contains sample start and loop point offsets.
    /// Its a simple array of 16-bit values.
    ///
    /// SAMPLE DIRECTORY
    ///
    /// OFFSET  SIZE    DESC
    /// dir+0   16-BIT  SAMPLE-0 START
    /// dir+2   16-BIT  SAMPLE-0 LOOP START
    /// dir+4   16-BIT  SAMPLE-1 START
    /// dir+6   16-BIT  SAMPLE-1 LOOP START
    /// dir+8   16-BIT  SAMPLE-2 START
    /// ...
    /// This can continue for up to 256 samples. (SRCN can only reference
    /// 256 samples)
    ///
    inline void setWavePage(uint8_t address) { wave_page = address; }

    /// @brief Set the index of the sample in the source directory to play.
    ///
    /// @param index the offset of the sample from the wave page
    ///
    /// @details
    /// Source number is a reference to the "Source Directory" (see DIR).
    /// The DSP will use the sample with this index from the directory.
    /// I'm not sure what happens when you change the SRCN when the
    /// channel is active, but it probably doesn't have any effect
    /// until KON is set.
    ///          7     6     5     4     3     2     1     0
    ///       +-----+-----+-----+-----+-----+-----+-----+-----+
    /// $x4   |                 Source Number                 |
    ///       +-----+-----+-----+-----+-----+-----+-----+-----+
    ///
    inline void setWaveIndex(uint8_t index) { wave_index = index; }

    /// @brief Set the frequency of the low-pass gate to a new value.
    ///
    /// @param freq the frequency to set the low-pass gate to
    ///
    inline void setFrequency(float freq) { rate = get_pitch(freq); }

    /// @brief Set the volume to new level for the left channel.
    ///
    /// @param value the level to set the left channel to
    ///
    inline void setVolumeLeft(int8_t value) { volumeLeft = value; }

    /// @brief Set the volume to new level for the right channel.
    ///
    /// @param value the level to set the right channel to
    ///
    inline void setVolumeRight(int8_t value) { volumeRight = value; }

    inline int16_t getOutput() { return output; }

    /// @brief Run DSP for some samples and write them to the given buffer.
    ///
    /// @param out the output buffer to write the samples to
    /// @param trigger a boolean signal for triggering the sample player
    /// @param gate_on a boolean signal for enabling the sample playback
    /// @param phase_modulation the phase modulation to apply to the voice
    ///
    /// @details
    /// the sample rate of the system is locked to 32kHz just like the SNES
    ///
    void run(StereoSample& out, bool trigger, bool gate_on, int phase_modulation = 0) {
        // use the global wave page address to lookup a pointer to the first
        // entry in the source directory. the wave page is multiplied by 0x100
        // to produce the RAM address of the source directory.
        const SourceDirectoryEntry* const source_directory =
            reinterpret_cast<SourceDirectoryEntry*>(&ram[wave_page * 0x100]);
        // ---------------------------------------------------------------
        // MARK: Gate / Envelope generator
        // ---------------------------------------------------------------
        if (trigger) {  // trigger the voice
            addr = source_directory[wave_index].start;
            block_remain = 1;
            block_header.byte = 0;
            // decode three samples immediately
            fraction = 0x3FFF;
            envelope_stage = EnvelopeStage::On;
        }
        if (!gate_on) {  // enter the release stage
            envelope_stage = EnvelopeStage::Release;
        }
        // return if the envelope generator is in the off stage
        if (envelope_stage == EnvelopeStage::Off) return;
        // process the gate using the envelope generator to prevent pops
        auto envelope = clock_envelope();
        // envelope is negative if entering the off stage
        if (envelope < 0) return;
        // ---------------------------------------------------------------
        // MARK: BRR Sample Decoder
        // Decode samples when fraction >= 1.0 (0x1000)
        // ---------------------------------------------------------------
        for (int n = fraction >> 12; --n >= 0;) {
            if (!--block_remain) {
                if (block_header.flags.is_end) {
                    if (block_header.flags.is_loop) {
                        addr = source_directory[wave_index].loop;
                    } else {  // first block was end block; don't play anything
                        envelope_stage = EnvelopeStage::Off;
                        envelope_value = 0;
                        output = 0;
                        samples[0] = samples[1] = samples[2] = samples[3] = 0;
                        break;
                    }
                }
                block_header.byte = ram[addr++];
                block_remain = 16;  // nibbles
            }

            if (
                block_remain == 9 &&
                (ram[addr + 5] & 3) == 1 &&
                !(block_header.flags.is_end && block_header.flags.is_loop)
            ) {  // next block has end flag set, this block ends early
                envelope_stage = EnvelopeStage::Off;
                envelope_value = 0;
                output = 0;
                samples[0] = samples[1] = samples[2] = samples[3] = 0;
                break;
            }
            // get the next sample from RAM
            int delta = ram[addr];
            if (block_remain & 1) {  // use lower nibble
                delta <<= 4;
                addr++;
            }
            // Use sign-extended upper nibble
            delta = int8_t(delta) >> 4;
            // For invalid ranges (D,E,F): if the nibble is negative,
            // the result is F000.  If positive, 0000. Nothing else
            // like previous range, etc seems to have any effect.  If
            // range is valid, do the shift normally.  Note these are
            // both shifted right once to do the filters properly, but
            // the output will be shifted back again at the end.
            int shift = block_header.flags.volume;
            delta = (delta << shift) >> 1;
            if (shift > 0x0C) delta = (delta >> 14) & ~0x7FF;
            // -----------------------------------------------------------
            // MARK: BRR Reconstruction Filter (1,2,3 point IIR)
            // -----------------------------------------------------------
            switch (block_header.flags.filter) {
            case 0:  // !filter1 !filter2
                break;
            case 1:  // !filter1 filter2
                delta += samples[0] >> 1;
                delta += (-samples[0]) >> 5;
                break;
            case 2:  // filter1 !filter2
                delta += samples[0];
                delta -= samples[1] >> 1;
                delta += (-samples[0] - (samples[0] >> 1)) >> 5;
                delta += samples[1] >> 5;
                break;
            case 3:  // filter1 filter2
                delta += samples[0];
                delta -= samples[1] >> 1;
                delta += (-samples[0] * 13) >> 7;
                delta += (samples[1] + (samples[1] >> 1)) >> 4;
                break;
            }
            // cycle sample history and set delta to latest sample
            samples[3] = samples[2];
            samples[2] = samples[1];
            samples[1] = samples[0];
            samples[0] = 2 * clamp_16(delta);
        }
        // ---------------------------------------------------------------
        // MARK: Gaussian Interpolation Filter
        // ---------------------------------------------------------------
        // get the 14-bit frequency value
        int phase = 0x3FFF & rate;
        // apply phase modulation
        phase = (phase * (phase_modulation + 32768)) >> 15;
        // Gaussian interpolation using most recent 4 samples
        const uint16_t index = fraction >> 2 & 0x3FC;
        fraction = (fraction & 0x0FFF) + phase;
        const auto table1 = get_gaussian(index);
        const auto table2 = get_gaussian(255 * 4 - index);
        int sample = ((table1[0] * samples[3]) >> 12) +
                     ((table1[1] * samples[2]) >> 12) +
                     ((table2[1] * samples[1]) >> 12);
        sample = static_cast<int16_t>(2 * sample);
        sample +=     (table2[0] * samples[0]) >> 11 & ~1;
        // scale output from this voice
        output = clamp_16(sample);
        output = (output * envelope) >> 11 & ~1;
        // -------------------------------------------------------------------
        // MARK: Output
        // -------------------------------------------------------------------
        out.samples[StereoSample::LEFT] = clamp_16((volumeLeft * output) >> 7);
        out.samples[StereoSample::RIGHT] = clamp_16((volumeRight * output) >> 7);
    }
};

#endif  // DSP_SONY_S_DSP_BRR_HPP_
