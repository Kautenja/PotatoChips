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

#ifndef BLIP_BUFFER_HPP_
#define BLIP_BUFFER_HPP_

#include <cstdint>

/// A 32-bit signed value
typedef int32_t blip_long;

/// A 32-bit unsigned value
typedef uint32_t blip_ulong;

/// A time unit at source clock rate
typedef blip_long blip_time_t;

/// An output sample type for 16-bit signed samples [-32768, 32767]
typedef int16_t blip_sample_t;

/// TODO:
typedef blip_ulong blip_resampled_time_t;

/// The number of bits in re-sampled ratio fraction. Higher values give a more
/// accurate ratio but reduce maximum buffer size.
static constexpr uint8_t BLIP_BUFFER_ACCURACY = 16;

/// Number bits in phase offset. Fewer than 6 bits (64 phase offsets) results
/// in noticeable broadband noise when synthesizing high frequency square
/// waves. Affects size of BLIPSynth objects since they store the waveform
/// directly.
#if BLIP_BUFFER_FAST
static constexpr uint8_t BLIP_PHASE_BITS = 8;
#else
static constexpr uint8_t BLIP_PHASE_BITS = 6;
#endif  // BLIP_BUFFER_FAST

/// TODO:
static constexpr int blip_widest_impulse_ = 16;

/// TODO:
static constexpr int blip_buffer_extra_ = blip_widest_impulse_ + 2;

/// TODO:
static constexpr int blip_res = 1 << BLIP_PHASE_BITS;

/// TODO:
static constexpr int blip_max_length = 0;

/// TODO:
static constexpr int blip_default_length = 250;

/// A Band-limited sound synthesis buffer (BLIPBuffer 0.4.1).
class BLIPBuffer {
 public:
    typedef const char* blargg_err_t;

    /// The result from setting the sample rate to a new value
    enum class SampleRateStatus {
        Success = 0,  // setting the sample rate succeeded
        OutOfMemory,  // ran out of resources for buffer
        SilentBuffer  // attempting to resize silent buffer
    };

    /// @brief Set the output sample rate and buffer length in milliseconds.
    ///
    /// @param samples_per_sec the number of samples per second
    /// @param buffer_length length of the buffer in milliseconds (1/1000 sec).
    /// defaults to 250, i.e., 1/4 sec.
    /// @returns NULL on success, otherwise if there isn't enough memory,
    /// returns error without affecting current buffer setup.
    ///
    SampleRateStatus set_sample_rate(
        uint32_t samples_per_sec,
        uint32_t buffer_length = 1000 / 4
    );

    /// @brief Return the current output sample rate.
    ///
    /// @returns the audio sample rate
    ///
    inline uint32_t sample_rate() const { return sample_rate_; }

    /// @brief Return the length of the buffer in milliseconds.
    ///
    /// @returns the length of the buffer in milliseconds (1/1000 sec)
    ///
    inline uint32_t length() const { return length_; }

    /// @brief Set the number of source time units per second.
    ///
    /// @param TODO:
    ///
    inline void set_clock_rate(long cps) {
        factor_ = clock_rate_factor(clock_rate_ = cps);
    }

    /// @brief Return the number of source time units per second.
    ///
    /// @returns the number of source time units per second
    ///
    inline long get_clock_rate() const { return clock_rate_; }

    /// @brief End current time frame of specified duration and make its
    /// samples available (along with any still-unread samples).
    ///
    /// @param time the number of source cycles to complete the frame
    /// @details
    /// Begins a new time frame at the end of the current frame.
    ///
    void end_frame(blip_time_t time);

    /// @brief Return the number of samples available for reading.
    ///
    /// @returns the number of samples available for reading from the buffer
    ///
    inline long samples_count() const {
        return (long) (offset_ >> BLIP_BUFFER_ACCURACY);
    }

