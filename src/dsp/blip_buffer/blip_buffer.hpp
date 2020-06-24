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

#ifndef BLIP_BUFFER_BLIP_BUFFER_HPP
#define BLIP_BUFFER_BLIP_BUFFER_HPP

#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>

/// Source time unit.
typedef int32_t blip_time_t;

/// Type for sample produced. Signed 16-bit format.
typedef int16_t blip_sample_t;

class BLIPBuffer {
 public:
    static constexpr auto BLIP_BUFFER_ACCURACY = 16;

    /// Initialize an empty BLIPBuffer.
    BLIPBuffer() {
        samples_per_sec = 44100;
        buffer_ = NULL;
        // try to cause assertion failure if buffer is used before these are set
        clocks_per_sec = 0;
        factor_ = ~0ul;
        offset_ = 0;
        buffer_size_ = 0;
        length_ = 0;
        bass_freq_ = 16;
    }

    /// Destroy an instance of BLIPBuffer.
    ~BLIPBuffer() { delete[] buffer_; }

    /// Set output sample rate and buffer length in milliseconds (1/1000 sec),
    /// then clear buffer. If there is insufficient memory for the buffer,
    /// sets the buffer length to 0 and returns error string or propagates
    /// exception if compiler supports it.
    const char* set_sample_rate(int32_t new_rate) {
        unsigned new_size = (UINT_MAX >> BLIP_BUFFER_ACCURACY) + 1 - WIDEST_IMPULSE - 64;

        if (buffer_size_ != new_size) {
            delete[] buffer_;
            buffer_ = NULL;  // allow for exception in allocation below
            buffer_size_ = 0;
            offset_ = 0;
            buffer_ = new buf_t_[new_size + WIDEST_IMPULSE];
        }

        buffer_size_ = new_size;
        length_ = new_size * 1000 / new_rate - 1;

        samples_per_sec = new_rate;
        // recalculate factor
        if (clocks_per_sec) set_clock_rate(clocks_per_sec);
        // recalculate shift
        bass_freq(bass_freq_);

        clear();

        return 0;
    }

    /// Return current output sample rate.
    inline int32_t get_sample_rate() const { return samples_per_sec; };

    /// Set number of source time units per second.
    inline void set_clock_rate(int32_t cps) {
        clocks_per_sec = cps;
        factor_ = (uint32_t) floor((double) samples_per_sec / cps * (1L << BLIP_BUFFER_ACCURACY) + 0.5);
        // clock_rate/sample_rate ratio is too large
        assert(factor_ > 0);
    }

    /// Return number of source time unites per second.
    inline int32_t get_clock_rate() const { return clocks_per_sec; }

    /// Return length of buffer, in milliseconds
    inline int get_length() const { return length_; }

    /// Number of samples delay from synthesis to samples read out
    inline int get_output_latency() const { return WIDEST_IMPULSE / 2; }

    /// Set frequency at which high-pass filter attenuation passes -3dB
    inline void bass_freq(int freq) {
        bass_freq_ = freq;
        if (freq == 0) {
            bass_shift = 31;  // 32 or greater invokes undefined behavior elsewhere
            return;
        }
        bass_shift = 1 + (int) floor(1.442695041 * log(0.124 * samples_per_sec / freq));
        if (bass_shift < 0)  bass_shift = 0;
        if (bass_shift > 24) bass_shift = 24;
    }

    /// Remove all available samples and clear buffer to silence. If
    /// 'entire_buffer' is false, just clear out any samples waiting rather
    /// than the entire buffer.
    inline void clear(bool entire_buffer = true) {
        int32_t count = (entire_buffer ? buffer_size_ : samples_count());
        offset_ = 0;
        reader_accum = 0;
        memset(buffer_, sample_offset & 0xFF, (count + WIDEST_IMPULSE) * sizeof (buf_t_));
    }

    /// End current time frame of specified duration and make its samples
    /// available (aint32_t with any still-unread samples) for reading with
    /// read_samples(). Begin a new time frame at the end of the current
    /// frame. All transitions must have been added before 'time'.
    inline void end_frame(blip_time_t time) {
        offset_ += time * factor_;
        assert(("BLIPBuffer::end_frame(): Frame went past end of buffer", samples_count() <= (int32_t) buffer_size_));
    }

    /// Return the number of samples available for reading with read_samples().
    inline int32_t samples_count() const {
        return int32_t (offset_ >> BLIP_BUFFER_ACCURACY);
    }

    /// Read at most 'max_samples' out of buffer into 'dest', removing them
    /// from the buffer. Return number of samples actually read and removed.
    /// If stereo is true, increment 'dest' one extra time after writing each
    /// sample, to allow easy interleving of two channels into a stereo output
    /// buffer.
    int32_t read_samples(
        blip_sample_t* dest,
        int32_t max_samples,
        bool stereo = false
    );

