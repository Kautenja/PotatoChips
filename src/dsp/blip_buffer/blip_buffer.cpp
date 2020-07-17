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

BLIPBuffer::SampleRateStatus BLIPBuffer::set_sample_rate(
    uint32_t samples_per_sec,
    uint32_t buffer_length
) {
    if (buffer_size_ == silent_buf_size) {
        assert(0);
        return SampleRateStatus::SilentBuffer;
    }

    // start with maximum length that re-sampled time can represent
    uint32_t new_size = (std::numeric_limits<uint32_t>::max() >> BLIP_BUFFER_ACCURACY) - blip_buffer_extra_ - 64;
    if (buffer_length != blip_max_length) {
        uint32_t s = (samples_per_sec * (buffer_length + 1) + 999) / 1000;
        if (s < new_size)
            new_size = s;
        else  // fails if requested buffer length exceeds limit
            assert(0);
    }

    if (buffer_size_ != new_size) {
        void* p = realloc(buffer_, (new_size + blip_buffer_extra_) * sizeof *buffer_);
        if (!p)
            return SampleRateStatus::OutOfMemory;
        buffer_ = (buf_t_*) p;
    }

    buffer_size_ = new_size;
    assert(buffer_size_ != silent_buf_size);

    // update things based on the sample rate
    sample_rate_ = samples_per_sec;
    length_ = new_size * 1000 / samples_per_sec - 1;
    if (buffer_length)  // ensure length is same as that passed in
        assert(length_ == buffer_length);
    if (clock_rate_)  // reset clock rate if one is set
        set_clock_rate(clock_rate_);
    bass_freq(bass_freq_);

    clear();

    return SampleRateStatus::Success;
}

long BLIPBuffer::read_samples(blip_sample_t* BLIP_RESTRICT out, long max_samples, bool stereo) {
    long count = samples_count();
    if (count > max_samples)
        count = max_samples;

    if (count) {
        int const bass = BLIP_READER_BASS(*this);
        BLIP_READER_BEGIN(reader, *this);

        if (!stereo) {
            for (blip_long n = count; n; --n)
            {
                blip_long s = BLIP_READER_READ(reader);
                if ((blip_sample_t) s != s)
                    s = 0x7FFF - (s >> 24);
                *out++ = (blip_sample_t) s;
                BLIP_READER_NEXT(reader, bass);
            }
        }
        else
        {
            for (blip_long n = count; n; --n)
            {
                blip_long s = BLIP_READER_READ(reader);
                if ((blip_sample_t) s != s)
                    s = 0x7FFF - (s >> 24);
                *out = (blip_sample_t) s;
                out += 2;
                BLIP_READER_NEXT(reader, bass);
            }
        }
        BLIP_READER_END(reader, *this);

        remove_samples(count);
    }
    return count;
}

void BLIPBuffer::mix_samples(blip_sample_t const* in, long count) {
    if (buffer_size_ == silent_buf_size) {
        assert(0);
        return;
    }

    buf_t_* out = buffer_ + (offset_ >> BLIP_BUFFER_ACCURACY) + blip_widest_impulse_ / 2;

    int const sample_shift = blip_sample_bits - 16;
    int prev = 0;
    while (count--) {
        blip_long s = (blip_long) *in++ << sample_shift;
        *out += s - prev;
        prev = s;
        ++out;
    }
    *out -= prev;
}

// BLIPSynth_

BLIPSynth_Fast_::BLIPSynth_Fast_() {
    buf = 0;
    last_amp = 0;
    delta_factor = 0;
}

void BLIPSynth_Fast_::volume_unit(double new_unit) {
    delta_factor = int (new_unit * (1L << blip_sample_bits) + 0.5);
}

#if !BLIP_BUFFER_FAST

BLIPSynth_::BLIPSynth_(int16_t* p, int w) :
    impulses(p),
    width(w) {
    volume_unit_ = 0.0;
    kernel_unit = 0;
    buf = 0;
    last_amp = 0;
    delta_factor = 0;
}

#undef PI
#define PI 3.1415926535897932384626433832795029

