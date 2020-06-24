// Band-limited waveform buffer.
// Copyright 2020 Christian Kauten
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

void BLIPImpulse::scale_impulse(int unit, imp_t* imp_in) const {
    int32_t offset = ((int32_t) unit << IMPULSE_BITS) - IMPULSE_OFFSET * unit +
            (1 << (IMPULSE_BITS - 1));
    imp_t* imp = imp_in;
    imp_t* fimp = impulse;
    for (int n = res / 2 + 1; n--;) {
        int error = unit;
        for (int nn = width; nn--;) {
            int32_t a = ((int32_t) *fimp++ * unit + offset) >> IMPULSE_BITS;
            error -= a - unit;
            *imp++ = (imp_t) a;
        }

        // add error to middle
        imp[-width / 2 - 1] += (imp_t) error;
    }

    if (res > 2) {
        // second half is mirror-image
        const imp_t* rev = imp - width - 1;
        for (int nn = (res / 2 - 1) * width - 1; nn--;)
            *imp++ = *--rev;
        *imp++ = (imp_t) unit;
    }

    // copy to odd offset
    *imp++ = (imp_t) unit;
    memcpy(imp, imp_in, (res * width - 1) * sizeof *imp);
}

void BLIPImpulse::treble_eq(const blip_eq_t& new_eq) {
    static constexpr double pi = 3.1415926535897932384626433832795029L;
    if (!generate && new_eq.treble == eq.treble && new_eq.cutoff == eq.cutoff &&
            new_eq.sample_rate == eq.sample_rate)
        return; // already calculated with same parameters

    generate = false;
    eq = new_eq;

    double treble = pow(10.0, 1.0 / 20 * eq.treble);  // dB (-6dB = 0.50)
    if (treble < 0.000005)
        treble = 0.000005;

    const double treble_freq = 22050.0;  // treble level at 22 kHz harmonic
    const double sample_rate = eq.sample_rate;
    const double pt = treble_freq * 2 / sample_rate;
    double cutoff = eq.cutoff * 2 / sample_rate;
    if (cutoff >= pt * 0.95 || cutoff >= 0.95) {
        cutoff = 0.5;
        treble = 1.0;
    }

    // DSF Synthesis (See T. Stilson & J. Smith (1996),
    // Alias-free digital synthesis of classic analog waveforms)

    // reduce adjacent impulse interference by using small part of wide impulse
    const double n_harm = 4096;
    const double rolloff = pow(treble, 1.0 / (n_harm * pt - n_harm * cutoff));
    const double rescale = 1.0 / pow(rolloff, n_harm * cutoff);

    const double pow_a_n = rescale * pow(rolloff, n_harm);
    const double pow_a_nc = rescale * pow(rolloff, n_harm * cutoff);

    double total = 0.0;
    const double to_angle = pi / 2 / n_harm / BLIP_MAX_RES;

    float buf[BLIP_MAX_RES * (BLIPBuffer::WIDEST_IMPULSE - 2) / 2];
    const int size = BLIP_MAX_RES * (width - 2) / 2;
    for (int i = size; i--;) {
        double angle = (i * 2 + 1) * to_angle;

        const double cos_angle = cos(angle);
        const double cos_nc_angle = cos(n_harm * cutoff * angle);
        const double cos_nc1_angle = cos((n_harm * cutoff - 1.0) * angle);

        double b = 2.0 - 2.0 * cos_angle;
        double a = 1.0 - cos_angle - cos_nc_angle + cos_nc1_angle;

        double d = 1.0 + rolloff * (rolloff - 2.0 * cos_angle);
        double c = pow_a_n * rolloff * cos((n_harm - 1.0) * angle) -
                pow_a_n * cos(n_harm * angle) -
                pow_a_nc * rolloff * cos_nc1_angle +
                pow_a_nc * cos_nc_angle;

        // optimization of a / b + c / d
        double y = (a * d + c * b) / (b * d);

        // fixed window which affects wider impulses more
        if (width > 12) {
            double window = cos(n_harm / 1.25 / BLIPBuffer::WIDEST_IMPULSE * angle);
            y *= window * window;
        }

        total += (float) y;
        buf[i] = (float) y;
    }

    // integrate runs of length 'BLIP_MAX_RES'
    double factor = IMPULSE_AMP * 0.5 / total;  // 0.5 accounts for other mirrored half
    imp_t* imp = impulse;
    const int step = BLIP_MAX_RES / res;
    int offset = res > 1 ? BLIP_MAX_RES : BLIP_MAX_RES / 2;
    for (int n = res / 2 + 1; n--; offset -= step) {
        for (int w = -width / 2; w < width / 2; w++) {
            double sum = 0;
            for (int i = BLIP_MAX_RES; i--;) {
                int index = w * BLIP_MAX_RES + offset + i;
                if (index < 0)
                    index = -index - 1;
                if (index < size)
                    sum += buf[index];
            }
            *imp++ = (imp_t) floor(sum * factor + (IMPULSE_OFFSET + 0.5));
        }
    }

    // rescale
    double unit = volume_unit_;
    if (unit >= 0) {
        volume_unit_ = -1;
        volume_unit(unit);
    }
}

