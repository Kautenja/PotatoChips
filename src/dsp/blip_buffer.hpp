// Band-limited sound synthesis buffer (forked from Blip_Buffer 0.4.1).
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

#ifndef DSP_BLIP_BUFFER_HPP_
#define DSP_BLIP_BUFFER_HPP_

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cmath>
#include <limits>
#include "exceptions.hpp"

/// A 32-bit signed value
typedef int32_t blip_long;

/// A 32-bit unsigned value
typedef uint32_t blip_ulong;

/// A time unit at source clock rate
typedef blip_long blip_time_t;

/// An output sample type for 16-bit signed samples[-32768, 32767]
typedef int16_t blip_sample_t;

/// A re-sampled time unit
typedef blip_ulong blip_resampled_time_t;

/// The number of bits in re-sampled ratio fraction. Higher values give a more
/// accurate ratio but reduce maximum buffer size.
static constexpr uint8_t BLIP_BUFFER_ACCURACY = 16;

/// Number bits in phase offset. Fewer than 6 bits (64 phase offsets) results
/// in noticeable broadband noise when synthesizing high frequency square
/// waves. Affects size of BLIPSynthesizer objects since they store the waveform
/// directly.
static constexpr uint8_t BLIP_PHASE_BITS = 6;

/// TODO:
static constexpr int blip_widest_impulse_ = 16;

/// TODO:
static constexpr int blip_buffer_extra_ = blip_widest_impulse_ + 2;

/// TODO:
static constexpr int blip_res = 1 << BLIP_PHASE_BITS;

/// TODO:
static constexpr uint32_t blip_max_length = 0;

/// TODO:
static constexpr uint32_t blip_default_length = 250;

/// TODO:
static constexpr uint8_t blip_sample_bits = 30;

/// Constant value to use instead of BLIP_READER_BASS(), for slightly more
/// optimal code at the cost of having no bass control
static constexpr uint32_t blip_reader_default_bass = 9;

/// maximal length that re-sampled time can represent
static constexpr uint32_t MAX_RESAMPLED_TIME =
    (std::numeric_limits<uint32_t>::max() >> BLIP_BUFFER_ACCURACY) - blip_buffer_extra_ - 64;

#if defined (__GNUC__) || _MSC_VER >= 1100
    #define BLIP_RESTRICT __restrict
#else
    #define BLIP_RESTRICT
#endif

/// A Band-limited sound synthesis buffer.
class BLIPBuffer {
 private:
    /// The sample rate to generate samples from the buffer at
    uint32_t sample_rate_ = 0;
    /// The clock rate of the chip to emulate
    uint32_t clock_rate_ = 0;
    /// the frequency of the high-pass filter (TODO: in Hz?)
    int bass_freq_ = 16;

    /// Disable the copy constructor.
    BLIPBuffer(const BLIPBuffer&);

    /// Disable the assignment operator
    BLIPBuffer& operator=(const BLIPBuffer&);

 public:
    typedef blip_time_t buf_t_;
    /// TODO:
    blip_ulong factor_ = 1;
    /// TODO:
    buf_t_* buffer_ = 0;
    /// TODO:
    uint32_t buffer_size_ = 0;
    /// TODO:
    blip_long reader_accum_ = 0;
    /// TODO:
    int bass_shift_ = 0;

    /// Initialize a new BLIP Buffer.
    BLIPBuffer() { }

    /// Destroy an existing BLIP Buffer.
    ~BLIPBuffer() { free(buffer_); }

    /// The result from setting the sample rate to a new value
    enum class SampleRateStatus {
        Success = 0,               // setting the sample rate succeeded
        BufferLengthExceedsLimit,  // requested length exceeds limit
        OutOfMemory                // ran out of resources for buffer
    };

