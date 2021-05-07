// Band-limited sound synthesis buffer (forked from Blip_Buffer 0.4.1).
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

#ifndef DSP_BLIP_BUFFER_HPP_
#define DSP_BLIP_BUFFER_HPP_

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>
#include "exceptions.hpp"

/// A time unit at source clock rate
typedef int32_t blip_time_t;

/// The number of bits in re-sampled ratio fraction. Higher values give a more
/// accurate ratio but reduce maximum buffer size.
static constexpr uint32_t BLIP_BUFFER_ACCURACY = 16;

/// Number bits in phase offset. Fewer than 6 bits (64 phase offsets) results
/// in noticeable broadband noise when synthesizing high frequency square
/// waves. Affects size of BLIPSynthesizer objects since they store the waveform
/// directly.
static constexpr uint32_t BLIP_PHASE_BITS = 6;

/// TODO:
static constexpr int BLIP_WIDEST_IMPULSE = 16;

/// TODO:
static constexpr int BLIP_RES = 1 << BLIP_PHASE_BITS;

/// TODO:
static constexpr uint32_t BLIP_SAMPLE_BITS = 30;

#if defined (__GNUC__) || _MSC_VER >= 1100
    #define BLIP_RESTRICT __restrict
#else
    #define BLIP_RESTRICT
#endif

/// @brief A Band-limited impulse polynomial buffer.
class BLIPBuffer {
 protected:
    /// The sample rate to generate samples from the buffer at
    uint32_t sample_rate = 0;
    /// The clock rate of the chip to emulate
    uint32_t clock_rate = 0;
    /// the clock rate factor, i.e., the number of CPU samples per audio sample
    uint32_t factor = 1L << BLIP_BUFFER_ACCURACY;

    /// the cut-off frequency of the high-pass filter in Hz
    int32_t bass_freq = 16;
    /// the number of shifts to adjust samples to filter out bass according to
    /// the cut-off frequency of the hi-pass filter (`bass_freq`)
    int32_t bass_shift = 0;

    /// the accumulator for integrating samples into
    int32_t accumulator = 0;
    /// the buffer of samples in the BLIP buffer
    int32_t buffer[BLIP_WIDEST_IMPULSE + 1];

 private:
    /// Disable the copy constructor.
    BLIPBuffer(const BLIPBuffer&);

    /// Disable the assignment operator.
    BLIPBuffer& operator=(const BLIPBuffer&);

 public:
    /// @brief Initialize a new BLIPBuffer.
    BLIPBuffer() { flush(); }

    /// @brief Return the current sample rate.
    ///
    /// @returns the audio sample rate
    ///
    inline uint32_t get_sample_rate() const { return sample_rate; }

    /// @brief Return the number of source time units per second.
    ///
    /// @returns the number of source time units per second
    ///
    inline uint32_t get_clock_rate() const { return clock_rate; }

    /// @brief Return the current factor from the sample rate and clock rate.
    ///
    /// @returns the current factor
    ///
    inline uint32_t get_factor() const { return factor; }

    /// @brief Return the frequency of the  high-pass filter.
    ///
    /// @returns the cut-off frequency of the high-pass filter
    /// @details
    /// Higher values reduce the bass more.
    ///
    inline uint32_t get_bass_freq() const { return bass_freq; }

    /// @brief Return the TODO.
    ///
    /// @returns TODO
    ///
    inline uint32_t get_bass_shift() const { return bass_shift; }

    /// @brief Return the sample accumulator.
    ///
    /// @returns the sample accumulator
    ///
    inline int32_t get_accumulator() const { return accumulator; }

    /// @brief Return a pointer to the underlying buffer.
    ///
    /// @returns a pointer to the underlying buffer of samples
    ///
    inline int32_t* get_buffer() { return buffer; }

    /// @brief Flush the current contents of the buffer and accumulator.
    void flush() { accumulator = 0; memset(buffer, 0, sizeof(buffer)); }