    /// @brief Read at most `max_samples` out of this buffer into `dest` and
    /// remove them from the buffer.
    ///
    /// @param dest the destination to push samples from the buffer into
    /// @param max_samples the maximal number of samples to read into the buffer
    /// @param stereo if true increments `dest` one extra time after writing
    /// each sample to allow easy interleaving of two channels into a stereo
    /// output buffer.
    /// @returns the number of samples actually read and removed
    ///
    long read_samples(blip_sample_t* dest, long max_samples, int stereo = 0);

    /// @brief Remove samples from those waiting to be read.
    ///
    /// @param count the number of samples to remove from the buffer
    ///
    void remove_samples(long count);

    /// @brief Remove all available samples and clear buffer to silence.
    ///
    /// @param entire_buffer is false, clears out any samples waiting rather
    /// than the entire buffer.
    ///
    void clear(int entire_buffer = 1);

    /// @brief Set frequency high-pass filter frequency, where higher values
    /// reduce the bass more.
    ///
    /// @param frequency TODO:
    ///
    void bass_freq(int frequency);

    /// @brief Return the number of samples delay from synthesis to samples
    /// available.
    ///
    /// @returns the number of samples delay from synthesis to samples available
    ///
    inline int output_latency() const { return blip_widest_impulse_ / 2; }

// ---------------------------------------------------------------------------
// MARK: Experimental features
// ---------------------------------------------------------------------------

    /// @brief Return the number of clocks needed until `count` samples will be
    /// available.
    ///
    /// @param count the number of samples to convert to clock cycles
    /// @returns the number of clock cycles needed to produce `count` samples.
    /// If buffer can't even hold `count` samples, returns number of clocks
    /// until buffer becomes full.
    ///
    blip_time_t count_clocks(long count) const;

    /// @brief Return the number of raw samples that can be mixed within frame
    /// of given `duration`.
    ///
    /// @param duration the duration of the frame to mix raw sample into
    /// @returns the number of raw samples that can be mixed within frame
    /// of given `duration`
    ///
    long count_samples(blip_time_t duration) const;

    /// @brief Mix 'count' samples from the given buffer into this buffer.
    ///
    /// @param buf the buffer to mix samples from into this buffer
    /// @param count the number of samples to mix from `buf` into this buffer
    ///
    void mix_samples(blip_sample_t const* buf, long count);

// ---------------------------------------------------------------------------
// TODO: not documented yet
// ---------------------------------------------------------------------------

    void set_modified() { modified_ = 1; }

    int clear_modified() { int b = modified_; modified_ = 0; return b; }

    typedef blip_ulong blip_resampled_time_t;

    void remove_silence(long count);

    blip_resampled_time_t resampled_duration(int t) const {
        return t * factor_;
    }

    blip_resampled_time_t resampled_time(blip_time_t t) const {
        return t * factor_ + offset_;
    }

    blip_resampled_time_t clock_rate_factor(long clock_rate) const;

 public:
    /// Initialize a new BLIP Buffer.
    BLIPBuffer();

    /// Destroy an existing BLIP Buffer.
    ~BLIPBuffer();

 private:
    /// Disable the copy constructor.
    BLIPBuffer(const BLIPBuffer&);

    /// Disable the assignment operator
    BLIPBuffer& operator = (const BLIPBuffer&);

 public:
    typedef blip_time_t buf_t_;
    blip_ulong factor_;
    blip_resampled_time_t offset_;
    buf_t_* buffer_;
    blip_long buffer_size_;
    blip_long reader_accum_;
    int bass_shift_;

 private:
    uint32_t sample_rate_;
    long clock_rate_;
    int bass_freq_;
    uint32_t length_;
    int modified_;
    friend class BLIPReader;
};

class blip_eq_t;

class BLIPSynth_Fast_ {
public:
    BLIPBuffer* buf;
    int last_amp;
    int delta_factor;

    void volume_unit(double);
    BLIPSynth_Fast_();
    void treble_eq(blip_eq_t const&) { }
};