    /// @brief Set the output sample rate and buffer length in milliseconds.
    ///
    /// @param samples_per_sec the number of samples per second
    /// @param clock_cycles_per_sec the number of source clock cycles per second
    /// @param buffer_length length of the buffer in milliseconds (1/1000 sec).
    /// defaults to 250, i.e., 1/4 sec.
    /// @returns NULL on success, otherwise if there isn't enough memory,
    /// returns error without affecting current buffer setup.
    ///
    SampleRateStatus set_sample_rate(
        uint32_t samples_per_sec,
        uint32_t clock_cycles_per_sec,
        uint32_t buffer_length = 1000 / 4
    ) {
        // check the size parameter
        uint32_t new_size = MAX_RESAMPLED_TIME;
        if (buffer_length != blip_max_length) {
            uint32_t size = (samples_per_sec * (buffer_length + 1) + 999) / 1000;
            if (size >= new_size)  // fails if requested length exceeds limit
                return SampleRateStatus::BufferLengthExceedsLimit;
            new_size = size;
        }
        // resize the buffer
        if (buffer_size_ != new_size) {
            void* p = realloc(buffer_, (new_size + blip_buffer_extra_) * sizeof *buffer_);
            // if the reallocation failed, return an out of memory flag
            if (!p) return SampleRateStatus::OutOfMemory;
            // update the buffer and buffer size
            buffer_ = (buf_t_*) p;
            buffer_size_ = new_size;
        }
        // update instance variables based on the new sample rate
        sample_rate_ = samples_per_sec;
        // update the high-pass filter
        bass_freq(bass_freq_);
        // calculate the number of cycles per sample (round by truncation)
        uint32_t cycles_per_sample = clock_cycles_per_sec / samples_per_sec;
        // re-calculate the clock rate with rounding error accounted for
        clock_rate_ = cycles_per_sample * samples_per_sec;
        // calculate the time factor based on the clock_rate and sample_rate
        factor_ = clock_rate_factor(clock_rate_);
        // clear the buffer
        reader_accum_ = 0;
        // return success flag
        return SampleRateStatus::Success;
    }

    /// @brief Return the current output sample rate.
    ///
    /// @returns the audio sample rate
    ///
    inline uint32_t get_sample_rate() const { return sample_rate_; }

    /// @brief Return the number of source time units per second.
    ///
    /// @returns the number of source time units per second
    ///
    inline uint32_t get_clock_rate() const { return clock_rate_; }

    /// @brief Read out of this buffer into `dest` and remove them from the buffer.
    ///
    /// @param output the output array to push samples from the buffer into
    /// @returns the sample
    ///
    blip_sample_t read_sample() {
        // create a temporary pointer to the buffer that can be mutated
        const buf_t_* BLIP_RESTRICT buffer_temp = buffer_;
        // get the current accumulator
        blip_long read_accum_temp = reader_accum_;
        // get the sample from the accumulator
        blip_long sample = read_accum_temp >> (blip_sample_bits - 16);
        if (static_cast<blip_sample_t>(sample) != sample)
            sample = std::numeric_limits<blip_sample_t>::max() - (sample >> 24);
        read_accum_temp += *buffer_temp - (read_accum_temp >> (bass_shift_));
        // update the accumulator
        reader_accum_ = read_accum_temp;
        // -------------------------------------------------------------------
        // TODO: remove
        // -------------------------------------------------------------------
        // copy remaining samples to beginning and clear old samples
        static constexpr auto count = 1;
        long remain = count + blip_buffer_extra_;
        memmove(buffer_, buffer_ + count, remain * sizeof *buffer_);
        memset(buffer_ + remain, 0, count * sizeof *buffer_);
        // -------------------------------------------------------------------
        return sample;
    }

    /// @brief Set frequency high-pass filter frequency, where higher values
    /// reduce the bass more.
    ///
    /// @param frequency TODO:
    ///
    inline void bass_freq(int frequency) {
        bass_freq_ = frequency;
        int shift = 31;
        if (frequency > 0) {
            shift = 13;
            long f = (frequency << 16) / sample_rate_;
            while ((f >>= 1) && --shift) { }
        }
        bass_shift_ = shift;
    }

// ---------------------------------------------------------------------------
// TODO: not documented yet
// ---------------------------------------------------------------------------

