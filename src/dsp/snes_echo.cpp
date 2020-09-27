// Sony SPC700 emulator.
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

#include "snes_echo.hpp"
#include <algorithm>
#include <cstddef>
#include <limits>

/// Clamp an integer to a 16-bit value.
///
/// @param n a 32-bit integer value to clip
/// @returns n clipped to a 16-bit value [-32768, 32767]
///
inline int clamp_16(int n) {
    const int lower = std::numeric_limits<int16_t>::min();
    const int upper = std::numeric_limits<int16_t>::max();
    return std::max(lower, std::min(n, upper));
}

void Sony_S_DSP_Echo::run(int left, int right, int16_t* output_buffer) {
    // get the current feedback sample in the echo buffer
    auto const echo_sample = reinterpret_cast<BufferSample*>(&ram[buffer_head]);
    // increment the echo pointer by the size of the echo buffer sample (4)
    buffer_head += sizeof(BufferSample);
    // check if for the end of the ring buffer and wrap the pointer around
    // the echo delay is clamped in [0, 15] and each delay index requires
    // 2KB of RAM (0x800)
    if (buffer_head >= (delay & DELAY_LEVELS) * DELAY_LEVEL_BYTES)
        buffer_head = 0;
    // cache the feedback value (sign-extended to 32-bit)
    int fb_left = echo_sample->samples[BufferSample::LEFT];
    int fb_right = echo_sample->samples[BufferSample::RIGHT];

    // put samples in history ring buffer
    auto const fir_samples = &fir_buffer[fir_offset];
    // move backwards one step
    fir_offset = (fir_offset + FIR_MAX_INDEX) & FIR_MAX_INDEX;
    // put sample into the first sample in the buffer
    fir_samples[0].samples[BufferSample::LEFT]  = fb_left;
    fir_samples[0].samples[BufferSample::RIGHT] = fb_right;
    // duplicate at +8 eliminates wrap checking below
    fir_samples[8].samples[BufferSample::LEFT]  = fb_left;
    fir_samples[8].samples[BufferSample::RIGHT] = fb_right;

    // FIR left channel
    fb_left =                              fb_left * fir_coeff[7] +
        fir_samples[1].samples[BufferSample::LEFT] * fir_coeff[6] +
        fir_samples[2].samples[BufferSample::LEFT] * fir_coeff[5] +
        fir_samples[3].samples[BufferSample::LEFT] * fir_coeff[4] +
        fir_samples[4].samples[BufferSample::LEFT] * fir_coeff[3] +
        fir_samples[5].samples[BufferSample::LEFT] * fir_coeff[2] +
        fir_samples[6].samples[BufferSample::LEFT] * fir_coeff[1] +
        fir_samples[7].samples[BufferSample::LEFT] * fir_coeff[0];
    // FIR right channel
    fb_right =                             fb_right * fir_coeff[7] +
        fir_samples[1].samples[BufferSample::RIGHT] * fir_coeff[6] +
        fir_samples[2].samples[BufferSample::RIGHT] * fir_coeff[5] +
        fir_samples[3].samples[BufferSample::RIGHT] * fir_coeff[4] +
        fir_samples[4].samples[BufferSample::RIGHT] * fir_coeff[3] +
        fir_samples[5].samples[BufferSample::RIGHT] * fir_coeff[2] +
        fir_samples[6].samples[BufferSample::RIGHT] * fir_coeff[1] +
        fir_samples[7].samples[BufferSample::RIGHT] * fir_coeff[0];

    // put the echo samples into the buffer
    echo_sample->samples[BufferSample::LEFT] =
        clamp_16(left + ((fb_left  * feedback) >> 14));
    echo_sample->samples[BufferSample::RIGHT] =
        clamp_16(right + ((fb_right * feedback) >> 14));

    if (output_buffer) {  // write final samples
        // (1) add the echo to the samples for the left and right channel, (2)
        // clamp the left and right samples, and (3) place them into the buffer
        output_buffer[0] =
            clamp_16(left + ((fb_left  * mixLeft) >> 14));
        output_buffer[1] =
            clamp_16(right + ((fb_right * mixRight) >> 14));
    }
}