class BLIPSynth_ {
public:
    BLIPBuffer* buf;
    int last_amp;
    int delta_factor;

    void volume_unit(double);
    BLIPSynth_(short* impulses, int width);
    void treble_eq(blip_eq_t const&);
private:
    double volume_unit_;
    short* const impulses;
    int const width;
    blip_long kernel_unit;
    int impulses_size() const { return blip_res / 2 * width + 1; }
    void adjust_impulse();
};

// Quality level. Start with blip_good_quality.
const int blip_med_quality  = 8;
const int blip_good_quality = 12;
const int blip_high_quality = 16;

// Range specifies the greatest expected change in amplitude. Calculate it
// by finding the difference between the maximum and minimum expected
// amplitudes (max - min).
template<int quality,int range>
class BLIPSynth {
public:
    // Set overall volume of waveform
    void volume(double v) { impl.volume_unit(v * (1.0 / (range < 0 ? -range : range))); }

    // Configure low-pass filter (see blip_buffer.txt)
    void treble_eq(blip_eq_t const& eq)       { impl.treble_eq(eq); }

    // Get/set BLIPBuffer used for output
    BLIPBuffer* output() const                 { return impl.buf; }
    void output(BLIPBuffer* b)               { impl.buf = b; impl.last_amp = 0; }

    // Update amplitude of waveform at given time. Using this requires a separate
    // BLIPSynth for each waveform.
    // void update(blip_time_t time, int amplitude);
    inline void update(blip_time_t time, int amplitude) {
        int delta = amplitude - impl.last_amp;
        impl.last_amp = amplitude;
        offset_resampled(time * impl.buf->factor_ + impl.buf->offset_, delta, impl.buf);
    }

// ---------------------------------------------------------------------------
// MARK: Low-level interface
// TODO: document
// ---------------------------------------------------------------------------

    /// Add an amplitude transition of specified delta, optionally into
    /// specified buffer rather than the one set with output(). Delta can be
    /// positive or negative. The actual change in amplitude is
    /// delta * (volume / range)
    inline void offset(blip_time_t time, int delta, BLIPBuffer* buf) const {
        offset_resampled(time * buf->factor_ + buf->offset_, delta, buf);
    }

    inline void offset(blip_time_t time, int delta) const {
        offset(time, delta, impl.buf);
    }

    /// Works directly in terms of fractional output samples. Contact Shay Green
    /// for more info.
    void offset_resampled(blip_resampled_time_t time, int delta, BLIPBuffer* buf) const;

private:
#if BLIP_BUFFER_FAST
    BLIPSynth_Fast_ impl;
#else
    BLIPSynth_ impl;
    typedef short imp_t;
    imp_t impulses [blip_res * (quality / 2) + 1];
public:
    BLIPSynth() : impl(impulses, quality) { }
#endif
};

/// Low-pass equalization parameters
class blip_eq_t {
 public:
    /// Logarithmic rolloff to treble dB at half sampling rate. Negative values
    /// reduce treble, small positive values (0 to 5.0) increase treble.
    blip_eq_t(double treble_db) :
        treble(treble_db),
        rolloff_freq(0),
        sample_rate(44100),
        cutoff_freq(0) { }

    // See blip_buffer.txt
    blip_eq_t(
        double treble,
        long rolloff_freq,
        long sample_rate,
        long cutoff_freq
    ) :
        treble(treble),
        rolloff_freq(rolloff_freq),
        sample_rate(sample_rate),
        cutoff_freq(cutoff_freq) { }

 private:
    double treble;
    long rolloff_freq;
    long sample_rate;
    long cutoff_freq;
    void generate(float* out, int count) const;
    friend class BLIPSynth_;
};

int const blip_sample_bits = 30;

// Dummy BLIPBuffer to direct sound output to, for easy muting without
// having to stop sound code.
class Silent_BLIPBuffer : public BLIPBuffer {
    buf_t_ buf [blip_buffer_extra_ + 1];