    /// @brief Set the output sample rate and clock rate.
    ///
    /// @param sample_rate_ the number of samples per second
    /// @param clock_rate_ the number of source clock cycles per second
    ///
    void set_sample_rate(const uint32_t& sample_rate_, const uint32_t& clock_rate_) {
        if (!(sample_rate_ > 0))  // sample rate must be positive
            throw Exception("sample_rate must be greater than 0.");
        if (!(clock_rate_ > 0))  // clock rate must be positive
            throw Exception("clock_rate must be greater than 0.");
        // Calculate the number of clock cycles per sample, quantize by
        // truncation, and re-calculate the clock rate with rounding error
        // accounted for.
        auto quantized_clock_rate = sample_rate_ * (clock_rate_ / sample_rate_);
        // calculate the time factor based on the clock_rate and sample_rate
        float ratio = static_cast<float>(sample_rate_) / quantized_clock_rate;
        int32_t factor_ = floor(ratio * (1L << BLIP_BUFFER_ACCURACY) + 0.5f);
        if (!(factor_ > 0))  // factor must be positive
            throw Exception("sample_rate : clock_rate ratio is too large.");
        // update the instance variables atomically after error handling
        sample_rate = sample_rate_;
        clock_rate = quantized_clock_rate;
        factor = factor_;
        // reset the bass frequency because sample_rate has changed. This
        // function is atomic and guaranteed to not raise an error.
        set_bass_freq(bass_freq);
        // clear the contents of the buffer / accumulator
        flush();
    }

    /// @brief Set the frequency of the high-pass filter
    /// @details
    /// Higher values reduce the bass more.
    ///
    /// @param frequency the cut-off frequency of the high-pass filter
    ///
    inline void set_bass_freq(const int32_t& frequency) {
        int32_t shift = 31;
        if (frequency > 0) {
            shift = 13;
            int32_t f = (frequency << 16) / sample_rate;
            while ((f >>= 1) && --shift) { }
        }
        bass_freq = frequency;
        bass_shift = shift;
    }

    /// @brief Return a scaled floating point output sample from the buffer.
    ///
    /// @returns the sample \f$\in [-1, 1]\f$
    /// @details
    /// The buffer is advanced by the read operation.
    ///
    inline float read_sample() {
        // get the sample from the accumulator (don't clip it though). cast
        // it as a float for later calculation.
        float sample = accumulator >> (BLIP_SAMPLE_BITS - 16);
        accumulator += *buffer - (accumulator >> (bass_shift));
        // copy remaining samples to beginning and clear old samples
        static constexpr auto count = 1;
        auto remain = count + BLIP_WIDEST_IMPULSE;
        memmove(buffer, buffer + count, remain * sizeof *buffer);
        memset(buffer + remain, 0, count * sizeof *buffer);
        // scale the sample by the scale factor and the binary code space for
        // the digital signal to produce a floating point value
        return sample / std::numeric_limits<int16_t>::max();
    }
};

/// @brief Low-pass equalization parameters and logic.
class BLIPEqualizer {
 private:
    /// the constant value for Pi
    static constexpr double pi = 3.1415926535897932384626433832795029;
    /// Logarithmic roll-off to treble dB at half sampling rate. Negative
    /// values reduce treble, small positive values (0 to 5.0) increase treble.
    double treble = 0;
    /// TODO:
    uint32_t rolloff_freq = 0;
    /// the sample rate the engine is running at
    uint32_t sample_rate = 0;
    /// TODO:
    uint32_t cutoff_freq = 0;

    /// Generate a sinc function.
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
            BLIP_RES * 2.25 / count + 0.85;
        double cutoff = rolloff_freq * oversample / half_rate;
        // generate a sinc
        gen_sinc(out, count, BLIP_RES * oversample, treble, cutoff);
        // apply (half of) hamming window
        double to_fraction = pi / (count - 1);
        for (uint32_t i = count; i--;)
            out[i] *= 0.54f - 0.46f * cos(i * to_fraction);
    }
};

/// the synthesis quality level. Start with BLIP_QUALITY_GOOD.
enum BLIPQuality {
    BLIP_QUALITY_MEDIUM  = 8,
    BLIP_QUALITY_GOOD = 12,
    BLIP_QUALITY_HIGH = 16
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
    double volume_unit = 0;
    /// TODO:
    int16_t impulses[BLIP_RES * (quality / 2) + 1];
    /// TODO:
    int32_t kernel_unit = 0;
    /// the output buffer that the synthesizer writes samples to
    BLIPBuffer* buffer = 0;
    /// the last amplitude value (DPCM sample) to output from the synthesizer
    int last_amp = 0;
    /// the influence of amplitude deltas based on the volume unit
    int delta_factor = 0;

