// Band-limited sound synthesis buffer (BLIPBuffer 0.4.1).
// Copyright 2020 Christian Kauten
// Copyright 2003-2006 Shay Green
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

#ifdef BLARGG_ENABLE_OPTIMIZER
    #include BLARGG_ENABLE_OPTIMIZER
#endif

// BLIPSynth_

#if !BLIP_BUFFER_FAST

BLIPSynth_::BLIPSynth_(blip_sample_t* p, int w) : impulses(p), width(w) {
    volume_unit_ = 0.0;
    kernel_unit = 0;
    buf = 0;
    last_amp = 0;
    delta_factor = 0;
}

#include "sinc.hpp"

void blip_eq_t::generate(float* out, int count) const {
    // lower cutoff freq for narrow kernels with their wider transition band
    // (8 points->1.49, 16 points->1.15)
    double oversample = blip_res * 2.25 / count + 0.85;
    double half_rate = sample_rate * 0.5;
    if (cutoff_freq)
        oversample = half_rate / cutoff_freq;
    double cutoff = rolloff_freq * oversample / half_rate;
    // generate a sinc
    gen_sinc(out, count, blip_res * oversample, treble, cutoff);
    // apply (half of) hamming window
    double to_fraction = BLARGG_PI / (count - 1);
    for (int i = count; i--;)
        out[i] *= 0.54f - 0.46f * static_cast<float>(cos(i * to_fraction));
}

void BLIPSynth_::adjust_impulse() {
    // sum pairs for each phase and add error correction to end of first half
    int const size = impulses_size();
    for (int p = blip_res; p-- >= blip_res / 2;) {
        int p2 = blip_res - 2 - p;
        long error = kernel_unit;
        for (int i = 1; i < size; i += blip_res) {
            error -= impulses[i + p ];
            error -= impulses[i + p2];
        }
        if (p == p2)
            error /= 2; // phase = 0.5 impulse uses same half for both sides
        impulses[size - blip_res + p] += (blip_sample_t) error;
    }
}

void BLIPSynth_::treble_eq(blip_eq_t const& eq) {
    float fimpulse[blip_res / 2 * (blip_widest_impulse_ - 1) + blip_res * 2];

    int const half_size = blip_res / 2 * (width - 1);
    eq.generate(&fimpulse[blip_res], half_size);

    int i;

    // need mirror slightly past center for calculation
    for (i = blip_res; i--;)
        fimpulse[blip_res + half_size + i] = fimpulse[blip_res + half_size - 1 - i];

    // starts at 0
    for (i = 0; i < blip_res; i++)
        fimpulse[i] = 0.0f;

    // find rescale factor
    double total = 0.0;
    for (i = 0; i < half_size; i++)
        total += fimpulse[blip_res + i];

    // double const base_unit = 44800.0 - 128 * 18; // allows treble up to +0 dB
    // double const base_unit = 37888.0; // allows treble to +5 dB
    double const base_unit = 32768.0; // necessary for blip_unscaled to work
    double rescale = base_unit / 2 / total;
    kernel_unit = (long) base_unit;

    // integrate, first difference, rescale, convert to int
    double sum = 0.0;
    double next = 0.0;
    int const impulses_size = this->impulses_size();
    for (i = 0; i < impulses_size; i++) {
        impulses[i] = (blip_sample_t) floor((next - sum) * rescale + 0.5);
        sum += fimpulse[i];
        next += fimpulse[i + blip_res];
    }
    adjust_impulse();

    // volume might require rescaling
    double vol = volume_unit_;
    if (vol) {
        volume_unit_ = 0.0;
        volume_unit(vol);
    }
}

void BLIPSynth_::volume_unit(double new_unit) {
    if (new_unit != volume_unit_) {
        // use default eq if it hasn't been set yet
        if (!kernel_unit)
            treble_eq(-8.0);

        volume_unit_ = new_unit;
        double factor = new_unit * (1L << blip_sample_bits) / kernel_unit;

        if (factor > 0.0) {
            int shift = 0;

            // if unit is really small, might need to attenuate kernel
            while (factor < 2.0) {
                shift++;
                factor *= 2.0;
            }

            if (shift) {
                kernel_unit >>= shift;
                assert(kernel_unit > 0); // fails if volume unit is too low
                // keep values positive to avoid round-towards-zero of sign-preserving
                // right shift for negative values
                long offset = 0x8000 + (1 << (shift - 1));
                long offset2 = 0x8000 >> shift;
                for (int i = impulses_size(); i--;)
                    impulses[i] = (blip_sample_t) (((impulses[i] + offset) >> shift) - offset2);
                adjust_impulse();
            }
        }
        delta_factor = (int) floor(factor + 0.5);
    }
}

#endif  // !BLIP_BUFFER_FAST