    inline blip_resampled_time_t resampled_duration(int time) const {
        return time * factor_;
    }

    inline blip_resampled_time_t resampled_time(blip_time_t time) const {
        return time * factor_;
    }

    /// @brief Return the clock rate factor based on given clock rate and the
    /// current sample rate.
    ///
    /// @param clock_rate the clock rate to calculate the clock rate factor of
    /// @returns the number of clock cycles per sample
    /// @details
    /// throws an exception if the factor is too large or the sample rate is
    /// not set
    ///
    inline blip_resampled_time_t clock_rate_factor(uint32_t clock_rate) const {
        double ratio = static_cast<double>(sample_rate_) / clock_rate;
        blip_long factor = floor(ratio * (1L << BLIP_BUFFER_ACCURACY) + 0.5);
        if (!(factor > 0 || !sample_rate_))  // fails if ratio is too large
            throw Exception("sample_rate : clock_rate ratio is too large");
        return factor;
    }
};

/// Low-pass equalization parameters and logic.
class BLIPEqualizer {
 private:
    /// the constant value for Pi
    static constexpr double pi = 3.1415926535897932384626433832795029;
    /// Logarithmic roll-off to treble dB at half sampling rate. Negative
    /// values reduce treble, small positive values (0 to 5.0) increase treble.
    double treble;
    /// TODO:
    uint32_t rolloff_freq;
    /// the sample rate the engine is running at
    uint32_t sample_rate;
    /// TODO:
    uint32_t cutoff_freq;

    /// Generate a sinc.
    ///
    /// @param out the output buffer to generate sinc values into
    /// @param count the number of samples to generate
    /// @param oversample TODO:
    /// @param treble Logarithmic roll-off to treble dB at half sampling rate.
    /// Negative values reduce treble, small positive values (0 to 5.0)
    /// increase treble.
    /// @param cutoff TODO:
    ///
    static inline void gen_sinc(
        float* out,
        uint32_t count,
        double oversample,
        double treble,
        double cutoff
    ) {
        if (cutoff >= 0.999) cutoff = 0.999;
        if (treble < -300.0) treble = -300.0;
        if (treble > 5.0)    treble = 5.0;
        static constexpr double maxh = 4096.0;
        const double rolloff =
            pow(10.0, 1.0 / (maxh * 20.0) * treble / (1.0 - cutoff));
        const double pow_a_n = pow(rolloff, maxh - maxh * cutoff);
        const double to_angle = pi / 2 / maxh / oversample;
        for (uint32_t i = 0; i < count; i++) {
            double angle = ((i - count) * 2 + 1) * to_angle;
            double c = rolloff * cos((maxh - 1.0) * angle) - cos(maxh * angle);
            double cos_nc_angle = cos(maxh * cutoff * angle);
            double cos_nc1_angle = cos((maxh * cutoff - 1.0) * angle);
            double cos_angle = cos(angle);
            c = c * pow_a_n - rolloff * cos_nc1_angle + cos_nc_angle;
            double d = 1.0 + rolloff * (rolloff - cos_angle - cos_angle);
            double b = 2.0 - cos_angle - cos_angle;
            double a = 1.0 - cos_angle - cos_nc_angle + cos_nc1_angle;
            // a / b + c / d
            out[i] = (a * d + c * b) / (b * d);
        }
    }

 public:
    /// Initialize a new BLIPEqualizer.
    ///
    /// @param treble Logarithmic rolloff to treble dB at half sampling rate.
    /// Negative values reduce treble, small positive values (0 to 5.0) increase
    /// treble.
    /// @param rolloff_freq TODO:
    /// @param sample_rate the sample rate the engine is running at
    /// @param cutoff_freq TODO:
    ///
    explicit BLIPEqualizer(
        double treble,
        uint32_t rolloff_freq = 0,
        uint32_t sample_rate = 44100,
        uint32_t cutoff_freq = 0
    ) :
        treble(treble),
        rolloff_freq(rolloff_freq),
        sample_rate(sample_rate),
        cutoff_freq(cutoff_freq) { }

