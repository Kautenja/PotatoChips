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
    BLIPBuffer();

    /// Destroy an instance of BLIPBuffer.
    ~BLIPBuffer() { delete[] buffer_; }

    /// Set output sample rate and buffer length in milliseconds (1/1000 sec),
    /// then clear buffer. If there is insufficient memory for the buffer,
    /// sets the buffer length to 0 and returns error string or propagates
    /// exception if compiler supports it.
    const char* set_sample_rate(int32_t new_rate) {
        unsigned new_size = (UINT_MAX >> BLIP_BUFFER_ACCURACY) + 1 - widest_impulse_ - 64;

        if (buffer_size_ != new_size) {
            delete[] buffer_;
            buffer_ = NULL;  // allow for exception in allocation below
            buffer_size_ = 0;
            offset_ = 0;
            buffer_ = new buf_t_[new_size + widest_impulse_];
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
    inline int get_output_latency() const { return widest_impulse_ / 2; }

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
        memset(buffer_, sample_offset & 0xFF, (count + widest_impulse_) * sizeof (buf_t_));
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
    void remove_samples(int32_t count);

    // Experimental external buffer mixing support

    /// Number of raw samples that can be mixed within frame of specified
    /// duration
    inline int32_t count_samples(blip_time_t duration) const {
        return (resampled_time(duration) >> BLIP_BUFFER_ACCURACY) - (offset_ >> BLIP_BUFFER_ACCURACY);
    }

    /// Mix 'count' samples from 'buf' into buffer.
    void mix_samples(const blip_sample_t* buf, int32_t count);

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
    // noncopyable
    BLIPBuffer(const BLIPBuffer&);
    BLIPBuffer& operator = (const BLIPBuffer&);

// Don't use the following members. They are public only for technical reasons.
 public:
        enum { widest_impulse_ = 24 };
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

const int blip_res_bits_ = 5;

typedef uint32_t blip_pair_t_;

class BLIPImpulse {
    typedef uint16_t imp_t;

    blip_eq_t eq;
    double  volume_unit_;
    imp_t*  impulses;
    imp_t*  impulse;
    int     width;
    int     fine_bits;
    int     res;
    bool    generate;

    void fine_volume_unit();

    void scale_impulse(int unit, imp_t*) const;

 public:
    BLIPBuffer* buf;
    uint32_t offset;

    void init(blip_pair_t_* impulses, int width, int res, int fine_bits = 0);

    void volume_unit(double);

    void treble_eq(const blip_eq_t&);
};

#endif  // BLIP_BUFFER_BLIP_BUFFER_HPP