 public:
    // The following cannot be used (an assertion will fail if attempted):
    blargg_err_t set_sample_rate(long samples_per_sec, int msec_length);
    blip_time_t count_clocks(long count) const;
    void mix_samples(blip_sample_t const* buf, long count);

    Silent_BLIPBuffer();
};

    #if defined (__GNUC__) || _MSC_VER >= 1100
        #define BLIP_RESTRICT __restrict
    #else
        #define BLIP_RESTRICT
    #endif

// Optimized reading from BLIPBuffer, for use in custom sample output

// Begin reading from buffer. Name should be unique to the current block.
#define BLIP_READER_BEGIN(name, blip_buffer) \
    const BLIPBuffer::buf_t_* BLIP_RESTRICT name##_reader_buf = (blip_buffer).buffer_;\
    blip_long name##_reader_accum = (blip_buffer).reader_accum_

// Get value to pass to BLIP_READER_NEXT()
#define BLIP_READER_BASS(blip_buffer) ((blip_buffer).bass_shift_)

// Constant value to use instead of BLIP_READER_BASS(), for slightly more optimal
// code at the cost of having no bass control
int const blip_reader_default_bass = 9;

// Current sample
#define BLIP_READER_READ(name)        (name##_reader_accum >> (blip_sample_bits - 16))

// Current raw sample in full internal resolution
#define BLIP_READER_READ_RAW(name)    (name##_reader_accum)