    /// Generate sinc values into an output buffer with given quantity.
    ///
    /// @param out the output buffer to equalize
    /// @param count the number of samples to generate
    /// @details
    /// for usage within instances of BLIPSynthesizer_
    ///
    inline void _generate(float* out, uint32_t count) const {
        // lower cutoff freq for narrow kernels with their wider transition band
        // (8 points->1.49, 16 points->1.15)
        double half_rate = sample_rate * 0.5;
        double oversample = cutoff_freq ?
            half_rate / cutoff_freq :
            blip_res * 2.25 / count + 0.85;
        double cutoff = rolloff_freq * oversample / half_rate;
        // generate a sinc
        gen_sinc(out, count, blip_res * oversample, treble, cutoff);
        // apply (half of) hamming window
        double to_fraction = pi / (count - 1);
        for (uint32_t i = count; i--;)
            out[i] *= 0.54f - 0.46f * cos(i * to_fraction);
    }
};

/// the synthesis quality level. Start with blip_good_quality.
enum BLIPQuality {
    blip_med_quality  = 8,
    blip_good_quality = 12,
    blip_high_quality = 16
};

/// @brief A digital synthesizer for arbitrary waveforms based on BLIP.
/// @tparam quality the quality of the BLIP algorithm
/// @tparam range specifies the greatest expected change in amplitude.
/// Calculate it by finding the difference between the maximum and minimum
/// expected amplitudes (max - min).
///
template<BLIPQuality quality, int range>
class BLIPSynthesizer {
 private:
    /// TODO:
    double volume_unit;
    /// TODO:
    blip_sample_t impulses[blip_res * (quality / 2) + 1];
    /// TODO:
    blip_long kernel_unit;

    /// TODO:
    inline int impulses_size() const { return blip_res / 2 * quality + 1; }

    /// TODO:
    void adjust_impulse() {
        // sum pairs for each phase and add error correction to end of first half
        int const size = impulses_size();
        for (int p = blip_res; p-- >= blip_res / 2;) {
            int p2 = blip_res - 2 - p;
            long error = kernel_unit;
            for (int i = 1; i < size; i += blip_res) {
                error -= impulses[i + p ];
                error -= impulses[i + p2];
            }
            if (p == p2)  // phase = 0.5 impulse uses same half for both sides
                error /= 2;
            impulses[size - blip_res + p] += (blip_sample_t) error;
        }
    }

 public:
    /// the output buffer that the synthesizer writes samples to
    BLIPBuffer* buf;
    /// the last amplitude value (DPCM sample) to output from the synthesizer
    int last_amp;
    /// TODO:
    int delta_factor;

    /// Initialize a new BLIP synthesizer.
    BLIPSynthesizer() :
        volume_unit(0.0),
        kernel_unit(0),
        buf(0),
        last_amp(0),
        delta_factor(0) {
        memset(impulses, 0, sizeof impulses);
    }