    /// TODO:
    inline int impulses_size() const { return BLIP_RES / 2 * quality + 1; }

    /// TODO:
    void adjust_impulse() {
        // sum pairs for each phase and add error correction to end of first half
        int const size = impulses_size();
        for (int p = BLIP_RES; p-- >= BLIP_RES / 2;) {
            int p2 = BLIP_RES - 2 - p;
            long error = kernel_unit;
            for (int i = 1; i < size; i += BLIP_RES) {
                error -= impulses[i + p ];
                error -= impulses[i + p2];
            }
            if (p == p2)  // phase = 0.5 impulse uses same half for both sides
                error /= 2;
            impulses[size - BLIP_RES + p] += (int16_t) error;
        }
    }

 public:
    /// Initialize a new BLIP synthesizer.
    BLIPSynthesizer() { memset(impulses, 0, sizeof impulses); }

    /// Set the volume to a new value.
    ///
    /// @param new_unit the new volume level to use
    ///
    void set_volume(double new_unit) {
        new_unit = new_unit * (1.0 / (range < 0 ? -range : range));
        if (new_unit != volume_unit) {
            // use default eq if it hasn't been set yet
            if (!kernel_unit)
                set_treble_eq(BLIPEqualizer(-8.0));

            volume_unit = new_unit;
            double factor = new_unit * (1L << BLIP_SAMPLE_BITS) / kernel_unit;

            if (factor > 0.0) {
                int shift = 0;

                // if unit is really small, might need to attenuate kernel
                while (factor < 2.0) {
                    shift++;
                    factor *= 2.0;
                }

                if (shift) {
                    kernel_unit >>= shift;
                    if (kernel_unit <= 0)
                        throw Exception("volume level is too low");
                    // keep values positive to avoid round-towards-zero of sign-preserving
                    // right shift for negative values
                    long offset = 0x8000 + (1 << (shift - 1));
                    long offset2 = 0x8000 >> shift;
                    for (int i = impulses_size(); i--;)
                        impulses[i] = (int16_t) (((impulses[i] + offset) >> shift) - offset2);
                    adjust_impulse();
                }
            }
            delta_factor = (int) floor(factor + 0.5);
        }
    }

    /// @brief Set treble equalization for the synthesizer.
    ///
    /// @param equalizer the equalization parameter for the synthesizer
    ///
    void set_treble_eq(BLIPEqualizer const& equalizer) {
        float fimpulse[BLIP_RES / 2 * (BLIP_WIDEST_IMPULSE - 1) + BLIP_RES * 2];

        int const half_size = BLIP_RES / 2 * (quality - 1);
        equalizer._generate(&fimpulse[BLIP_RES], half_size);

        int i;

        // need mirror slightly past center for calculation
        for (i = BLIP_RES; i--;)
            fimpulse[BLIP_RES + half_size + i] = fimpulse[BLIP_RES + half_size - 1 - i];

        // starts at 0
        for (i = 0; i < BLIP_RES; i++)
            fimpulse[i] = 0.0f;

        // find rescale factor
        double total = 0.0;
        for (i = 0; i < half_size; i++)
            total += fimpulse[BLIP_RES + i];

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
            impulses[i] = (int16_t) floor((next - sum) * rescale + 0.5);
            sum += fimpulse[i];
            next += fimpulse[i + BLIP_RES];
        }
        adjust_impulse();