void BLIPBuffer::remove_samples(int32_t count) {
    // sample rate must have been set
    assert(buffer_);
    // optimization
    if (!count) return;
    remove_silence(count);
    // Allows synthesis slightly past time passed to end_frame(), as int32_t as it's
    // not more than an output sample.
    // to do: kind of hacky, could add run_until() which keeps track of extra synthesis
    int const copy_extra = 1;
    // copy remaining samples to beginning and clear old samples
    int32_t remain = samples_count() + WIDEST_IMPULSE + copy_extra;
    if (count >= remain)
        memmove(buffer_, buffer_ + count, remain * sizeof (buf_t_));
    else
        memcpy( buffer_, buffer_ + count, remain * sizeof (buf_t_));
    memset(buffer_ + remain, sample_offset & 0xFF, count * sizeof (buf_t_));
}

int32_t BLIPBuffer::read_samples(blip_sample_t* out, int32_t max_samples, bool stereo) {
    // sample rate must have been set
    assert(buffer_);
    int32_t count = samples_count();
    if (count > max_samples) count = max_samples;
    // optimization
    if (!count) return 0;

    int sample_offset = this->sample_offset;
    int bass_shift = this->bass_shift;
    buf_t_* buf = buffer_;
    int32_t accum = reader_accum;

    if (!stereo) {
        for (int32_t n = count; n--;) {
            int32_t s = accum >> accum_fract;
            accum -= accum >> bass_shift;
            accum += (int32_t (*buf++) - sample_offset) << accum_fract;
            *out++ = (blip_sample_t) s;

            // clamp sample
            if ((int16_t) s != s)
                out[-1] = blip_sample_t (0x7FFF - (s >> 24));
        }
    }
    else {
        for (int32_t n = count; n--;) {
            int32_t s = accum >> accum_fract;
            accum -= accum >> bass_shift;
            accum += (int32_t (*buf++) - sample_offset) << accum_fract;
            *out = (blip_sample_t) s;
            out += 2;

            // clamp sample
            if ((int16_t) s != s)
                out[-2] = blip_sample_t (0x7FFF - (s >> 24));
        }
    }
    reader_accum = accum;
    remove_samples(count);
    return count;
}

void BLIPBuffer::mix_samples(const blip_sample_t* in, int32_t count) {
    buf_t_* buf = &buffer_[(offset_ >> BLIP_BUFFER_ACCURACY) + (WIDEST_IMPULSE / 2 - 1)];

    int prev = 0;
    while (count--) {
        int s = *in++;
        *buf += s - prev;
        prev = s;
        ++buf;
    }
    *buf -= *--in;
}