    /// TODO:
    void volume(double new_unit) {
        new_unit = new_unit * (1.0 / (range < 0 ? -range : range));
        if (new_unit != volume_unit) {
            // use default eq if it hasn't been set yet
            if (!kernel_unit)
                treble_eq(BLIPEqualizer(-8.0));

            volume_unit = new_unit;
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

    /// TODO:
    void treble_eq(BLIPEqualizer const& eq) {
        float fimpulse[blip_res / 2 * (blip_widest_impulse_ - 1) + blip_res * 2];

        int const half_size = blip_res / 2 * (quality - 1);
        eq._generate(&fimpulse[blip_res], half_size);

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
        double vol = volume_unit;
        if (vol) {
            volume_unit = 0.0;
            volume(vol);
        }
    }

    /// Get the buffer used for output.
    ///
    /// @returns the buffer that this synthesizer is writing samples to
    ///
    inline BLIPBuffer* output() const { return buf; }

    /// Set the buffer used for output.
    ///
    /// @param buffer the buffer that this synthesizer will write samples to
    ///
    inline void output(BLIPBuffer* buffer) {
        buf = buffer;
        last_amp = 0;
    }

    /// Update amplitude of waveform at given time. Using this requires a
    /// separate BLIPSynthesizer for each waveform.
    ///
    /// @param time the time of the sample
    /// @param amplitude the amplitude of the waveform to synthesizer
    ///
    inline void update(blip_time_t time, int amplitude) {
        int delta = amplitude - last_amp;
        last_amp = amplitude;
        offset_resampled(time * buf->factor_, delta, buf);
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
        offset_resampled(time * buf->factor_, delta, buf);
    }

    inline void offset(blip_time_t time, int delta) const {
        offset(time, delta, buf);
    }

    /// Works directly in terms of fractional output samples. Contact Shay Green
    /// for more info.
    void offset_resampled(blip_resampled_time_t time, int delta, BLIPBuffer* blip_buf) const {
        // Fails if time is beyond end of BLIPBuffer, due to a bug in caller code
        // or the need for a longer buffer as set by set_sample_rate().
        assert((time >> BLIP_BUFFER_ACCURACY) < blip_buf->buffer_size_);
        delta *= delta_factor;
        blip_long* BLIP_RESTRICT buf = blip_buf->buffer_ + (time >> BLIP_BUFFER_ACCURACY);
        int phase = (int) (time >> (BLIP_BUFFER_ACCURACY - BLIP_PHASE_BITS) & (blip_res - 1));

        int const fwd = (blip_widest_impulse_ - quality) / 2;
        int const rev = fwd + quality - 2;
        int const mid = quality / 2 - 1;

        blip_sample_t const* BLIP_RESTRICT imp = impulses + blip_res - phase;

        #if defined (_M_IX86)    || \
            defined (_M_IA64)    || \
            defined (__i486__)   || \
            defined (__x86_64__) || \
            defined (__ia64__)   || \
            defined (__i386__)  // CISC

        // straight forward implementation resulted in better code on GCC for x86

        #define ADD_IMP(out, in) \
            buf[out] += (blip_long) imp[blip_res * (in)] * delta

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

        #else  // CISC (false)

        // for RISC processors, help compiler by reading ahead of writes

        #define BLIP_FWD(i) {\
            blip_long t0 =                       i0 * delta + buf[fwd     + i];\
            blip_long t1 = imp[blip_res * (i + 1)] * delta + buf[fwd + 1 + i];\
            i0 =           imp[blip_res * (i + 2)];\
            buf[fwd     + i] = t0;\
            buf[fwd + 1 + i] = t1;\
        }
        #define BLIP_REV(r) {\
            blip_long t0 =                 i0 * delta + buf[rev     - r];\
            blip_long t1 = imp[blip_res * r] * delta + buf[rev + 1 - r];\
            i0 =           imp[blip_res * (r - 1)];\
            buf[rev     - r] = t0;\
            buf[rev + 1 - r] = t1;\
        }

            blip_long i0 = *imp;
            BLIP_FWD(0)
            if (quality > 8 ) BLIP_FWD(2)
            if (quality > 12) BLIP_FWD(4) {
                blip_long t0 =                   i0 * delta + buf[fwd + mid - 1];
                blip_long t1 = imp[blip_res * mid] * delta + buf[fwd + mid    ];
                imp = impulses + phase;
                i0 = imp[blip_res * mid];
                buf[fwd + mid - 1] = t0;
                buf[fwd + mid    ] = t1;
            }
            if (quality > 12) BLIP_REV(6)
            if (quality > 8 ) BLIP_REV(4)
            BLIP_REV(2)

            blip_long t0 =   i0 * delta + buf[rev    ];
            blip_long t1 = *imp * delta + buf[rev + 1];
            buf[rev    ] = t0;
            buf[rev + 1] = t1;
        #endif  // CISC

        #undef BLIP_FWD
        #undef BLIP_REV
    }
};

#endif  // DSP_BLIP_BUFFER_HPP_