        // volume might require rescaling
        double vol = volume_unit;
        if (vol) {
            volume_unit = 0.0;
            set_volume(vol);
        }
    }

    /// Set the buffer used for output.
    ///
    /// @param buffer_ the buffer that this synthesizer will write samples to
    ///
    inline void set_output(BLIPBuffer* buffer_) {
        buffer = buffer_;
        last_amp = 0;
    }

    /// Get the buffer used for output.
    ///
    /// @returns the buffer that this synthesizer is writing samples to
    ///
    inline BLIPBuffer* get_output() const { return buffer; }

    /// Update amplitude of waveform at given time. Using this requires a
    /// separate BLIPSynthesizer for each waveform.
    ///
    /// @param time the time of the sample
    /// @param amplitude the amplitude of the waveform to synthesizer
    ///
    inline void update(int32_t time, int amplitude) {
        int delta = amplitude - last_amp;
        last_amp = amplitude;
        offset_resampled(buffer->get_factor() * time, delta, buffer);
    }

    /// @brief Add an amplitude transition of specified delta into specified
    /// buffer rather than the instance buffer.
    ///
    /// @param time TODO:
    /// @param delta the change in amplitude. can be positive or negative.
    /// The actual change in amplitude is delta * (volume / range)
    /// @param buffer the buffer to write the data into
    ///
    inline void offset(int32_t time, int delta, BLIPBuffer* buffer) const {
        offset_resampled(buffer->get_factor() * time, delta, buffer);
    }

    /// @brief Add an amplitude transition of specified delta.
    ///
    /// @param time TODO:
    /// @param delta the change in amplitude. can be positive or negative.
    /// The actual change in amplitude is delta * (volume / range)
    ///
    inline void offset(int32_t time, int delta) const {
        offset(time, delta, buffer);
    }

    /// @brief TODO:
    ///
    /// @param time TODO:
    /// @param delta the change in amplitude. can be positive or negative.
    /// The actual change in amplitude is delta * (volume / range)
    /// @param blip_buffer the buffer to write the data into
    /// @details
    /// Works directly in terms of fractional output samples.
    /// Contact Shay Green for more info.
    ///
    void offset_resampled(uint32_t time, int delta, BLIPBuffer* blip_buffer) const {
        // TODO: remove. the "1" used to be a call to blip_buffer->get_size()
        // that is now static. This likely no longer needs to be checked
        if (!((time >> BLIP_BUFFER_ACCURACY) < 1))
            throw Exception("time goes beyond end of buffer");
        delta *= delta_factor;
        int32_t* BLIP_RESTRICT buffer = blip_buffer->get_buffer() + (time >> BLIP_BUFFER_ACCURACY);
        int phase = (int) (time >> (BLIP_BUFFER_ACCURACY - BLIP_PHASE_BITS) & (BLIP_RES - 1));

        int const fwd = (BLIP_WIDEST_IMPULSE - quality) / 2;
        int const rev = fwd + quality - 2;
        int const mid = quality / 2 - 1;

        int16_t const* BLIP_RESTRICT imp = impulses + BLIP_RES - phase;

        #if defined (_M_IX86)    || \
            defined (_M_IA64)    || \
            defined (__i486__)   || \
            defined (__x86_64__) || \
            defined (__ia64__)   || \
            defined (__i386__)  // CISC

        // straight forward implementation resulted in better code on GCC for x86

        #define ADD_IMP(out, in) \
            buffer[out] += (int32_t) imp[BLIP_RES * (in)] * delta

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
            int32_t t0 =                       i0 * delta + buffer[fwd     + i];\
            int32_t t1 = imp[BLIP_RES * (i + 1)] * delta + buffer[fwd + 1 + i];\
            i0 =           imp[BLIP_RES * (i + 2)];\
            buffer[fwd     + i] = t0;\
            buffer[fwd + 1 + i] = t1;\
        }
        #define BLIP_REV(r) {\
            int32_t t0 =                 i0 * delta + buffer[rev     - r];\
            int32_t t1 = imp[BLIP_RES * r] * delta + buffer[rev + 1 - r];\
            i0 =           imp[BLIP_RES * (r - 1)];\
            buffer[rev     - r] = t0;\
            buffer[rev + 1 - r] = t1;\
        }

            int32_t i0 = *imp;
            BLIP_FWD(0)
            if (quality > 8 ) BLIP_FWD(2)
            if (quality > 12) BLIP_FWD(4) {
                int32_t t0 =                   i0 * delta + buffer[fwd + mid - 1];
                int32_t t1 = imp[BLIP_RES * mid] * delta + buffer[fwd + mid    ];
                imp = impulses + phase;
                i0 = imp[BLIP_RES * mid];
                buffer[fwd + mid - 1] = t0;
                buffer[fwd + mid    ] = t1;
            }
            if (quality > 12) BLIP_REV(6)
            if (quality > 8 ) BLIP_REV(4)
            BLIP_REV(2)

            int32_t t0 =   i0 * delta + buffer[rev    ];
            int32_t t1 = *imp * delta + buffer[rev + 1];
            buffer[rev    ] = t0;
            buffer[rev + 1] = t1;
        #endif  // CISC

        #undef BLIP_FWD
        #undef BLIP_REV
    }
};

#undef BLIP_RESTRICT

#endif  // DSP_BLIP_BUFFER_HPP_