// Advance to next sample
#define BLIP_READER_NEXT(name, bass) \
    (void) (name##_reader_accum += *name##_reader_buf++ - (name##_reader_accum >> (bass)))

// End reading samples from buffer. The number of samples read must now be removed
// using BLIPBuffer::remove_samples().
#define BLIP_READER_END(name, blip_buffer) \
    (void) ((blip_buffer).reader_accum_ = name##_reader_accum)


// Compatibility with older version
const long blip_unscaled = 65535;
const int blip_low_quality  = blip_med_quality;
const int blip_best_quality = blip_high_quality;

/// Deprecated; use BLIP_READER macros as follows:
/// BLIPReader r; r.begin(buf); -> BLIP_READER_BEGIN(r, buf);
/// int bass = r.begin(buf)      -> BLIP_READER_BEGIN(r, buf); int bass = BLIP_READER_BASS(buf);
/// r.read()                       -> BLIP_READER_READ(r)
/// r.read_raw()                   -> BLIP_READER_READ_RAW(r)
/// r.next(bass)                 -> BLIP_READER_NEXT(r, bass)
/// r.next()                       -> BLIP_READER_NEXT(r, blip_reader_default_bass)
/// r.end(buf)                   -> BLIP_READER_END(r, buf)
class BLIPReader {
 public:
    inline int begin(const BLIPBuffer& blip_buf) {
        buf = blip_buf.buffer_;
        accum = blip_buf.reader_accum_;
        return blip_buf.bass_shift_;
    }

    blip_long read() const          { return accum >> (blip_sample_bits - 16); }
    blip_long read_raw() const      { return accum; }
    void next(int bass_shift = 9)         { accum += *buf++ - (accum >> bass_shift); }
    void end(BLIPBuffer& b)              { b.reader_accum_ = accum; }

private:
    const BLIPBuffer::buf_t_* buf;
    blip_long accum;
};

// ---------------------------------------------------------------------------
// MARK: End of public interface
// ---------------------------------------------------------------------------

#include <assert.h>

template<int quality,int range>
inline void BLIPSynth<quality,range>::offset_resampled(blip_resampled_time_t time,
        int delta, BLIPBuffer* blip_buf) const {
    // Fails if time is beyond end of BLIPBuffer, due to a bug in caller code or the
    // need for a longer buffer as set by set_sample_rate().
    assert((blip_long) (time >> BLIP_BUFFER_ACCURACY) < blip_buf->buffer_size_);
    delta *= impl.delta_factor;
    blip_long* BLIP_RESTRICT buf = blip_buf->buffer_ + (time >> BLIP_BUFFER_ACCURACY);
    int phase = (int) (time >> (BLIP_BUFFER_ACCURACY - BLIP_PHASE_BITS) & (blip_res - 1));

#if BLIP_BUFFER_FAST
    blip_long left = buf [0] + delta;

    // Kind of crappy, but doing shift after multiply results in overflow.
    // Alternate way of delaying multiply by delta_factor results in worse
    // sub-sample resolution.
    blip_long right = (delta >> BLIP_PHASE_BITS) * phase;
    left  -= right;
    right += buf [1];

    buf [0] = left;
    buf [1] = right;
#else

    int const fwd = (blip_widest_impulse_ - quality) / 2;
    int const rev = fwd + quality - 2;
    int const mid = quality / 2 - 1;

    imp_t const* BLIP_RESTRICT imp = impulses + blip_res - phase;

    #if defined (_M_IX86) || defined (_M_IA64) || defined (__i486__) || \
            defined (__x86_64__) || defined (__ia64__) || defined (__i386__)

    // straight forward implementation resulted in better code on GCC for x86

    #define ADD_IMP(out, in) \
        buf [out] += (blip_long) imp [blip_res * (in)] * delta

    #define BLIP_FWD(i) {\
        ADD_IMP(fwd     + i, i    );\
        ADD_IMP(fwd + 1 + i, i + 1);\
    }
    #define BLIP_REV(r) {\
        ADD_IMP(rev     - r, r + 1);\
        ADD_IMP(rev + 1 - r, r    );\
    }

        BLIP_FWD(0)
        if (quality > 8 ) BLIP_FWD(2)
        if (quality > 12) BLIP_FWD(4) {
            ADD_IMP(fwd + mid - 1, mid - 1);
            ADD_IMP(fwd + mid    , mid    );
            imp = impulses + phase;
        }
        if (quality > 12) BLIP_REV(6)
        if (quality > 8 ) BLIP_REV(4)
        BLIP_REV(2)

        ADD_IMP(rev    , 1);
        ADD_IMP(rev + 1, 0);

    #else

    // for RISC processors, help compiler by reading ahead of writes

    #define BLIP_FWD(i) {\
        blip_long t0 =                       i0 * delta + buf [fwd     + i];\
        blip_long t1 = imp [blip_res * (i + 1)] * delta + buf [fwd + 1 + i];\
        i0 =           imp [blip_res * (i + 2)];\
        buf [fwd     + i] = t0;\
        buf [fwd + 1 + i] = t1;\
    }
    #define BLIP_REV(r) {\
        blip_long t0 =                 i0 * delta + buf [rev     - r];\
        blip_long t1 = imp [blip_res * r] * delta + buf [rev + 1 - r];\
        i0 =           imp [blip_res * (r - 1)];\
        buf [rev     - r] = t0;\
        buf [rev + 1 - r] = t1;\
    }

        blip_long i0 = *imp;
        BLIP_FWD(0)
        if (quality > 8 ) BLIP_FWD(2)
        if (quality > 12) BLIP_FWD(4) {
            blip_long t0 =                   i0 * delta + buf [fwd + mid - 1];
            blip_long t1 = imp [blip_res * mid] * delta + buf [fwd + mid    ];
            imp = impulses + phase;
            i0 = imp [blip_res * mid];
            buf [fwd + mid - 1] = t0;
            buf [fwd + mid    ] = t1;
        }
        if (quality > 12) BLIP_REV(6)
        if (quality > 8 ) BLIP_REV(4)
        BLIP_REV(2)

        blip_long t0 =   i0 * delta + buf [rev    ];
        blip_long t1 = *imp * delta + buf [rev + 1];
        buf [rev    ] = t0;
        buf [rev + 1] = t1;
    #endif

#endif
}

#undef BLIP_FWD
#undef BLIP_REV

#endif  // BLIP_BUFFER_HPP_