static void gen_sinc(float* out, int count, double oversample, double treble, double cutoff) {
    if (cutoff >= 0.999)
        cutoff = 0.999;

    if (treble < -300.0)
        treble = -300.0;
    if (treble > 5.0)
        treble = 5.0;

    double const maxh = 4096.0;
    double const rolloff = pow(10.0, 1.0 / (maxh * 20.0) * treble / (1.0 - cutoff));
    double const pow_a_n = pow(rolloff, maxh - maxh * cutoff);
    double const to_angle = PI / 2 / maxh / oversample;
    for (int i = 0; i < count; i++) {
        double angle = ((i - count) * 2 + 1) * to_angle;
        double c = rolloff * cos((maxh - 1.0) * angle) - cos(maxh * angle);
        double cos_nc_angle = cos(maxh * cutoff * angle);
        double cos_nc1_angle = cos((maxh * cutoff - 1.0) * angle);
        double cos_angle = cos(angle);

        c = c * pow_a_n - rolloff * cos_nc1_angle + cos_nc_angle;
        double d = 1.0 + rolloff * (rolloff - cos_angle - cos_angle);
        double b = 2.0 - cos_angle - cos_angle;
        double a = 1.0 - cos_angle - cos_nc_angle + cos_nc1_angle;

        out [i] = (float) ((a * d + c * b) / (b * d)); // a / b + c / d
    }
}

void blip_eq_t::generate(float* out, int count) const {
    // lower cutoff freq for narrow kernels with their wider transition band
    // (8 points->1.49, 16 points->1.15)
    double oversample = blip_res * 2.25 / count + 0.85;
    double half_rate = sample_rate * 0.5;
    if (cutoff_freq)
        oversample = half_rate / cutoff_freq;
    double cutoff = rolloff_freq * oversample / half_rate;

    gen_sinc(out, count, blip_res * oversample, treble, cutoff);

    // apply (half of) hamming window
    double to_fraction = PI / (count - 1);
    for (int i = count; i--;)
        out [i] *= 0.54f - 0.46f * (float) cos(i * to_fraction);
}

void BLIPSynth_::adjust_impulse() {
    // sum pairs for each phase and add error correction to end of first half
    int const size = impulses_size();
    for (int p = blip_res; p-- >= blip_res / 2;) {
        int p2 = blip_res - 2 - p;
        long error = kernel_unit;
        for (int i = 1; i < size; i += blip_res) {
            error -= impulses [i + p ];
            error -= impulses [i + p2];
        }
        if (p == p2)
            error /= 2; // phase = 0.5 impulse uses same half for both sides
        impulses [size - blip_res + p] += (int16_t) error;
        //printf("error: %ld\n", error);
    }

    //for (int i = blip_res; i--; printf("\n"))
    //  for (int j = 0; j < width / 2; j++)
    //      printf("%5ld,", impulses [j * blip_res + i + 1]);
}

void BLIPSynth_::treble_eq(blip_eq_t const& eq) {
    float fimpulse [blip_res / 2 * (blip_widest_impulse_ - 1) + blip_res * 2];

    int const half_size = blip_res / 2 * (width - 1);
    eq.generate(&fimpulse [blip_res], half_size);

    int i;

    // need mirror slightly past center for calculation
    for (i = blip_res; i--;)
        fimpulse [blip_res + half_size + i] = fimpulse [blip_res + half_size - 1 - i];

    // starts at 0
    for (i = 0; i < blip_res; i++)
        fimpulse [i] = 0.0f;

    // find rescale factor
    double total = 0.0;
    for (i = 0; i < half_size; i++)
        total += fimpulse [blip_res + i];

    //double const base_unit = 44800.0 - 128 * 18; // allows treble up to +0 dB
    //double const base_unit = 37888.0; // allows treble to +5 dB
    double const base_unit = 32768.0; // necessary for blip_unscaled to work
    double rescale = base_unit / 2 / total;
    kernel_unit = (long) base_unit;

    // integrate, first difference, rescale, convert to int
    double sum = 0.0;
    double next = 0.0;
    int const impulses_size = this->impulses_size();
    for (i = 0; i < impulses_size; i++) {
        impulses [i] = (int16_t) floor((next - sum) * rescale + 0.5);
        sum += fimpulse [i];
        next += fimpulse [i + blip_res];
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
            while (factor < 2.0)
            {
                shift++;
                factor *= 2.0;
            }

            if (shift)
            {
                kernel_unit >>= shift;
                assert(kernel_unit > 0); // fails if volume unit is too low

                // keep values positive to avoid round-towards-zero of sign-preserving
                // right shift for negative values
                long offset = 0x8000 + (1 << (shift - 1));
                long offset2 = 0x8000 >> shift;
                for (int i = impulses_size(); i--;)
                    impulses [i] = (int16_t) (((impulses [i] + offset) >> shift) - offset2);
                adjust_impulse();
            }
        }
        delta_factor = (int) floor(factor + 0.5);
        //printf("delta_factor: %d, kernel_unit: %d\n", delta_factor, kernel_unit);
    }
}

#endif  // !BLIP_BUFFER_FAST
