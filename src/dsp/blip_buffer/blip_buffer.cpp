// Band-limited waveform buffer.
// Copyright 2020 Christian Kauten
// Copyright 2006 Shay Green
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

#include "blip_buffer.hpp"

int32_t BLIPBuffer::read_samples(blip_sample_t* out, int32_t max_samples, bool stereo) {
    // sample rate must have been set
    assert(buffer_);
    int32_t count = samples_count();
    if (count > max_samples) count = max_samples;
    if (!count) return 0;

    auto buf = buffer_;
    int32_t accum = reader_accum;

    if (!stereo) {
        for (int32_t n = count; n--;) {
            int32_t s = accum >> accum_fract;
            accum -= accum >> bass_shift;
            accum += (static_cast<int32_t>(*buf++) - sample_offset) << accum_fract;
            *out++ = static_cast<blip_sample_t>(s);
            // clamp sample
            if (static_cast<int16_t>(s) != s)
                out[-1] = blip_sample_t(0x7FFF - (s >> 24));
        }
    } else {
        for (int32_t n = count; n--;) {
            int32_t s = accum >> accum_fract;
            accum -= accum >> bass_shift;
            accum += (static_cast<int32_t>(*buf++) - sample_offset) << accum_fract;
            *out = static_cast<blip_sample_t>(s);
            out += 2;
            // clamp sample
            if (static_cast<int16_t>(s) != s)
                out[-2] = blip_sample_t(0x7FFF - (s >> 24));
        }
    }
    reader_accum = accum;
    remove_samples(count);
    return count;
}