    /// Remove 'count' samples from those waiting to be read
    void remove_samples(int32_t count) {
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

    // Experimental external buffer mixing support

    /// Number of raw samples that can be mixed within frame of specified
    /// duration
    inline int32_t count_samples(blip_time_t duration) const {
        return (resampled_time(duration) >> BLIP_BUFFER_ACCURACY) - (offset_ >> BLIP_BUFFER_ACCURACY);
    }

    /// Mix 'count' samples from 'buf' into buffer.
    void mix_samples(const blip_sample_t* in, int32_t count) {
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

    // not documented yet

    inline void remove_silence(int32_t count) {
        assert(("BLIPBuffer::remove_silence(): Tried to remove more samples than available", count <= samples_count()));
        offset_ -= resampled_time_t (count) << BLIPBuffer::BLIP_BUFFER_ACCURACY;
    }

    typedef uint32_t resampled_time_t;

    inline resampled_time_t resampled_time(blip_time_t t) const {
        return t * resampled_time_t (factor_) + offset_;
    }

    inline resampled_time_t resampled_duration(int t) const {
        return t * resampled_time_t (factor_);
    }

 private:
    /// Disable the public copy constructor.
    BLIPBuffer(const BLIPBuffer&);

    /// Disable the public assignment operator.
    BLIPBuffer& operator=(const BLIPBuffer&);

// Don't use the following members. They are public only for technical reasons.
 public:
        static constexpr int WIDEST_IMPULSE = 24;
        typedef uint16_t buf_t_;

        uint32_t factor_;
        resampled_time_t offset_;
        buf_t_* buffer_;
        unsigned buffer_size_;

 private:
        int32_t reader_accum;
        int bass_shift;
        int32_t samples_per_sec;
        int32_t clocks_per_sec;
        int bass_freq_;
        int length_;

        // less than 16 to give extra sample range
        enum { accum_fract = 15 };
        // repeated byte allows memset to clear buffer
        enum { sample_offset = 0x7F7F };
};

// Low-pass equalization parameters (see notes.txt)
class blip_eq_t {
 public:
    blip_eq_t(double treble_ = 0) :
        treble(treble_), cutoff(0), sample_rate(44100) { }

    blip_eq_t(double treble_, int32_t cutoff_, int32_t sample_rate_) :
        treble(treble_), cutoff(cutoff_), sample_rate(sample_rate_) { }

 private:
    double treble;
    int32_t cutoff;
    int32_t sample_rate;
    friend class BLIPImpulse;
};

// End of public interface

static constexpr int BLIP_RES_BITS = 5;
static constexpr int BLIP_MAX_RES = 1 << BLIP_RES_BITS;

typedef uint32_t blip_pair_t_;

class BLIPImpulse {
    static constexpr int IMPULSE_BITS = 15;
    static constexpr int32_t IMPULSE_AMP = 1L << IMPULSE_BITS;
    static constexpr int32_t IMPULSE_OFFSET = IMPULSE_AMP / 2;

    typedef uint16_t imp_t;

    blip_eq_t eq;
    double  volume_unit_;
    imp_t*  impulses;
    imp_t*  impulse;
    int     width;
    int     fine_bits;
    int     res;
    bool    generate;

    void fine_volume_unit() {
        // to do: find way of merging in-place without temporary buffer
        imp_t temp[BLIP_MAX_RES * 2 * BLIPBuffer::WIDEST_IMPULSE];
        scale_impulse((offset & 0xffff) << fine_bits, temp);
        imp_t* imp2 = impulses + res * 2 * width;
        scale_impulse(offset & 0xffff, imp2);

        // merge impulses
        imp_t* imp = impulses;
        imp_t* src2 = temp;
        for (int n = res / 2 * 2 * width; n--;) {
            *imp++ = *imp2++;
            *imp++ = *imp2++;
            *imp++ = *src2++;
            *imp++ = *src2++;
        }
    }

    void scale_impulse(int unit, imp_t* imp_in) const {
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

 public:
    BLIPBuffer* buf;
    uint32_t offset;

    inline void init(blip_pair_t_* imps, int w, int r, int fb = 0) {
        fine_bits = fb;
        width = w;
        impulses = reinterpret_cast<imp_t*>(imps);
        generate = true;
        volume_unit_ = -1.0;
        res = r;
        buf = NULL;

        impulse = &impulses[width * res * 2 * (fine_bits ? 2 : 1)];
        offset = 0;
    }

    void volume_unit(double new_unit) {
        if (new_unit == volume_unit_) return;
        if (generate) treble_eq(blip_eq_t(-8.87, 8800, 44100));
        volume_unit_ = new_unit;
        offset = 0x10001 * (uint32_t) floor(volume_unit_ * 0x10000 + 0.5);
        if (fine_bits)
            fine_volume_unit();
        else
            scale_impulse(offset & 0xffff, impulses);
    }

    void treble_eq(const blip_eq_t& new_eq) {
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
};

#endif  // BLIP_BUFFER_BLIP_BUFFER_HPP
