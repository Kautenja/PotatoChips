
// Buffer of sound samples into which band-limited waveforms can be synthesized
// using Blip_Wave or Blip_Synth.

// Blip_Buffer 0.3.3. Copyright (C) 2003-2005 Shay Green. GNU LGPL license.

#ifndef BLIP_BUFFER_H
#define BLIP_BUFFER_H

#include <cstdint>
#include <cassert>

/// Source time unit.
typedef int32_t blip_time_t;

/// Type for sample produced. Signed 16-bit format.
typedef int16_t blip_sample_t;

class Blip_Buffer {
 public:
    static constexpr auto BLIP_BUFFER_ACCURACY = 16;

    /// Initialize an empty BLIPBuffer.
    Blip_Buffer();

    /// Destroy an instance of BLIPBuffer.
    ~Blip_Buffer();

    /// Set output sample rate and buffer length in milliseconds (1/1000 sec),
    /// then clear buffer. If there is insufficient memory for the buffer,
    /// sets the buffer length to 0 and returns error string or propagates
    /// exception if compiler supports it.
    const char* set_sample_rate(int32_t samples_per_sec);

    /// Return current output sample rate.
    inline int32_t get_sample_rate() const { return samples_per_sec; };

    /// Set number of source time units per second.
    void set_clock_rate(int32_t);

    /// Return number of source time unites per second.
    inline int32_t get_clock_rate() const { return clocks_per_sec; }

    /// Return length of buffer, in milliseconds
    inline int get_length() const { return length_; }

    /// Set frequency at which high-pass filter attenuation passes -3dB
    void bass_freq(int frequency);

    /// Remove all available samples and clear buffer to silence. If
    /// 'entire_buffer' is false, just clear out any samples waiting rather
    /// than the entire buffer.
    void clear(bool entire_buffer = true);

    /// End current time frame of specified duration and make its samples
    /// available (aint32_t with any still-unread samples) for reading with
    /// read_samples(). Begin a new time frame at the end of the current
    /// frame. All transitions must have been added before 'time'.
    inline void end_frame(blip_time_t time) {
        offset_ += time * factor_;
        assert(("Blip_Buffer::end_frame(): Frame went past end of buffer", samples_avail() <= (int32_t) buffer_size_));
    }

    /// Return the number of samples available for reading with read_samples().
    inline int32_t samples_avail() const {
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

    /// Number of samples delay from synthesis to samples read out
    inline int get_output_latency() const { return widest_impulse_ / 2; }

    // Experimental external buffer mixing support

    /// Number of raw samples that can be mixed within frame of specified
    /// duration
    int32_t count_samples(blip_time_t duration) const;

    /// Mix 'count' samples from 'buf' into buffer.
    void mix_samples(const blip_sample_t* buf, int32_t count);

    // not documented yet

    inline void remove_silence(int32_t count) {
        assert(("Blip_Buffer::remove_silence(): Tried to remove more samples than available", count <= samples_avail()));
        offset_ -= resampled_time_t (count) << Blip_Buffer::BLIP_BUFFER_ACCURACY;
    }

    typedef uint32_t resampled_time_t;

    resampled_time_t resampled_time(blip_time_t t) const {
        return t * resampled_time_t (factor_) + offset_;
    }

    resampled_time_t resampled_duration(int t) const {
        return t * resampled_time_t (factor_);
    }

 private:
    // noncopyable
    Blip_Buffer(const Blip_Buffer&);
    Blip_Buffer& operator = (const Blip_Buffer&);

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
    friend class Blip_Impulse_;
};

// End of public interface

const int blip_res_bits_ = 5;

typedef uint32_t blip_pair_t_;

class Blip_Impulse_ {
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
    Blip_Buffer* buf;
    uint32_t offset;

    void init(blip_pair_t_* impulses, int width, int res, int fine_bits = 0);

    void volume_unit(double);

    void treble_eq(const blip_eq_t&);
};

// MSVC6 fix
typedef Blip_Buffer::resampled_time_t blip_resampled_time_t;

#endif  // BLIP_BUFFER_H
